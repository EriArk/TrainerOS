# SPDX-License-Identifier: GPL-3.0-or-later
from dataclasses import dataclass
import hashlib
from typing import Callable

from psycopg.types.json import Jsonb

from .commands import CODE, Conflict, canonical
from .modules import MODULE_NAMES


@dataclass(frozen=True)
class EventSpec:
    name: str
    producer: str
    validate: Callable

    def __post_init__(self):
        if not CODE.fullmatch(self.name) or self.producer not in MODULE_NAMES or not callable(self.validate):
            raise ValueError('invalid_event_registration')


def publish(conn, spec, event_id, aggregate_id, revision, payload):
    """Only trusted module code calls this after domain checks, inside its transaction."""
    if not 1 <= revision <= 9223372036854775807:
        raise ValueError('invalid_aggregate_revision')
    spec.validate(payload)
    digest = hashlib.sha256(canonical(payload)).hexdigest()
    row = conn.execute('''INSERT INTO trainer_backend.event_index
        (id,producer,event_type,aggregate_id,aggregate_revision,payload_hash)
        VALUES (%s,%s,%s,%s,%s,%s) ON CONFLICT(id) DO NOTHING RETURNING id''',
        (event_id, spec.producer, spec.name, aggregate_id, revision, digest)).fetchone()
    if row:
        conn.execute('INSERT INTO trainer_backend.outbox(event_id,payload) VALUES (%s,%s)', (event_id, Jsonb(payload)))
        return True
    previous = conn.execute('SELECT * FROM trainer_backend.event_index WHERE id=%s', (event_id,)).fetchone()
    if (previous['producer'], previous['event_type'], previous['aggregate_id'],
        previous['aggregate_revision'], previous['payload_hash']) != (spec.producer, spec.name, aggregate_id, revision, digest):
        raise Conflict('event_identity_conflict')
    # A payload deleted by retention must not be re-created by a replay.
    return False


@dataclass(frozen=True)
class Consumer:
    name: str
    event_types: tuple[str, ...]
    ordering: str
    apply: Callable

    def __post_init__(self):
        if not CODE.fullmatch(self.name) or not self.event_types or self.ordering not in ('all', 'latest'):
            raise ValueError('invalid_consumer_registration')


def deliver_once(db, consumer):
    """Effect and receipt commit together; handler exceptions leave event unacknowledged.

    'latest' is only for replaceable state projections; counters/ledger must opt into
    'all' and apply their own causal/order rules. There is no universal event ordering.
    """
    with db.transaction() as conn:
        candidates = conn.execute('''SELECT e.*,o.payload FROM trainer_backend.outbox o
            JOIN trainer_backend.event_index e ON e.id=o.event_id
            WHERE e.event_type=ANY(%s) AND NOT EXISTS(SELECT 1 FROM trainer_backend.consumer_receipts r
                WHERE r.consumer=%s AND r.event_id=e.id)
            ORDER BY e.created_at,e.id LIMIT 16''',
            (list(consumer.event_types), consumer.name)).fetchall()
        event = None
        for candidate in candidates:
            # An advisory lock needs no UPDATE permission on immutable outbox rows.
            locked = conn.execute('SELECT pg_try_advisory_xact_lock(hashtextextended(%s,0)) AS locked',
                (consumer.name + ':' + str(candidate['id']),)).fetchone()['locked']
            if not locked:
                continue
            # Fresh READ COMMITTED statement: a prior consumer may have committed
            # after the candidate snapshot but before our lock acquisition.
            applied = conn.execute('SELECT 1 FROM trainer_backend.consumer_receipts WHERE consumer=%s AND event_id=%s',
                (consumer.name, candidate['id'])).fetchone()
            if not applied:
                event = candidate
                break
        if not event:
            return 'idle'
        stale = False
        if consumer.ordering == 'latest':
            key = (consumer.name, event['producer'], event['aggregate_id'])
            conn.execute('''INSERT INTO trainer_backend.consumer_watermarks(consumer,producer,aggregate_id,revision)
                VALUES (%s,%s,%s,0) ON CONFLICT DO NOTHING''', key)
            watermark = conn.execute('''SELECT revision FROM trainer_backend.consumer_watermarks
                WHERE consumer=%s AND producer=%s AND aggregate_id=%s FOR UPDATE''', key).fetchone()
            stale = event['aggregate_revision'] <= watermark['revision']
        if not stale:
            consumer.apply(conn, event)
            if consumer.ordering == 'latest':
                conn.execute('''UPDATE trainer_backend.consumer_watermarks SET revision=%s
                    WHERE consumer=%s AND producer=%s AND aggregate_id=%s''', (event['aggregate_revision'], *key))
        disposition = 'stale' if stale else 'applied'
        conn.execute('INSERT INTO trainer_backend.consumer_receipts(consumer,event_id,disposition) VALUES (%s,%s,%s)',
                     (consumer.name, event['id'], disposition))
        return disposition
