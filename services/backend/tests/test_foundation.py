# SPDX-License-Identifier: GPL-3.0-or-later
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import threading
from uuid import uuid4

from fastapi.testclient import TestClient
import psycopg
import pytest

from trainer_backend.app import create_app
from trainer_backend.commands import Conflict, Forbidden, Outcome, Principal, receipt, run_command
from trainer_backend.config import Settings
from trainer_backend.db import Database, MigrationError, migrate, migrations
from trainer_backend.events import Consumer, EventSpec, deliver_once, publish
from trainer_backend.jobs import LeaseLost, Worker, claim, complete, enqueue, fail
from trainer_backend.modules import MODULES


def effect(conn, key, value=1):
    conn.execute('''INSERT INTO public.fixture_effects(id,value) VALUES (%s,%s)
        ON CONFLICT(id) DO UPDATE SET value=fixture_effects.value+EXCLUDED.value''', (key, value))


def count(db, key):
    with db.transaction() as conn:
        row = conn.execute('SELECT value FROM public.fixture_effects WHERE id=%s', (key,)).fetchone()
    return row['value'] if row else 0


def queue_job(db, actor, key=None, attempts=3):
    resource_id = key or uuid4()
    def handler(conn, receipt_id):
        job_id = enqueue(conn, 'operations.fixture', resource_id, receipt_id, attempts)
        return Outcome('pending', job_id)
    result = run_command(db, actor, 'operations.queue', uuid4(), {}, handler)
    return result['resource_id'], resource_id


def test_module_inventory_matches_design():
    design = json.loads((Path(__file__).parents[1] / 'contracts/modules.json').read_text(encoding='utf-8'))
    assert {m.name: m.owner for m in MODULES} == design['owners']


def test_migration_repeat_and_runtime_privileges(postgres, db):
    assert migrate(postgres[0]) == len(migrations())
    assert migrate(postgres[0]) == len(migrations())
    assert db.ready()
    forbidden = [
        'CREATE TABLE trainer_backend.not_allowed(id integer)',
        "UPDATE trainer_backend.schema_migrations SET checksum=repeat('0',64)",
        "UPDATE trainer_backend.audit_events SET action='changed'",
        "UPDATE trainer_backend.principals SET state='active'",
        'DELETE FROM trainer_backend.event_index',
    ]
    for sql in forbidden:
        with pytest.raises(psycopg.errors.InsufficientPrivilege):
            with db.transaction() as conn:
                conn.execute(sql)


def test_migration_tamper_detected(postgres, db):
    with psycopg.connect(postgres[0]) as conn:
        original = conn.execute('SELECT version,checksum FROM trainer_backend.schema_migrations').fetchall()
        conn.execute("UPDATE trainer_backend.schema_migrations SET checksum=repeat('0',64)")
    try:
        with pytest.raises(MigrationError):
            migrate(postgres[0])
        with pytest.raises(MigrationError):
            db.ready()
    finally:
        with psycopg.connect(postgres[0]) as conn:
            for version,checksum in original:
                conn.execute('UPDATE trainer_backend.schema_migrations SET checksum=%s WHERE version=%s', (checksum,version))


def test_concurrent_duplicate_command_applies_once(db, actor):
    operation_id, key = uuid4(), uuid4()
    start = threading.Barrier(6)
    def submit(_):
        start.wait(timeout=5)
        return run_command(db, actor, 'operations.increment', operation_id, {'value': 1},
            lambda conn, _: (effect(conn, key), Outcome('completed', key))[1])
    with ThreadPoolExecutor(max_workers=6) as pool:
        results = list(pool.map(submit, range(6)))
    assert len({r['id'] for r in results}) == 1
    assert count(db, key) == 1
    with pytest.raises(Conflict):
        run_command(db, actor, 'operations.increment', operation_id, {'value': 2}, lambda *_: Outcome('completed'))


def test_command_rollback_includes_job_receipt_and_effect(db, actor):
    key, operation_id = uuid4(), uuid4()
    def broken(conn, receipt_id):
        effect(conn, key)
        enqueue(conn, 'operations.fixture', key, receipt_id)
        raise RuntimeError('intentional')
    with pytest.raises(RuntimeError):
        run_command(db, actor, 'operations.rollback', operation_id, {}, broken)
    assert count(db, key) == 0
    with db.transaction() as conn:
        assert conn.execute('SELECT count(*) AS n FROM trainer_backend.jobs').fetchone()['n'] == 0
        assert conn.execute('SELECT count(*) AS n FROM trainer_backend.operation_receipts').fetchone()['n'] == 0


def test_revoke_and_foreign_receipt(postgres, db, actor):
    done = run_command(db, actor, 'operations.test', uuid4(), {}, lambda *_: Outcome('completed'))
    other = Principal(uuid4(), 'player')
    with psycopg.connect(postgres[0]) as conn:
        conn.execute("INSERT INTO trainer_backend.principals VALUES (%s,'player','active',clock_timestamp())", (other.id,))
    assert receipt(db, other, done['id']) is None
    with pytest.raises(Forbidden):
        run_command(db, Principal(actor.id, 'operator'), 'operations.test', uuid4(), {}, lambda *_: Outcome('completed'))
    with psycopg.connect(postgres[0]) as conn:
        conn.execute("UPDATE trainer_backend.principals SET state='revoked' WHERE id=%s", (actor.id,))
    with pytest.raises(Forbidden):
        run_command(db, actor, 'operations.test', uuid4(), {}, lambda *_: Outcome('completed'))


def test_claim_concurrency_and_completion(db, actor):
    job_id, key = queue_job(db, actor)
    with ThreadPoolExecutor(max_workers=4) as pool:
        claims = list(pool.map(lambda _: claim(db, ['operations.fixture']), range(4)))
    obtained = [job for job in claims if job]
    assert len(obtained) == 1 and obtained[0].id == job_id
    complete(db, obtained[0], lambda conn, _: effect(conn, key))
    assert count(db, key) == 1
    with pytest.raises(LeaseLost):
        complete(db, obtained[0], lambda conn, _: effect(conn, key))


def test_expired_lease_fences_old_worker(postgres, db, actor):
    job_id, key = queue_job(db, actor)
    old = claim(db, ['operations.fixture'])
    with psycopg.connect(postgres[0]) as conn:
        conn.execute("UPDATE trainer_backend.jobs SET lease_until=clock_timestamp()-interval '1 second' WHERE id=%s", (job_id,))
    new = claim(db, ['operations.fixture'])
    assert new.fence == old.fence + 1 and new.lease_token != old.lease_token
    with pytest.raises(LeaseLost):
        complete(db, old, lambda conn, _: effect(conn, key, 100))
    assert not fail(db, old, 0)
    complete(db, new, lambda conn, _: effect(conn, key))
    assert count(db, key) == 1


def test_handler_failure_rolls_back_then_dead_letters(db, actor):
    job_id, key = queue_job(db, actor, attempts=2)
    def broken(conn, _):
        effect(conn, key)
        raise RuntimeError('intentional handler failure')
    for _ in range(2):
        job = claim(db, ['operations.fixture'])
        with pytest.raises(RuntimeError):
            complete(db, job, broken)
        assert fail(db, job, 0)
    assert claim(db, ['operations.fixture']) is None
    assert count(db, key) == 0
    with db.transaction() as conn:
        assert conn.execute('SELECT state,attempt FROM trainer_backend.jobs WHERE id=%s', (job_id,)).fetchone() == {'state': 'dead_letter', 'attempt': 2}


def test_unknown_jobs_not_silently_acknowledged(db, actor):
    queue_job(db, actor)
    assert Worker(db, {}).once() == 'idle'
    assert claim(db, ['operations.fixture']) is not None


def valid_payload(payload):
    if set(payload) != {'value'} or type(payload['value']) is not int:
        raise ValueError('invalid_fixture_event')


SPEC = EventSpec('operations.fixture', 'operations', valid_payload)


def test_event_dedup_survives_payload_retention(postgres, db):
    event_id, aggregate_id = uuid4(), uuid4()
    with db.transaction() as conn:
        assert publish(conn, SPEC, event_id, aggregate_id, 1, {'value': 1})
        assert not publish(conn, SPEC, event_id, aggregate_id, 1, {'value': 1})
    with pytest.raises(Conflict):
        with db.transaction() as conn:
            publish(conn, SPEC, event_id, aggregate_id, 1, {'value': 2})
    with psycopg.connect(postgres[0]) as conn:
        conn.execute('DELETE FROM trainer_backend.outbox WHERE event_id=%s', (event_id,))
    with db.transaction() as conn:
        assert not publish(conn, SPEC, event_id, aggregate_id, 1, {'value': 1})
        assert conn.execute('SELECT count(*) AS n FROM trainer_backend.outbox').fetchone()['n'] == 0


def test_consumer_failure_retry_and_concurrent_dedup(db):
    key = uuid4()
    with db.transaction() as conn:
        publish(conn, SPEC, uuid4(), key, 1, {'value': 1})
    def broken(conn, _):
        effect(conn, key)
        raise RuntimeError('crash before acknowledgement')
    with pytest.raises(RuntimeError):
        deliver_once(db, Consumer('operations.counter', (SPEC.name,), 'all', broken))
    assert count(db, key) == 0
    good = Consumer('operations.counter', (SPEC.name,), 'all', lambda conn, e: effect(conn, key, e['payload']['value']))
    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(lambda _: deliver_once(db, good), range(4)))
    assert results.count('applied') == 1
    assert count(db, key) == 1


def test_latest_state_never_restores_revoked_older_revision(db):
    key = uuid4()
    with db.transaction() as conn:
        publish(conn, SPEC, uuid4(), key, 2, {'value': 2})
    consumer = Consumer('operations.state', (SPEC.name,), 'latest', lambda conn, e: effect(conn, key, e['payload']['value']))
    assert deliver_once(db, consumer) == 'applied'
    with db.transaction() as conn:
        publish(conn, SPEC, uuid4(), key, 1, {'value': 1})
    assert deliver_once(db, consumer) == 'stale'
    assert count(db, key) == 2


def test_api_has_only_real_foundation_routes(postgres, db):
    with TestClient(create_app(Settings(postgres[1], 'test'))) as client:
        assert client.get('/health/live').status_code == 200
        assert client.get('/health/ready').json()['scope'] == 'foundation-only'
        capabilities = client.get('/v1/capabilities').json()
        assert capabilities['capabilities'] == {}
        assert len(capabilities['modules']) == 13
        assert client.post('/v1/me/purchases', json={}).status_code == 404
        paths=set(client.get('/openapi.json').json()['paths'])
        assert {'/health/live','/health/ready','/v1/capabilities','/v1/auth/refresh'}<=paths
        assert '/v1/me/purchases' not in paths
        assert client.post('/v1/auth/refresh',json={'refresh_token':'x'*43}).status_code==503
        response = client.get('/health/live')
        assert response.headers['cache-control'] == 'no-store'
        assert response.headers['x-request-id']


def test_unreachable_database_is_degraded_not_fake_ready():
    secret = 'never-log-this-test-secret'
    settings = Settings(f'host=127.0.0.1 port=1 user=unavailable password={secret} dbname=unavailable', 'test')
    assert secret not in repr(settings)
    with TestClient(create_app(settings)) as client:
        assert client.get('/health/live').status_code == 200
        response = client.get('/health/ready')
        assert response.status_code == 503 and secret not in response.text


def test_production_refused_until_deployment_is_prepared():
    with pytest.raises(ValueError, match='not_approved'):
        Settings('redacted', 'production')


def test_server_settings_load_secret_file(tmp_path, monkeypatch):
    secret = tmp_path / 'dsn'
    secret.write_text('host=db password=not-in-repr')
    monkeypatch.setenv('TRAINEROS_BACKEND_DATABASE_URL_FILE', str(secret))
    monkeypatch.setenv('TRAINEROS_BACKEND_ENV', 'server')
    monkeypatch.setenv('TRAINEROS_BACKEND_HOST', '0.0.0.0')
    settings = Settings.from_env()
    assert settings.environment == 'server' and settings.host == '0.0.0.0'
    assert settings.database_url == secret.read_text()
    assert 'not-in-repr' not in repr(settings)
