from concurrent.futures import ThreadPoolExecutor
from uuid import uuid4

import psycopg
import pytest

from trainer_backend.commands import Conflict
from trainer_backend.economy import maintenance
from trainer_backend.economy.service import Progression
from trainer_backend.economy.shop import Shop
from test_accounts import account, login
from test_membership import funded


def award(postgres, account, user, operation=None, amount=100000, reason='Trading test'):
    return maintenance.grant_test_sparks(postgres[0], user['player_id'], account.provider.subject,
                                         amount, reason, operation or uuid4())


def test_grant_replay_spend_and_relogin(account, postgres):
    user = login(account)
    operation = uuid4()
    receipt = award(postgres, account, user, operation)
    assert award(postgres, account, user, operation) == receipt
    with pytest.raises(Conflict):
        award(postgres, account, user, operation, amount=1)
    progress = Progression(account.db, account)
    snapshot = progress.snapshot(login(account)['access_token'])
    assert snapshot['balance'] == 100000 and snapshot['mastery']['xp'] == 0
    assert len(snapshot['history']) == 1 and snapshot['history'][0]['source'] == 'operator.test_grant'
    assert snapshot['support_preview']['eligible_rewards'] == 0
    assert [o['top_up'] for o in snapshot['support_preview']['options']] == [0, 0]
    Shop(progress).purchase(user['access_token'], uuid4(), 'profile.aurora', 1, 250)
    snapshot = progress.snapshot(user['access_token'])
    assert snapshot['balance'] == sum(r['sparks'] for r in snapshot['history']) == 99750
    with account.db.transaction() as c:
        assert c.execute("SELECT count(*) n FROM trainer_backend.audit_events WHERE action='wallet.test_grant'").fetchone()['n'] == 1
        assert c.execute("SELECT count(*) n FROM trainer_backend.event_index WHERE event_type='wallet.test_sparks_granted'").fetchone()['n'] == 1


def test_parallel_grants_and_purchase_share_account_lock(account, postgres):
    user = login(account)
    award(postgres, account, user, amount=500)
    operation = uuid4()
    shop = Shop(Progression(account.db, account))
    with ThreadPoolExecutor(4) as pool:
        futures = [pool.submit(award, postgres, account, user, operation) for _ in range(3)]
        purchase = pool.submit(shop.purchase, user['access_token'], uuid4(), 'profile.dusk', 1, 250)
        assert len({str(f.result()['id']) for f in futures}) == 1
        purchase.result()
    assert shop.progression.snapshot(user['access_token'])['balance'] == 100250


def test_test_funding_never_multiplies(account, postgres):
    progress, user, verifier, support = funded(account)
    award(postgres, account, user)
    assert support.reconcile(user['access_token'])['balance'] == 102000
    verifier.set(2, 3)
    snapshot = support.reconcile(user['access_token'])
    assert snapshot['balance'] == 103000 and snapshot['mastery']['xp'] == 2000
    assert snapshot['support_preview']['base_sparks'] == 1000


def test_identity_and_privilege_boundaries(account, postgres):
    user = login(account)
    with pytest.raises(ValueError, match='identity_mismatch'):
        maintenance.grant_test_sparks(postgres[0], user['player_id'], '999', 100000, 'Trading test', uuid4())
    with pytest.raises(psycopg.errors.InsufficientPrivilege):
        maintenance.grant_test_sparks(postgres[1], user['player_id'], account.provider.subject, 100000, 'Trading test', uuid4())
    for statement in ('INSERT INTO trainer_backend.operator_spark_grants(id,player_id,sparks,reason) VALUES(%s,%s,1,\'Forged\')',
                      'UPDATE trainer_backend.operator_spark_grants SET sparks=1 WHERE id=%s OR player_id=%s',
                      'DELETE FROM trainer_backend.operator_spark_grants WHERE id=%s OR player_id=%s'):
        with pytest.raises(psycopg.errors.InsufficientPrivilege), account.db.transaction() as c:
            c.execute(statement, (uuid4(), user['player_id']))
    assert Progression(account.db, account).snapshot(user['access_token'])['balance'] == 0


@pytest.mark.parametrize('amount,reason', [(True, 'test'), (0, 'test'), (-1, 'test'), (1000001, 'test'), (1, ' '), (1, 'x'*201)])
def test_invalid_grants_rejected_before_connect(amount, reason):
    with pytest.raises(ValueError):
        maintenance.grant_test_sparks('unused', uuid4(), '123', amount, reason, uuid4())


def test_outbox_failure_rolls_back_every_effect(account, postgres, monkeypatch):
    user = login(account)
    with account.db.transaction() as c:
        audit_before = c.execute('SELECT count(*) n FROM trainer_backend.audit_events').fetchone()['n']
    def fail(*args):
        raise RuntimeError('outbox_failed')
    monkeypatch.setattr(maintenance, 'publish', fail)
    with pytest.raises(RuntimeError, match='outbox_failed'):
        award(postgres, account, user)
    with account.db.transaction() as c:
        for table in ('operator_spark_grants', 'reward_ledger', 'operation_receipts'):
            assert c.execute('SELECT count(*) n FROM trainer_backend.' + table).fetchone()['n'] == 0
        assert c.execute('SELECT count(*) n FROM trainer_backend.audit_events').fetchone()['n'] == audit_before
    assert Progression(account.db, account).snapshot(user['access_token'])['balance'] == 0
