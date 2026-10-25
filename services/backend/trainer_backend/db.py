# SPDX-License-Identifier: GPL-3.0-or-later
from contextlib import contextmanager
import hashlib
from importlib.resources import files
import logging

import psycopg
from psycopg import sql as sql_builder
from psycopg.rows import dict_row
from psycopg_pool import ConnectionPool


class MigrationError(RuntimeError):
    pass


class _RedactPoolMessage(logging.Filter):
    def filter(self, record):
        # Driver exceptions may contain connection parameters; do not format them.
        record.msg, record.args, record.exc_info = 'backend_database_pool_event', (), None
        return True


pool_log = logging.getLogger('psycopg.pool')
if not any(isinstance(f, _RedactPoolMessage) for f in pool_log.filters):
    pool_log.addFilter(_RedactPoolMessage())


def migrations():
    result = []
    for path in sorted(files('trainer_backend').joinpath('migrations').iterdir(), key=lambda p: p.name):
        if path.name.endswith('.sql'):
            sql = path.read_text(encoding='utf-8').replace('\r\n', '\n')
            result.append((path.name, hashlib.sha256(sql.encode()).hexdigest(), sql))
    return result


def _history_matches(rows, expected, *, complete):
    actual = [(r['version'], r['checksum']) for r in rows]
    wanted = [(version, digest) for version, digest, _ in expected]
    if actual != wanted[:len(actual)] or (complete and len(actual) != len(wanted)):
        raise MigrationError('schema_history_mismatch')


def migrate(database_url):
    """Explicit DDL entry point; never called by HTTP startup or a worker."""
    expected = migrations()
    with psycopg.connect(database_url, connect_timeout=3, row_factory=dict_row,
                         options='-c statement_timeout=30000 -c lock_timeout=10000') as conn:
        conn.execute('SELECT pg_advisory_xact_lock(1414680146)')
        conn.execute('CREATE SCHEMA IF NOT EXISTS trainer_backend')
        conn.execute('''CREATE TABLE IF NOT EXISTS trainer_backend.schema_migrations (
            version text PRIMARY KEY, checksum text NOT NULL CHECK(length(checksum)=64),
            applied_at timestamptz NOT NULL DEFAULT clock_timestamp())''')
        rows = conn.execute('SELECT version,checksum FROM trainer_backend.schema_migrations ORDER BY version').fetchall()
        _history_matches(rows, expected, complete=False)
        for version, digest, sql in expected[len(rows):]:
            conn.execute(sql, prepare=False)
            conn.execute('INSERT INTO trainer_backend.schema_migrations(version,checksum) VALUES (%s,%s)', (version, digest))
    return len(expected)


def grant_runtime(database_url, role):
    """Grant an existing non-owner role only foundation DML; no passwords/role creation."""
    rights = {
        'schema_migrations': 'SELECT', 'principals': 'SELECT',
        'operation_receipts': 'SELECT,INSERT,UPDATE', 'jobs': 'SELECT,INSERT,UPDATE',
        'event_index': 'SELECT,INSERT', 'outbox': 'SELECT,INSERT',
        'consumer_receipts': 'SELECT,INSERT', 'consumer_watermarks': 'SELECT,INSERT,UPDATE',
        'audit_events': 'SELECT,INSERT',
        'players': 'SELECT,UPDATE', 'external_identities': 'SELECT',
        'login_attempts': 'SELECT,INSERT,UPDATE', 'device_sessions': 'SELECT,INSERT,UPDATE',
        'refresh_credentials': 'SELECT,INSERT,UPDATE',
        'game_rulesets': 'SELECT', 'play_grants': 'SELECT,INSERT,UPDATE',
        'matches': 'SELECT,INSERT,UPDATE', 'match_participants': 'SELECT,INSERT',
        'result_submissions': 'SELECT,INSERT,UPDATE', 'result_decisions': 'SELECT,INSERT',
        'result_outcomes': 'SELECT,INSERT', 'board_revisions': 'SELECT,UPDATE',
    }
    with psycopg.connect(database_url, connect_timeout=3) as conn:
        conn.execute(sql_builder.SQL('GRANT USAGE ON SCHEMA trainer_backend TO {}').format(sql_builder.Identifier(role)))
        for table, privileges in rights.items():
            conn.execute(sql_builder.SQL('GRANT {} ON TABLE trainer_backend.{} TO {}').format(
                sql_builder.SQL(privileges), sql_builder.Identifier(table), sql_builder.Identifier(role)))
        conn.execute(sql_builder.SQL('GRANT EXECUTE ON FUNCTION trainer_backend.assert_principal(uuid,text) TO {}').format(sql_builder.Identifier(role)))
        conn.execute(sql_builder.SQL('GRANT EXECUTE ON FUNCTION trainer_backend.resolve_fluxer_player(text,text) TO {}').format(sql_builder.Identifier(role)))


class Database:
    def __init__(self, database_url, max_size=4):
        self.pool = ConnectionPool(database_url, min_size=0, max_size=max_size,
            timeout=3, max_waiting=16, open=True, reconnect_timeout=3,
            kwargs={'connect_timeout': 2, 'row_factory': dict_row,
                    'options': '-c statement_timeout=5000 -c lock_timeout=2000 -c idle_in_transaction_session_timeout=10000'})

    @contextmanager
    def transaction(self):
        with self.pool.connection() as conn:
            with conn.transaction():
                yield conn

    def ready(self):
        with self.transaction() as conn:
            rows = conn.execute('SELECT version,checksum FROM trainer_backend.schema_migrations ORDER BY version').fetchall()
            _history_matches(rows, migrations(), complete=True)
        return True

    def close(self):
        self.pool.close(timeout=3)
