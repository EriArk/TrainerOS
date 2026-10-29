# SPDX-License-Identifier: GPL-3.0-or-later
"""Explicit owner-authorized test funding, never exposed through HTTP or the SDK."""
from contextlib import contextmanager
from uuid import UUID, uuid4, uuid5, NAMESPACE_URL

import psycopg
from psycopg.rows import dict_row

from ..commands import Principal, Outcome, run_command
from ..events import EventSpec, publish
from .support import MAX_SAFE_INTEGER


def validate_grant(value):
    if set(value) != {'grant_id', 'player_id', 'sparks'}:
        raise ValueError('invalid_test_grant_event')
    UUID(value['grant_id'])
    UUID(value['player_id'])
    if type(value['sparks']) is not int or not 1 <= value['sparks'] <= 1000000:
        raise ValueError('invalid_test_grant_amount')


GRANTED = EventSpec('wallet.test_sparks_granted', 'wallet', validate_grant)


class BoundDatabase:
    def __init__(self, conn):
        self.conn = conn

    @contextmanager
    def transaction(self):
        yield self.conn


def grant_test_sparks(database_url, player_id, expected_subject, amount, reason, operation_id):
    player = UUID(str(player_id))
    operation = UUID(str(operation_id))
    if type(amount) is not int or not 1 <= amount <= 1000000:
        raise ValueError('invalid_test_grant_amount')
    if not isinstance(reason, str) or not 1 <= len(reason.strip()) <= 200:
        raise ValueError('test_grant_reason_required')
    if not isinstance(expected_subject, str) or not expected_subject.isascii() or not expected_subject.isdigit() or not 1 <= len(expected_subject) <= 32:
        raise ValueError('expected_fluxer_subject_required')
    reason = reason.strip()
    actor = uuid5(NAMESPACE_URL, 'traineros-backend:local-maintenance-operator')
    payload = {'player_id': str(player), 'expected_subject': expected_subject, 'sparks': amount, 'reason': reason}
    with psycopg.connect(database_url, row_factory=dict_row,
                         options='-c statement_timeout=15000 -c lock_timeout=5000') as conn:
        # No runtime role can create/elevate this principal or insert a grant receipt.
        conn.execute("INSERT INTO trainer_backend.principals(id,kind,state) VALUES(%s,'operator','active') ON CONFLICT DO NOTHING", (actor,))

        def apply(c, receipt):
            identity = c.execute('''SELECT e.player_id FROM trainer_backend.external_identities e
                JOIN trainer_backend.principals p ON p.id=e.player_id
                WHERE e.player_id=%s AND e.provider='fluxer' AND e.instance='https://fluxer.app'
                AND e.subject=%s AND p.kind='player' AND p.state='active' FOR SHARE OF e,p''',
                (player, expected_subject)).fetchone()
            if not identity:
                raise ValueError('player_identity_mismatch')
            c.execute('INSERT INTO trainer_backend.progression_accounts(player_id) VALUES(%s) ON CONFLICT DO NOTHING', (player,))
            account = c.execute('SELECT balance FROM trainer_backend.progression_accounts WHERE player_id=%s FOR UPDATE', (player,)).fetchone()
            if account['balance'] > MAX_SAFE_INTEGER - amount:
                raise ValueError('balance_limit')
            c.execute('INSERT INTO trainer_backend.operator_spark_grants(id,player_id,sparks,reason) VALUES(%s,%s,%s,%s)',
                      (receipt, player, amount, reason))
            ledger = uuid4()
            c.execute('''INSERT INTO trainer_backend.reward_ledger
                (id,player_id,source,event_id,title,xp,sparks,base_sparks,policy,earned_at,operator_grant_id)
                VALUES(%s,%s,'operator.test_grant',%s,%s,0,%s,0,'operator-test-v1',clock_timestamp(),%s)''',
                (ledger, player, str(operation), 'Тестовые Искры: ' + reason, amount, receipt))
            row = c.execute('UPDATE trainer_backend.progression_accounts SET balance=balance+%s,revision=revision+1 WHERE player_id=%s RETURNING revision', (amount, player)).fetchone()
            publish(c, GRANTED, ledger, player, row['revision'],
                    {'grant_id': str(receipt), 'player_id': str(player), 'sparks': amount})
            return Outcome('completed', ledger)

        return run_command(BoundDatabase(conn), Principal(actor, 'operator'),
                           'wallet.test_grant', operation, payload, apply)
