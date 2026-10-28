# SPDX-License-Identifier: GPL-3.0-or-later
from uuid import uuid4
from datetime import datetime, timezone
from ..accounts.provider import AuthError
from ..events import EventSpec, publish
from .policy import mastery
from . import prizes


def validate_purchase_event(value):
    if set(value) != {'purchase_id', 'player_id', 'offer_id'} or not all(isinstance(v, str) for v in value.values()):
        raise ValueError('invalid_purchase_event')


PURCHASED = EventSpec('inventory.purchase_completed', 'inventory', validate_purchase_event)


class Shop:
    def __init__(self, progression):
        self.progression = progression
        self.db = progression.db

    def catalogue(self):
        with self.db.transaction() as conn:
            return {'catalogue': conn.execute("SELECT * FROM trainer_backend.shop_offers WHERE available AND purpose='profile_scene' ORDER BY id LIMIT 100").fetchall(),
                    'outfit_catalogue': conn.execute("SELECT * FROM trainer_backend.shop_offers WHERE available AND purpose='outfit_slot' ORDER BY slot_number LIMIT 64").fetchall()}

    def snapshot(self, access, operation=None):
        with self.db.transaction() as conn:
            actor = self.progression.account(conn, access)
            prizes.levels(conn, actor.player_id)
            account = conn.execute('SELECT revision FROM trainer_backend.progression_accounts WHERE player_id=%s FOR SHARE', (actor.player_id,)).fetchone()
            items = conn.execute('''SELECT s.*,p.id AS purchase_id,p.created_at AS acquired_at FROM trainer_backend.owned_items i
                JOIN trainer_backend.shop_offers s ON s.id=i.offer_id JOIN trainer_backend.purchases p ON p.id=i.purchase_id
                WHERE i.player_id=%s ORDER BY p.created_at,s.id LIMIT 100''', (actor.player_id,)).fetchall()
            appearance = conn.execute('SELECT offer_id FROM trainer_backend.profile_appearances WHERE player_id=%s', (actor.player_id,)).fetchone()
            receipt = None
            if operation:
                receipt = conn.execute('SELECT * FROM trainer_backend.purchases WHERE player_id=%s AND operation_id=%s', (actor.player_id, operation)).fetchone()
                if not receipt:
                    raise AuthError('purchase_not_found', 404)
            return {'player_id': actor.player_id, 'revision': account['revision'], 'checked_at': datetime.now(timezone.utc),
                    'catalogue': conn.execute("SELECT * FROM trainer_backend.shop_offers WHERE available AND purpose='profile_scene' ORDER BY id LIMIT 100").fetchall(),
                    'items': [i for i in items if i['purpose'] == 'profile_scene'],
                    'outfit_catalogue': conn.execute("SELECT * FROM trainer_backend.shop_offers WHERE available AND purpose='outfit_slot' ORDER BY slot_number LIMIT 64").fetchall(),
                    'outfit_items': [i for i in items if i['purpose'] == 'outfit_slot'] + conn.execute('''SELECT
                        s.slot_id id,p.id grant_id,p.title,p.created_at acquired_at,1 revision,0 price,
                        'outfit_slot' purpose,'permanent_license' kind,64+p.level slot_number,p.level
                        FROM trainer_backend.outfit_slots s JOIN trainer_backend.prize_grants p ON p.id=s.grant_id
                        WHERE s.player_id=%s ORDER BY p.level''', (actor.player_id,)).fetchall(),
                    'outfits': conn.execute('SELECT * FROM trainer_backend.outfits WHERE player_id=%s ORDER BY offer_id', (actor.player_id,)).fetchall(),
                    'equipped': appearance['offer_id'] if appearance else None, 'receipt': receipt}

    def purchase(self, access, operation, offer_id, revision, price):
        with self.db.transaction() as conn:
            actor = self.progression.account(conn, access)
            account = conn.execute('SELECT * FROM trainer_backend.progression_accounts WHERE player_id=%s FOR UPDATE', (actor.player_id,)).fetchone()
            prior = conn.execute('SELECT * FROM trainer_backend.purchases WHERE player_id=%s AND operation_id=%s', (actor.player_id, operation)).fetchone()
            if prior:
                if (prior['offer_id'], prior['offer_revision'], prior['price']) != (offer_id, revision, price):
                    raise AuthError('purchase_identity_conflict', 409)
            else:
                offer = conn.execute('SELECT * FROM trainer_backend.lock_shop_offer(%s)', (offer_id,)).fetchone()
                if not offer or not offer['available']:
                    raise AuthError('offer_unavailable', 409)
                if (offer['revision'], offer['price']) != (revision, price):
                    raise AuthError('offer_changed', 409)
                if conn.execute('SELECT 1 FROM trainer_backend.owned_items WHERE player_id=%s AND offer_id=%s', (actor.player_id, offer_id)).fetchone():
                    raise AuthError('already_owned', 409)
                if mastery(account['xp'])['level'] < offer['minimum_level']:
                    raise AuthError('level_required', 409)
                if account['balance'] < offer['price']:
                    raise AuthError('insufficient_sparks', 409)
                purchase = uuid4()
                conn.execute('''INSERT INTO trainer_backend.purchases(id,player_id,operation_id,offer_id,offer_revision,price,title)
                    VALUES(%s,%s,%s,%s,%s,%s,%s)''', (purchase, actor.player_id, operation, offer_id, revision, offer['price'], offer['title']))
                conn.execute('INSERT INTO trainer_backend.owned_items(player_id,offer_id,purchase_id) VALUES(%s,%s,%s)', (actor.player_id, offer_id, purchase))
                if offer['purpose'] == 'outfit_slot':
                    conn.execute('INSERT INTO trainer_backend.outfit_slots(player_id,slot_id,purchase_id) VALUES(%s,%s,%s)', (actor.player_id, offer_id, purchase))
                conn.execute('''INSERT INTO trainer_backend.reward_ledger(id,player_id,source,event_id,title,xp,sparks,base_sparks,policy,earned_at,purchase_id)
                    VALUES(%s,%s,'shop.purchase',%s,%s,0,%s,0,'shop-v1',clock_timestamp(),%s)''',
                    (uuid4(), actor.player_id, str(purchase), offer['title'], -offer['price'], purchase))
                row = conn.execute('UPDATE trainer_backend.progression_accounts SET balance=balance-%s,revision=revision+1 WHERE player_id=%s RETURNING revision', (offer['price'], actor.player_id)).fetchone()
                conn.execute('INSERT INTO trainer_backend.audit_events(id,actor_id,action,resource_id) VALUES(%s,%s,%s,%s)', (uuid4(), actor.player_id, 'shop.purchase', purchase))
                publish(conn, PURCHASED, purchase, actor.player_id, row['revision'],
                        {'purchase_id': str(purchase), 'player_id': str(actor.player_id), 'offer_id': offer_id})
        return self.snapshot(access, operation)

    def equip(self, access, offer_id, expected_revision):
        with self.db.transaction() as conn:
            actor = self.progression.account(conn, access)
            row = conn.execute('SELECT revision FROM trainer_backend.progression_accounts WHERE player_id=%s FOR UPDATE', (actor.player_id,)).fetchone()
            if row['revision'] != expected_revision:
                raise AuthError('inventory_changed', 409)
            if offer_id is not None and not conn.execute('SELECT 1 FROM trainer_backend.owned_items WHERE player_id=%s AND offer_id=%s', (actor.player_id, offer_id)).fetchone():
                raise AuthError('item_not_owned', 409)
            if offer_id is not None and not conn.execute("SELECT 1 FROM trainer_backend.shop_offers WHERE id=%s AND purpose='profile_scene'", (offer_id,)).fetchone():
                raise AuthError('invalid_item_purpose', 409)
            conn.execute('''INSERT INTO trainer_backend.profile_appearances(player_id,offer_id) VALUES(%s,%s)
                ON CONFLICT(player_id) DO UPDATE SET offer_id=EXCLUDED.offer_id''', (actor.player_id, offer_id))
            conn.execute('UPDATE trainer_backend.progression_accounts SET revision=revision+1 WHERE player_id=%s', (actor.player_id,))
        return self.snapshot(access)
