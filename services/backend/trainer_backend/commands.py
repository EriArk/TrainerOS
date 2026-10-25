# SPDX-License-Identifier: GPL-3.0-or-later
from dataclasses import dataclass
import hashlib
import json
import re
from uuid import UUID, uuid4

from .modules import MODULE_NAMES

CODE = re.compile(r'^[a-z][a-z0-9_.-]{0,63}$')


class Conflict(RuntimeError):
    pass


class Forbidden(RuntimeError):
    pass


def canonical(value):
    if not isinstance(value, dict):
        raise ValueError('object_required')
    raw = json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False,
                     allow_nan=False).encode('utf-8')
    if len(raw) > 65536:
        raise ValueError('body_too_large')
    return raw


@dataclass(frozen=True)
class Principal:
    """Trusted internal identity from a future auth adapter, never a request field."""
    id: UUID
    kind: str


@dataclass(frozen=True)
class Outcome:
    state: str
    resource_id: UUID | None = None


def require_principal(conn, principal):
    row = conn.execute('SELECT trainer_backend.assert_principal(%s,%s) AS allowed', (principal.id, principal.kind)).fetchone()
    if not row['allowed']:
        raise Forbidden('principal_unavailable')


def run_command(db, principal, kind, operation_id, payload, handler):
    """Handler, receipt, audit and queued effects share one database transaction.

    Authorization of the domain resource remains the trusted handler's duty.
    Only a hash of the request and a bounded typed receipt are persisted here.
    """
    if not CODE.fullmatch(kind) or kind.split('.')[0] not in MODULE_NAMES:
        raise ValueError('unknown_command_owner')
    operation_id = UUID(str(operation_id))
    payload_hash = hashlib.sha256(canonical(payload)).hexdigest()
    with db.transaction() as conn:
        require_principal(conn, principal)
        receipt_id = uuid4()
        inserted = conn.execute('''INSERT INTO trainer_backend.operation_receipts
            (id,actor_id,operation_kind,operation_id,payload_hash,state)
            VALUES (%s,%s,%s,%s,%s,'pending')
            ON CONFLICT(actor_id,operation_kind,operation_id) DO NOTHING RETURNING id''',
            (receipt_id, principal.id, kind, operation_id, payload_hash)).fetchone()
        if not inserted:
            old = conn.execute('''SELECT id,payload_hash,state,resource_id,revision
                FROM trainer_backend.operation_receipts WHERE actor_id=%s AND operation_kind=%s
                AND operation_id=%s FOR UPDATE''', (principal.id, kind, operation_id)).fetchone()
            if old['payload_hash'] != payload_hash:
                raise Conflict('operation_payload_conflict')
            return old
        outcome = handler(conn, receipt_id)
        if not isinstance(outcome, Outcome) or outcome.state not in ('pending', 'completed', 'rejected'):
            raise ValueError('invalid_command_outcome')
        result = conn.execute('''UPDATE trainer_backend.operation_receipts SET state=%s,resource_id=%s
            WHERE id=%s RETURNING id,payload_hash,state,resource_id,revision''',
            (outcome.state, outcome.resource_id, receipt_id)).fetchone()
        conn.execute('INSERT INTO trainer_backend.audit_events(id,actor_id,action,resource_id) VALUES (%s,%s,%s,%s)',
                     (uuid4(), principal.id, kind, receipt_id))
        return result


def receipt(db, principal, receipt_id):
    with db.transaction() as conn:
        require_principal(conn, principal)
        return conn.execute('''SELECT id,state,resource_id,revision FROM trainer_backend.operation_receipts
            WHERE id=%s AND actor_id=%s''', (receipt_id, principal.id)).fetchone()
