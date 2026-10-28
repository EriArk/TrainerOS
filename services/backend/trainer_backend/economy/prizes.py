# SPDX-License-Identifier: GPL-3.0-or-later
"""Server-owned permanent awards. Cosmetic grants never alter XP or Sparks."""
from datetime import datetime, timezone
from uuid import uuid4
from ..accounts.provider import AuthError
from .policy import mastery


def grant(conn, player, event, kind, title, earned, *, level=None, game=None, item=None):
    row = conn.execute('''INSERT INTO trainer_backend.prize_grants
        (id,player_id,event_key,kind,title,earned_at,level,game_id,item_id)
        VALUES(%s,%s,%s,%s,%s,%s,%s,%s,%s) ON CONFLICT(player_id,event_key) DO NOTHING RETURNING id''',
        (uuid4(), player, event, kind, title, earned, level, game, item)).fetchone()
    if row:
        conn.execute('UPDATE trainer_backend.progression_accounts SET revision=revision+1 WHERE player_id=%s', (player,))
        conn.execute('INSERT INTO trainer_backend.audit_events(id,actor_id,action,resource_id) VALUES(%s,%s,%s,%s)',
                     (uuid4(), player, 'prizes.granted', row['id']))
    return row


def levels(conn, player):
    # Same account lock as purchases/reward ledger; safe for first upgrade/login.
    row = conn.execute('SELECT xp FROM trainer_backend.progression_accounts WHERE player_id=%s FOR UPDATE', (player,)).fetchone()
    level = mastery(row['xp'])['level']
    previous = conn.execute("SELECT coalesce(max(level),1) n FROM trainer_backend.prize_grants WHERE player_id=%s AND kind='clothing' AND level IS NOT NULL", (player,)).fetchone()['n']
    now = datetime.now(timezone.utc)
    for n in range(previous + 1, level + 1):
        grant(conn, player, f'level:{n}:clothing', 'clothing', f'Level {n} wardrobe reward', now, level=n, item=f'level.{n}')
        if n % 3 == 0:
            award = grant(conn, player, f'level:{n}:slot', 'outfit_slot', f'Level {n} look slot', now, level=n)
            if award:
                conn.execute('INSERT INTO trainer_backend.outfit_slots(player_id,slot_id,grant_id) VALUES(%s,%s,%s)',
                             (player, f'outfit.level.{n}', award['id']))


def games(conn, player, ra, awards):
    now = datetime.now(timezone.utc)
    for award in awards:
        if award['earned'] > now:
            continue
        game, kind = award['game'], award['kind']
        grant(conn, player, f'ra:{ra}:{game}:{kind}', kind, award['title'], award['earned'], game=game)
    # Backfill newly assigned game rewards, including already imported completions.
    rows = conn.execute('''SELECT c.id,c.title,p.game_id,min(p.earned_at) earned
        FROM trainer_backend.prize_catalogue c JOIN trainer_backend.prize_grants p ON p.game_id=c.game_id
        WHERE p.player_id=%s AND p.kind='beaten' GROUP BY c.id,c.title,p.game_id''', (player,)).fetchall()
    for row in rows:
        grant(conn, player, f'game:{row["game_id"]}:clothing', 'clothing', row['title'], row['earned'],
              game=row['game_id'], item=row['id'])


class Prizes:
    def __init__(self, progression):
        self.progression, self.db = progression, progression.db

    def snapshot(self, access, before=None, group_before=None):
        with self.db.transaction() as conn:
            actor = self.progression.account(conn, access)
            player = actor.player_id
            levels(conn, player)
            account = conn.execute('SELECT xp,revision FROM trainer_backend.progression_accounts WHERE player_id=%s', (player,)).fetchone()
            # Page by the newest fact of a whole game/level, never through its middle.
            collection = conn.execute('''WITH grouped AS (
                SELECT coalesce('game:'||game_id,'level:'||level,'grant:'||id) key,
                    max(sequence) sequence, jsonb_agg(to_jsonb(p) ORDER BY sequence) grants
                FROM trainer_backend.prize_grants p WHERE player_id=%s GROUP BY 1)
                SELECT * FROM grouped WHERE (%s::bigint IS NULL OR sequence<%s)
                ORDER BY sequence DESC LIMIT 101''', (player, group_before, group_before)).fetchall()
            collection_before = str(collection[99]['sequence']) if len(collection) > 100 else None
            collection = collection[:100]
            rows = conn.execute('''SELECT p.*,r.presented_at FROM trainer_backend.prize_grants p
                LEFT JOIN trainer_backend.prize_receipts r ON r.player_id=p.player_id AND r.grant_id=p.id
                WHERE p.player_id=%s AND (%s::bigint IS NULL OR p.sequence<%s)
                ORDER BY p.sequence DESC LIMIT 100''', (player, before, before)).fetchall()
            pending = conn.execute('''SELECT count(*) n FROM trainer_backend.prize_grants p WHERE p.player_id=%s
                AND NOT EXISTS(SELECT 1 FROM trainer_backend.prize_receipts r WHERE r.player_id=p.player_id AND r.grant_id=p.id)''', (player,)).fetchone()['n']
            # Keep the celebration bounded. Acknowledgement names only displayed grants.
            waiting = conn.execute('''WITH pending AS (
                SELECT p.*,coalesce('game:'||game_id,'level:'||level,'grant:'||id) group_key
                FROM trainer_backend.prize_grants p WHERE p.player_id=%s
                AND NOT EXISTS(SELECT 1 FROM trainer_backend.prize_receipts r WHERE r.player_id=p.player_id AND r.grant_id=p.id)),
                groups AS (SELECT group_key,min(sequence) first_sequence,count(*) n FROM pending GROUP BY group_key),
                bounded AS (SELECT *,sum(n) OVER(ORDER BY first_sequence) total,
                    row_number() OVER(ORDER BY first_sequence) position FROM groups)
                SELECT p.* FROM pending p JOIN bounded b USING(group_key)
                WHERE b.total<=100 OR b.position=1 ORDER BY b.first_sequence,p.sequence LIMIT 100''', (player,)).fetchall()
            trophies = conn.execute('''SELECT game_id,min(title) title,min(earned_at) earned_at,
                bool_or(kind='beaten') beaten,bool_or(kind='completed') completed,bool_or(kind='hardcore') hardcore,
                max(sequence) sequence FROM trainer_backend.prize_grants
                WHERE player_id=%s AND game_id IS NOT NULL AND kind IN ('beaten','completed','hardcore')
                GROUP BY game_id ORDER BY sequence DESC LIMIT 100''', (player,)).fetchall()
            shelf = conn.execute('''SELECT s.position,s.game_id,min(p.title) title,min(p.earned_at) earned_at,
                bool_or(p.kind='beaten') beaten,bool_or(p.kind='completed') completed,bool_or(p.kind='hardcore') hardcore
                FROM trainer_backend.prize_shelf s JOIN trainer_backend.prize_grants p
                ON p.player_id=s.player_id AND p.game_id=s.game_id AND p.kind IN ('beaten','completed','hardcore')
                WHERE s.player_id=%s GROUP BY s.position,s.game_id ORDER BY s.position''', (player,)).fetchall()
            n = mastery(account['xp'])['level']
            return {'player_id': player, 'revision': account['revision'], 'items': rows, 'pending': pending,
                    'waiting': waiting, 'trophies': trophies, 'shelf': shelf,
                    'collection': collection, 'collection_before': collection_before,
                    'next_before': str(rows[-1]['sequence']) if len(rows) == 100 else None,
                    'upcoming': [{'level': i, 'clothing': True, 'slot': i % 3 == 0} for i in range(n + 1, n + 4)],
                    'checked_at': datetime.now(timezone.utc)}

    def acknowledge(self, access, ids):
        with self.db.transaction() as conn:
            actor = self.progression.account(conn, access)
            for identity in set(ids):
                conn.execute('''INSERT INTO trainer_backend.prize_receipts(player_id,grant_id)
                    SELECT player_id,id FROM trainer_backend.prize_grants WHERE player_id=%s AND id=%s
                    ON CONFLICT DO NOTHING''', (actor.player_id, identity))
        return self.snapshot(access)

    def shelf(self, access, games):
        if len(games) > 5 or len(set(games)) != len(games):
            raise AuthError('invalid_shelf', 422)
        with self.db.transaction() as conn:
            player = self.progression.account(conn, access).player_id
            conn.execute('SELECT revision FROM trainer_backend.progression_accounts WHERE player_id=%s FOR UPDATE', (player,))
            for game in games:
                if not conn.execute("SELECT 1 FROM trainer_backend.prize_grants WHERE player_id=%s AND game_id=%s AND kind IN ('beaten','completed')", (player, game)).fetchone():
                    raise AuthError('trophy_not_owned', 403)
            current = [r['game_id'] for r in conn.execute('SELECT game_id FROM trainer_backend.prize_shelf WHERE player_id=%s ORDER BY position', (player,)).fetchall()]
            if current != games:
                conn.execute('DELETE FROM trainer_backend.prize_shelf WHERE player_id=%s', (player,))
                for index, game in enumerate(games):
                    conn.execute('INSERT INTO trainer_backend.prize_shelf(player_id,position,game_id) VALUES(%s,%s,%s)', (player, index, game))
                conn.execute('UPDATE trainer_backend.progression_accounts SET revision=revision+1 WHERE player_id=%s', (player,))
        return self.snapshot(access)
