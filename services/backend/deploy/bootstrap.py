"""Explicit maintenance command. Credentials remain in server-side secret files."""
from pathlib import Path
import psycopg
from psycopg import sql
from psycopg.conninfo import conninfo_to_dict, make_conninfo
from trainer_backend.db import migrate, grant_runtime

owner = make_conninfo(host='db', dbname='traineros_backend', user='backend_owner',
                     password=Path('/run/secrets/owner-password').read_text().strip())
runtime = conninfo_to_dict(Path('/run/secrets/runtime-dsn').read_text().strip())
try:
    with psycopg.connect(owner) as conn:
        exists = conn.execute("SELECT 1 FROM pg_roles WHERE rolname='backend_runtime'").fetchone()
        if not exists:
            conn.execute('CREATE ROLE backend_runtime LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE NOINHERIT')
        conn.execute(sql.SQL('ALTER ROLE backend_runtime PASSWORD {}').format(sql.Literal(runtime['password'])))
        conn.execute('REVOKE ALL ON DATABASE traineros_backend FROM PUBLIC')
        conn.execute('GRANT CONNECT ON DATABASE traineros_backend TO backend_runtime')
        conn.execute('REVOKE CREATE ON SCHEMA public FROM PUBLIC')
    migrate(owner)
    grant_runtime(owner, 'backend_runtime')
    print('foundation_migrated_runtime_granted')
except Exception:
    raise SystemExit('foundation_migration_failed') from None
