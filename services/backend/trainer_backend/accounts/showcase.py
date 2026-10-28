# SPDX-License-Identifier: GPL-3.0-or-later
"""Authenticated public projection. Never return private economy/wardrobe records."""
from uuid import UUID
import unicodedata
from psycopg.types.json import Jsonb
from .provider import AuthError
from .profiles import public_profiles
from ..economy.policy import mastery

FIELDS = ('level', 'shelf', 'history', 'activity', 'favorites')
DEFAULTS = dict(level=True, shelf=True, history=False, activity=False, favorites=False)


class Showcase:
    def __init__(self, accounts):
        self.accounts, self.db = accounts, accounts.db

    def sharing(self, access, update=None, revision=None):
        with self.db.transaction() as conn:
            player = self.accounts.authenticate(conn, access).player_id
            conn.execute('INSERT INTO trainer_backend.profile_sharing(player_id) VALUES(%s) ON CONFLICT DO NOTHING', (player,))
            row = conn.execute('SELECT * FROM trainer_backend.profile_sharing WHERE player_id=%s FOR UPDATE', (player,)).fetchone()
            if update is not None:
                if set(update) != set(FIELDS) or any(type(v) is not bool for v in update.values()):
                    raise AuthError('invalid_sharing', 422)
                if revision != row['revision']:
                    raise AuthError('sharing_changed', 409)
                row = conn.execute('''UPDATE trainer_backend.profile_sharing SET level=%s,shelf=%s,history=%s,activity=%s,favorites=%s,
                    revision=revision+1 WHERE player_id=%s RETURNING *''', tuple(update[k] for k in FIELDS)+(player,)).fetchone()
            return {k: row[k] for k in ('revision', *FIELDS)}

    def favorites(self, access, games, revision):
        # Presentation selected deliberately by the owner, never proof of ownership/play.
        if not isinstance(games,list) or len(games)>5:
            raise AuthError('invalid_favorites',422)
        titles=[]
        for value in games:
            if not isinstance(value,str) or not 1<=len(value)<=128 or any(unicodedata.category(c).startswith('C') for c in value):
                raise AuthError('invalid_favorites',422)
            title=unicodedata.normalize('NFC',' '.join(value.split()))
            if not title or title in titles:raise AuthError('invalid_favorites',422)
            titles.append(title)
        with self.db.transaction() as conn:
            player=self.accounts.authenticate(conn,access).player_id
            row=conn.execute('''UPDATE trainer_backend.profile_sharing SET favorite_games=%s,favorites=true,revision=revision+1
                WHERE player_id=%s AND revision=%s RETURNING revision''',(Jsonb(titles),player,revision)).fetchone()
            if not row:raise AuthError('sharing_changed',409)
        return self.sharing(access)

    def read(self, access, *, player=None, subject=None, before=None):
        with self.db.transaction() as conn:
            self.accounts.authenticate(conn, access)
            if subject is not None:
                identity = conn.execute('''SELECT player_id FROM trainer_backend.external_identities
                    WHERE instance=%s AND subject=%s''', (self.accounts.provider.instance, subject)).fetchone()
                player = identity['player_id'] if identity else None
            if player is None or not conn.execute("SELECT 1 FROM trainer_backend.players u JOIN trainer_backend.principals p ON p.id=u.id WHERE p.id=%s AND p.state='active'", (player,)).fetchone():
                raise AuthError('profile_not_found', 404)
            player = UUID(str(player))
            profile = public_profiles(conn, [player])[str(player)]
            sharing = conn.execute('SELECT * FROM trainer_backend.profile_sharing WHERE player_id=%s', (player,)).fetchone() or DEFAULTS
            # Self-preview uses this exact projection too; there is no owner bypass.
            result = {k: profile[k] for k in ('player_id', 'display_name', 'avatar')}
            result['visibility'] = {k: sharing[k] for k in FIELDS}
            if sharing['favorites']:result['favorites']=sharing.get('favorite_games',[])
            appearance = conn.execute('''SELECT o.appearance FROM trainer_backend.profile_appearances p
                JOIN trainer_backend.shop_offers o ON o.id=p.offer_id WHERE p.player_id=%s''', (player,)).fetchone()
            result['appearance'] = appearance['appearance'] if appearance else None
            identity = conn.execute('SELECT subject FROM trainer_backend.external_identities WHERE player_id=%s AND instance=%s',
                                    (player, self.accounts.provider.instance)).fetchone()
            result['subject'] = identity['subject'] if identity else None
            if sharing['level']:
                account = conn.execute('SELECT xp FROM trainer_backend.progression_accounts WHERE player_id=%s', (player,)).fetchone()
                result['level'] = mastery(account['xp'] if account else 0)['level']
            if sharing['shelf']:
                result['shelf'] = conn.execute('''SELECT s.position,s.game_id,min(p.title) title,min(p.earned_at) earned_at,
                    bool_or(p.kind='completed') completed,bool_or(p.kind='hardcore') hardcore
                    FROM trainer_backend.prize_shelf s JOIN trainer_backend.prize_grants p
                    ON p.player_id=s.player_id AND p.game_id=s.game_id AND p.kind IN ('beaten','completed','hardcore')
                    WHERE s.player_id=%s GROUP BY s.position,s.game_id ORDER BY s.position''', (player,)).fetchall()
            if sharing['history']:
                result['achievements']=conn.execute('''SELECT title,game_id,earned_at FROM trainer_backend.reward_ledger
                    WHERE player_id=%s AND source='ra.achievement' ORDER BY earned_at DESC,id DESC LIMIT 3''',(player,)).fetchall()
                rows = conn.execute('''WITH trophies AS (
                    SELECT game_id,min(title) title,min(earned_at) earned_at,max(sequence) sequence,
                        bool_or(kind='completed') completed,bool_or(kind='hardcore') hardcore
                    FROM trainer_backend.prize_grants WHERE player_id=%s AND kind IN ('beaten','completed','hardcore') GROUP BY game_id)
                    SELECT * FROM trophies WHERE (%s::bigint IS NULL OR sequence<%s) ORDER BY sequence DESC LIMIT 25''',
                    (player, before, before)).fetchall()
                result['next'] = str(rows[23]['sequence']) if len(rows)>24 else None
                result['history'] = [{k:v for k,v in row.items() if k!='sequence'} for row in rows[:24]]
            return result
