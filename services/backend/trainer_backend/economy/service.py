# SPDX-License-Identifier: GPL-3.0-or-later
import secrets
from datetime import datetime, timezone
from uuid import uuid4
from psycopg.errors import UniqueViolation
from ..accounts.provider import AuthError
from .policy import VERSION, achievement, completion, mastery
from .provider import RetroAchievements


class Progression:
    def __init__(self, db, accounts, provider=None):
        self.db, self.accounts, self.provider = db, accounts, provider or RetroAchievements()

    def account(self, conn, access):
        actor = self.accounts.authenticate(conn, access)
        conn.execute('INSERT INTO trainer_backend.progression_accounts(player_id) VALUES(%s) ON CONFLICT DO NOTHING', (actor.player_id,))
        return actor

    def snapshot(self, access, before=None):
        with self.db.transaction() as conn:
            actor = self.account(conn, access)
            row = conn.execute('SELECT * FROM trainer_backend.progression_accounts WHERE player_id=%s FOR SHARE', (actor.player_id,)).fetchone()
            link = conn.execute('SELECT username,linked_at FROM trainer_backend.reward_links WHERE player_id=%s', (actor.player_id,)).fetchone()
            challenge = conn.execute('SELECT username,challenge,expires_at FROM trainer_backend.reward_link_challenges WHERE player_id=%s AND expires_at>clock_timestamp()', (actor.player_id,)).fetchone()
            history = conn.execute('''SELECT id,source,title,xp,sparks,policy,earned_at,created_at FROM trainer_backend.reward_ledger
                WHERE player_id=%s AND (%s::timestamptz IS NULL OR created_at<%s)
                ORDER BY created_at DESC,id DESC LIMIT 50''', (actor.player_id, before, before)).fetchall()
            return {'player_id': actor.player_id, 'mastery': mastery(row['xp']), 'balance': row['balance'],
                    'revision': row['revision'], 'link': link, 'challenge': challenge, 'history': history,
                    'support_preview': None, 'checked_at': datetime.now(timezone.utc)}

    def challenge(self, access, username, key):
        self.reserve_check(access)
        profile = self.provider.profile(key, username)
        with self.db.transaction() as conn:
            actor = self.account(conn, access)
            if conn.execute('SELECT 1 FROM trainer_backend.reward_links WHERE player_id=%s OR ra_id=%s', (actor.player_id, profile['id'])).fetchone():
                raise AuthError('ra_already_linked', 409)
            code = 'TrainerOS-' + secrets.token_urlsafe(18)
            conn.execute('''INSERT INTO trainer_backend.reward_link_challenges(player_id,ra_id,username,challenge,expires_at)
                VALUES(%s,%s,%s,%s,clock_timestamp()+interval '30 minutes') ON CONFLICT(player_id) DO UPDATE
                SET ra_id=EXCLUDED.ra_id,username=EXCLUDED.username,challenge=EXCLUDED.challenge,expires_at=EXCLUDED.expires_at''',
                (actor.player_id, profile['id'], profile['name'], code))
        return self.snapshot(access)

    def verify(self, access, key):
        self.reserve_check(access)
        with self.db.transaction() as conn:
            actor = self.account(conn, access)
            pending = conn.execute('SELECT * FROM trainer_backend.reward_link_challenges WHERE player_id=%s AND expires_at>clock_timestamp()', (actor.player_id,)).fetchone()
        if not pending:
            raise AuthError('ra_challenge_expired', 409)
        profile = self.provider.profile(key, pending['ra_id'])
        if profile['id'] != pending['ra_id'] or not isinstance(profile['motto'], str) or pending['challenge'] not in profile['motto'].split():
            raise AuthError('ra_challenge_missing', 409)
        try:
            with self.db.transaction() as conn:
                actor = self.account(conn, access)
                current = conn.execute('SELECT * FROM trainer_backend.reward_link_challenges WHERE player_id=%s AND expires_at>clock_timestamp() FOR UPDATE', (actor.player_id,)).fetchone()
                if not current or current['challenge'] != pending['challenge']:
                    raise AuthError('ra_challenge_expired', 409)
                conn.execute('INSERT INTO trainer_backend.reward_links(player_id,ra_id,username) VALUES(%s,%s,%s)', (actor.player_id, profile['id'], profile['name']))
                conn.execute('UPDATE trainer_backend.reward_link_challenges SET expires_at=clock_timestamp() WHERE player_id=%s', (actor.player_id,))
        except UniqueViolation:
            raise AuthError('ra_already_linked', 409) from None
        return self.snapshot(access)

    def reserve_check(self, access):
        with self.db.transaction() as conn:
            actor = self.account(conn, access)
            row = conn.execute('''UPDATE trainer_backend.progression_accounts SET next_check_at=clock_timestamp()+interval '15 seconds'
                WHERE player_id=%s AND (next_check_at IS NULL OR next_check_at<=clock_timestamp()) RETURNING player_id''', (actor.player_id,)).fetchone()
            if not row:
                raise AuthError('progression_rate_limited', 429)

    def sync(self, access, key, games):
        self.reserve_check(access)
        with self.db.transaction() as conn:
            actor = self.account(conn, access)
            link = conn.execute('SELECT * FROM trainer_backend.reward_links WHERE player_id=%s', (actor.player_id,)).fetchone()
        if not link:
            raise AuthError('ra_not_linked', 409)
        profile = self.provider.profile(key, link['ra_id'])
        if profile['id'] != link['ra_id'] or not profile['eligible']:
            raise AuthError('ra_account_ineligible', 409)
        # Network work never holds a database lock. Caller supplies only locators,
        # not awards, points, dates, amounts or proof. All game data comes from RA.
        verified = [self.provider.game(key, link['ra_id'], game) for game in sorted(set(games))]
        now = datetime.now(timezone.utc)
        with self.db.transaction() as conn:
            actor = self.account(conn, access)  # logout/revocation during provider I/O
            conn.execute('SELECT player_id FROM trainer_backend.progression_accounts WHERE player_id=%s FOR UPDATE', (actor.player_id,))
            for game in verified:
                rows = game['achievements']
                for row in rows:
                    if row['earned'] is not None and link['linked_at'] <= row['earned'] <= now:
                        self.grant(conn, actor.player_id, link['ra_id'], game['id'], 'ra.achievement', row['id'], row['title'], row['earned'], achievement(row['points']))
                if rows and all(r['earned'] is not None and r['earned'] <= now for r in rows):
                    finished = max(r['earned'] for r in rows)
                    if finished >= link['linked_at']:
                        self.grant(conn, actor.player_id, link['ra_id'], game['id'], 'ra.completion', str(game['id']), game['title'], finished, completion(sum(r['points'] for r in rows)))
        return self.snapshot(access)

    @staticmethod
    def grant(conn, player, ra, game, source, event, title, earned, amounts):
        xp, sparks = amounts
        inserted = conn.execute('''INSERT INTO trainer_backend.reward_ledger
            (id,player_id,source,ra_id,game_id,event_id,title,xp,sparks,base_sparks,policy,earned_at)
            VALUES(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s) ON CONFLICT(ra_id,source,event_id) DO NOTHING RETURNING id''',
            (uuid4(), player, source, ra, game, event, title, xp, sparks, sparks, VERSION, earned)).fetchone()
        if inserted:
            conn.execute('UPDATE trainer_backend.progression_accounts SET xp=xp+%s,balance=balance+%s,revision=revision+1 WHERE player_id=%s', (xp, sparks, player))
            conn.execute('INSERT INTO trainer_backend.audit_events(id,actor_id,action,resource_id) VALUES(%s,%s,%s,%s)',
                         (uuid4(), player, 'rewards.verified_ra', inserted['id']))
