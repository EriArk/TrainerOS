# SPDX-License-Identifier: GPL-3.0-or-later
"""Trusted provider boundary and atomic support grants. No public writer is mounted.

Only a provider-specific server adapter
may return a bound, authenticated snapshot; a client body is never such evidence.
Revision must order authoritative state, not webhook arrival time.
"""
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import re
from uuid import UUID, uuid4
from psycopg.errors import UniqueViolation
from ..accounts.provider import AuthError
from ..commands import canonical
from ..events import EventSpec, publish

POLICY = 'support-grants-2026-10-v1'


@dataclass(frozen=True)
class Membership:
    player_id: UUID
    subject: str
    event_id: str
    revision: int
    multiplier: int
    observed_at: datetime
    valid_until: datetime
    evidence_hash: str


def active(conn, player):
    return conn.execute('''SELECT r.id,r.multiplier,r.valid_until FROM trainer_backend.support_memberships m
        JOIN trainer_backend.support_receipts r ON r.id=m.receipt_id
        WHERE m.player_id=%s AND r.multiplier>1 AND r.valid_until>clock_timestamp()
        ORDER BY r.multiplier DESC,r.valid_until DESC,r.id LIMIT 1''', (player,)).fetchone()


def status(conn, player):
    current = active(conn, player)
    return {'multiplier': current['multiplier'] if current else 1,
            'verified_until': current['valid_until'] if current else None}


def validate_grant_event(value):
    if set(value) != {'player_id', 'sparks', 'rewards'} or not isinstance(value['player_id'], str) or any(
            type(value[k]) is not int or value[k] <= 0 for k in ('sparks', 'rewards')):
        raise ValueError('invalid_support_grant_event')


GRANTED = EventSpec('support.topup_granted', 'support', validate_grant_event)


def grant_topups(conn, player, reward=None):
    """Caller holds the existing progression account lock, including new RA grants."""
    current = active(conn, player)
    if not current:
        return 0
    result = conn.execute('''WITH eligible AS (
        SELECT b.id,b.title,b.base_sparks,COALESCE(max(t.support_multiplier),1) AS highest
        FROM trainer_backend.reward_ledger b
        LEFT JOIN trainer_backend.reward_ledger t ON t.base_reward_id=b.id
        WHERE b.player_id=%s AND b.source IN ('ra.achievement','ra.completion')
            AND (%s::uuid IS NULL OR b.id=%s)
        GROUP BY b.id
    ), granted AS (
        INSERT INTO trainer_backend.reward_ledger
            (id,player_id,source,event_id,title,xp,sparks,base_sparks,policy,earned_at,base_reward_id,support_multiplier,support_receipt_id)
        SELECT gen_random_uuid(),%s,'support.topup',id::text||':'||%s::text,title,0,
            base_sparks*(%s-highest),0,%s,clock_timestamp(),id,%s,%s
        FROM eligible WHERE highest<%s AND base_sparks>0
        RETURNING sparks
    ) SELECT COALESCE(sum(sparks),0) AS sparks,count(*) AS rewards FROM granted''',
        (player, reward, reward, player, current['multiplier'], current['multiplier'], POLICY,
         current['multiplier'], current['id'], current['multiplier'])).fetchone()
    sparks = int(result['sparks'])
    if sparks:
        row = conn.execute('''UPDATE trainer_backend.progression_accounts SET balance=balance+%s,revision=revision+1
            WHERE player_id=%s RETURNING revision''', (sparks, player)).fetchone()
        event = uuid4()
        conn.execute('INSERT INTO trainer_backend.audit_events(id,actor_id,action,resource_id) VALUES(%s,%s,%s,%s)',
                     (uuid4(), player, 'support.topup_granted', event))
        publish(conn, GRANTED, event, player, row['revision'],
                {'player_id': str(player), 'sparks': sparks, 'rewards': result['rewards']})
    return sparks


class Support:
    def __init__(self, progression, verifier=None):
        self.progression, self.verifier = progression, verifier

    def reconcile(self, access):
        # Intentionally not exposed as an API or operator grant command. The real
        # adapter must verify account binding, membership, tier and source version.
        if self.verifier is None:
            raise AuthError('support_provider_not_configured', 503)
        with self.progression.db.transaction() as conn:
            player = self.progression.accounts.authenticate(conn, access).player_id
        provider = self.verifier.provider
        fact = self.verifier.membership(player)
        try:
            with self.progression.db.transaction() as conn:
                actor = self.progression.account(conn, access)
                record_verified(conn, provider, fact, actor.player_id)
        except UniqueViolation:
            raise AuthError('support_identity_conflict', 409) from None
        return self.progression.snapshot(access)


def record_verified(conn, provider, fact, player):
    """Internal adapter boundary. Caller owns link/session validation and transaction."""
    now = datetime.now(timezone.utc)
    if not isinstance(provider, str) or not re.fullmatch(r'[a-z][a-z0-9_.-]{0,31}', provider) or not isinstance(fact, Membership):
        raise AuthError('invalid_support_proof', 502)
    if (fact.player_id != player or type(fact.revision) is not int or not 0 < fact.revision <= 9223372036854775807
            or type(fact.multiplier) is not int or fact.multiplier not in (1, 2, 3)
            or any(not isinstance(v, str) or not 1 <= len(v) <= 128 or any(ord(c)<32 for c in v) for v in (fact.subject, fact.event_id))
            or any(not isinstance(t, datetime) or t.tzinfo is None or t.utcoffset() is None for t in (fact.observed_at, fact.valid_until))
            or fact.observed_at > now or not isinstance(fact.evidence_hash, str) or not re.fullmatch(r'[0-9a-f]{64}', fact.evidence_hash)):
        raise AuthError('invalid_support_proof', 502)
    payload = {'player_id': str(player), 'subject': fact.subject, 'revision': fact.revision, 'multiplier': fact.multiplier,
               'observed_at': fact.observed_at.astimezone(timezone.utc).isoformat(),
               'valid_until': fact.valid_until.astimezone(timezone.utc).isoformat(), 'evidence_hash': fact.evidence_hash}
    digest = hashlib.sha256(canonical(payload)).hexdigest()
    conn.execute('INSERT INTO trainer_backend.progression_accounts(player_id) VALUES(%s) ON CONFLICT DO NOTHING', (player,))
    conn.execute('SELECT player_id FROM trainer_backend.progression_accounts WHERE player_id=%s FOR UPDATE', (player,))
    prior = conn.execute('SELECT * FROM trainer_backend.support_receipts WHERE provider=%s AND event_id=%s', (provider, fact.event_id)).fetchone()
    if prior:
        if prior['player_id'] != player or prior['payload_hash'] != digest:
            raise AuthError('support_event_conflict', 409)
    else:
        previous = conn.execute('''SELECT m.player_id,r.* FROM trainer_backend.support_memberships m
            JOIN trainer_backend.support_receipts r ON r.id=m.receipt_id
            WHERE m.provider=%s AND (m.subject=%s OR m.player_id=%s)''', (provider, fact.subject, player)).fetchone()
        if previous and (previous['player_id'] != player or previous['subject'] != fact.subject):
            raise AuthError('support_identity_conflict', 409)
        if previous and (fact.revision == previous['revision'] and digest != previous['payload_hash']
                         or fact.revision > previous['revision'] and fact.observed_at < previous['observed_at']):
            raise AuthError('support_version_conflict', 409)
        receipt = uuid4()
        conn.execute('''INSERT INTO trainer_backend.support_receipts
            (id,player_id,provider,subject,event_id,revision,multiplier,observed_at,valid_until,evidence_hash,payload_hash)
            VALUES(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)''',
            (receipt, player, provider, fact.subject, fact.event_id, fact.revision, fact.multiplier,
             fact.observed_at, fact.valid_until, fact.evidence_hash, digest))
        if not previous or fact.revision > previous['revision']:
            conn.execute('''INSERT INTO trainer_backend.support_memberships(provider,subject,player_id,receipt_id)
                VALUES(%s,%s,%s,%s) ON CONFLICT(provider,subject) DO UPDATE SET receipt_id=EXCLUDED.receipt_id''',
                (provider, fact.subject, player, receipt))
            conn.execute('UPDATE trainer_backend.progression_accounts SET revision=revision+1 WHERE player_id=%s', (player,))
            conn.execute('INSERT INTO trainer_backend.audit_events(id,actor_id,action,resource_id) VALUES(%s,%s,%s,%s)',
                         (uuid4(), player, 'support.membership_changed', receipt))
            grant_topups(conn, player)
