# SPDX-License-Identifier: GPL-3.0-or-later
"""Starts its OWN loopback-only PostgreSQL. Never uses an existing database URL."""
import os
from pathlib import Path
import secrets
import socket
import subprocess
import shutil
from uuid import uuid4

import psycopg
from psycopg.conninfo import make_conninfo
import pytest

from trainer_backend.commands import Principal
from trainer_backend.db import Database, grant_runtime, migrate

ROOT = Path(__file__).resolve().parents[3]
FLAGS = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0


@pytest.fixture(scope='session')
def postgres():
    bin_dir = Path(os.environ.get('TRAINEROS_TEST_POSTGRES_BIN',
        str(ROOT / 'build/postgres-portable/pgsql/bin'))).resolve()
    suffix = '.exe' if os.name == 'nt' else ''
    pg_ctl = bin_dir / ('pg_ctl' + suffix)
    if not pg_ctl.exists():
        pytest.fail('Set TRAINEROS_TEST_POSTGRES_BIN to PostgreSQL binaries; no DB test is silently skipped.')
    test_root = ROOT / 'build' / ('backend-pg-test-' + uuid4().hex)
    test_root.mkdir(parents=True)
    password = secrets.token_urlsafe(32)
    pwfile = test_root / 'init-password'
    pwfile.write_text(password, encoding='ascii')
    if os.name != 'nt':
        pwfile.chmod(0o600)
    subprocess.run([str(bin_dir / ('initdb' + suffix)), '-D', str(test_root / 'data'),
        '-U', 'backend_test_owner', '--auth=scram-sha-256', '--encoding=UTF8', '--locale=C',
        '--pwfile=' + str(pwfile)], check=True, capture_output=True, creationflags=FLAGS)
    pwfile.unlink()
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    # On Windows postgres can inherit a captured pg_ctl pipe, preventing EOF forever.
    # File-backed output plus DEVNULL stdin leaves no parent reader waiting on the daemon.
    with (test_root / 'pg_ctl.log').open('wb') as output:
        subprocess.run([str(pg_ctl), '-D', str(test_root / 'data'), '-l', str(test_root / 'postgres.log'),
            '-o', f'-h 127.0.0.1 -p {port} -k "{test_root}"', '-w', 'start'], check=True, stdout=output,
            stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, timeout=30, creationflags=FLAGS)
    admin_url = make_conninfo(host='127.0.0.1', port=port, dbname='postgres',
        user='backend_test_owner', password=password)
    try:
        with psycopg.connect(admin_url, autocommit=True) as conn:
            conn.execute('CREATE ROLE backend_runtime NOSUPERUSER NOCREATEDB NOCREATEROLE NOINHERIT LOGIN')
            # Generated test-only credential is quoted by psycopg, never printed.
            from psycopg import sql
            conn.execute(sql.SQL('ALTER ROLE backend_runtime PASSWORD {}').format(sql.Literal(password)))
        migrate(admin_url)
        grant_runtime(admin_url, 'backend_runtime')
        with psycopg.connect(admin_url) as conn:
            conn.execute('CREATE TABLE public.fixture_effects(id uuid PRIMARY KEY, value integer NOT NULL)')
            conn.execute('GRANT SELECT,INSERT,UPDATE ON public.fixture_effects TO backend_runtime')
        runtime_url = make_conninfo(host='127.0.0.1', port=port, dbname='postgres',
            user='backend_runtime', password=password)
        yield admin_url, runtime_url
    finally:
        subprocess.run([str(pg_ctl), '-D', str(test_root / 'data'), '-m', 'fast', '-w', 'stop'],
            check=True, capture_output=True, creationflags=FLAGS)
        # This fixture owns only its generated directory, never an external database.
        if test_root.parent.resolve() != (ROOT / 'build').resolve() or not test_root.name.startswith('backend-pg-test-'):
            raise RuntimeError('unsafe_fixture_cleanup_path')
        shutil.rmtree(test_root)


@pytest.fixture
def db(postgres):
    admin_url, runtime_url = postgres
    with psycopg.connect(admin_url) as conn:
        conn.execute('''TRUNCATE trainer_backend.principals, trainer_backend.operation_receipts,
            trainer_backend.jobs,trainer_backend.event_index,trainer_backend.outbox,
            trainer_backend.consumer_receipts,trainer_backend.consumer_watermarks,
            trainer_backend.audit_events,trainer_backend.players,trainer_backend.external_identities,trainer_backend.public_profiles,
            trainer_backend.login_attempts,trainer_backend.device_sessions,trainer_backend.refresh_credentials,
            trainer_backend.game_rulesets,trainer_backend.play_grants,trainer_backend.matches,trainer_backend.match_participants,
            trainer_backend.result_submissions,trainer_backend.result_decisions,trainer_backend.result_outcomes,trainer_backend.board_revisions,
            trainer_backend.reward_ledger,trainer_backend.support_memberships,trainer_backend.support_receipts,trainer_backend.reward_links,trainer_backend.reward_link_challenges,trainer_backend.outfits,trainer_backend.profile_appearances,trainer_backend.owned_items,trainer_backend.purchases,trainer_backend.progression_accounts,
            public.fixture_effects''')
    database = Database(runtime_url, 8)
    yield database
    database.close()


@pytest.fixture
def actor(postgres, db):
    principal = Principal(uuid4(), 'player')
    with psycopg.connect(postgres[0]) as conn:
        conn.execute("INSERT INTO trainer_backend.principals(id,kind,state) VALUES (%s,'player','active')", (principal.id,))
    return principal
