# SPDX-License-Identifier: GPL-3.0-or-later
"""Saved outfits are mutable contents of permanent, purchased slots."""
import unicodedata
from uuid import uuid4
from psycopg.types.json import Jsonb
from ..accounts.provider import AuthError
from ..accounts.profiles import avatar_recipe


def save(shop, access, offer_id, name, recipe, expected_revision):
    if not isinstance(name, str):
        raise AuthError('invalid_outfit_name', 422)
    name = unicodedata.normalize('NFC', name).strip()
    if not 1 <= len(name) <= 32 or any(unicodedata.category(c).startswith('C') or c in '\u2028\u2029' for c in name):
        raise AuthError('invalid_outfit_name', 422)
    if recipe is None:
        raise AuthError('invalid_avatar_recipe', 422)
    avatar_recipe(recipe)
    with shop.db.transaction() as conn:
        actor = shop.progression.account(conn, access)
        player = actor.player_id
        conn.execute('SELECT revision FROM trainer_backend.progression_accounts WHERE player_id=%s FOR UPDATE', (player,))
        owned = conn.execute('''SELECT 1 FROM trainer_backend.owned_items i
            JOIN trainer_backend.shop_offers s ON s.id=i.offer_id
            WHERE i.player_id=%s AND i.offer_id=%s AND s.purpose='outfit_slot' ''', (player, offer_id)).fetchone()
        if not owned:
            raise AuthError('outfit_slot_not_owned', 403)
        prior = conn.execute('SELECT * FROM trainer_backend.outfits WHERE player_id=%s AND offer_id=%s', (player, offer_id)).fetchone()
        # A retry after a lost response cannot advance a revision or overwrite a
        # different edit from another handheld. Identical contents are a no-op.
        if prior and prior['name'] == name and prior['recipe'] == recipe:
            pass
        elif (prior['revision'] if prior else 0) != expected_revision:
            raise AuthError('outfit_changed', 409)
        else:
            conn.execute('''INSERT INTO trainer_backend.outfits(player_id,offer_id,name,recipe) VALUES(%s,%s,%s,%s)
                ON CONFLICT(player_id,offer_id) DO UPDATE SET name=EXCLUDED.name,recipe=EXCLUDED.recipe,
                    revision=outfits.revision+1,updated_at=clock_timestamp()''', (player, offer_id, name, Jsonb(recipe)))
            conn.execute('UPDATE trainer_backend.progression_accounts SET revision=revision+1 WHERE player_id=%s', (player,))
            conn.execute('INSERT INTO trainer_backend.audit_events(id,actor_id,action,resource_id) VALUES(%s,%s,%s,%s)',
                         (uuid4(), player, 'outfit.saved', player))
    return shop.snapshot(access)
