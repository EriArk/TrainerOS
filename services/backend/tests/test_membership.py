from concurrent.futures import ThreadPoolExecutor
from dataclasses import replace
from datetime import datetime, timedelta, timezone
from uuid import uuid4
import time
import pytest
import psycopg
from fastapi.testclient import TestClient
from trainer_backend.app import create_app
from trainer_backend.config import Settings
from trainer_backend.accounts.provider import AuthError
from trainer_backend.economy.membership import Membership, Support
from trainer_backend.economy.shop import Shop
from test_accounts import account, login, CONFIG, Provider
from test_progression import linked, release


class VerifiedFixture:
    provider = 'test_provider'
    def __init__(self, player):
        self.player = player
        self.set(1, 2)
    def set(self, revision, multiplier, valid_until=None):
        now = datetime.now(timezone.utc)
        self.fact = Membership(self.player, 'verified-subject', 'event-'+str(revision), revision,
            multiplier, now, valid_until or now+timedelta(days=1), 'a'*64)
    def membership(self, player): return self.fact


def funded(account):
    progress, user, proof = linked(account)
    progress.sync(user['access_token'], 'k'*32, [1])
    verifier = VerifiedFixture(user['player_id'])
    return progress, user, verifier, Support(progress, verifier)


def test_upgrade_cancel_rejoin_and_new_rewards_keep_per_reward_high_water(account):
    progress, user, verifier, support = funded(account);access=user['access_token']
    Shop(progress).purchase(access, uuid4(), 'profile.aurora', 1, 250)
    first=support.reconcile(access)
    assert first['balance']==1750 and first['mastery']['xp']==2000
    assert first['support']['multiplier']==2
    assert [o['top_up'] for o in first['support_preview']['options']]==[0,1000]
    repeat=Support(progress,verifier).reconcile(login(account)['access_token'])
    assert repeat['revision']==first['revision'] and repeat['balance']==1750
    verifier.set(2,3);old=verifier.fact
    upgraded=support.reconcile(access)
    assert upgraded['balance']==2750 and upgraded['mastery']['xp']==2000
    assert [o['top_up'] for o in upgraded['support_preview']['options']]==[0,0]
    verifier.set(3,1);cancelled=support.reconcile(access)
    assert cancelled['balance']==2750 and cancelled['support']['multiplier']==1
    assert len(Shop(progress).snapshot(access)['items'])==1
    verifier.fact=old;assert support.reconcile(access)['support']['multiplier']==1
    release(account.db);new=progress.sync(access,'k'*32,[2])
    assert new['balance']==3750 and new['mastery']['xp']==4000
    verifier.set(4,2);assert support.reconcile(access)['balance']==4750
    # A lower new tier cannot claw back old x3 rewards or pay them again.
    release(account.db);fresh=progress.sync(access,'k'*32,[3])
    assert fresh['balance']==6750 and fresh['mastery']['xp']==6000
    verifier.set(5,3);final=support.reconcile(access)
    assert final['balance']==8750 and final['support_preview']['credited_reward_sparks']==9000
    with account.db.transaction() as c:
        rows=c.execute("SELECT base_reward_id,max(support_multiplier) m,sum(sparks) paid FROM trainer_backend.reward_ledger WHERE source='support.topup' GROUP BY base_reward_id").fetchall()
        assert len(rows)==93 and all(r['m']==3 for r in rows)
        assert sum(r['paid'] for r in rows)==6000
        assert c.execute('SELECT sum(sparks) n FROM trainer_backend.reward_ledger').fetchone()['n']==8750


def test_downgrade_expiry_and_overlapping_providers_do_not_stack(account):
    progress,user,verifier,support=funded(account);access=user['access_token']
    verifier.set(1,3);support.reconcile(access)
    verifier.set(2,2);support.reconcile(access)
    release(account.db);assert progress.sync(access,'k'*32,[2])['balance']==5000
    second=VerifiedFixture(user['player_id']);second.provider='another_provider';second.set(1,3)
    other=Support(progress,second);assert other.reconcile(access)['balance']==6000
    verifier.set(3,1);assert support.reconcile(access)['support']['multiplier']==3
    second.set(2,3,datetime.now(timezone.utc)+timedelta(seconds=1));other.reconcile(access)
    time.sleep(1.05)
    assert progress.snapshot(access)['support']['multiplier']==1
    release(account.db);after=progress.sync(access,'k'*32,[3]);assert after['balance']==7000
    # A proof that is already expired cannot activate retrospective grants.
    second.set(3,3,datetime.now(timezone.utc)-timedelta(seconds=1));assert other.reconcile(access)['balance']==7000


def test_concurrent_delivery_and_purchase_are_atomic_and_idempotent(account):
    progress,user,verifier,support=funded(account);access=user['access_token']
    verifier.set(1,3)
    with ThreadPoolExecutor(3) as pool:
        calls=[pool.submit(support.reconcile,access),pool.submit(support.reconcile,access),
               pool.submit(Shop(progress).purchase,access,uuid4(),'profile.orbit',1,250)]
        for call in calls:call.result()
    assert progress.snapshot(access)['balance']==2750
    with account.db.transaction() as c:
        assert c.execute("SELECT count(*) n FROM trainer_backend.reward_ledger WHERE source='support.topup'").fetchone()['n']==31
        assert c.execute('SELECT count(*) n FROM trainer_backend.support_receipts').fetchone()['n']==1


def test_failure_rolls_back_receipt_money_audit_and_outbox(account,monkeypatch):
    progress,user,verifier,support=funded(account);access=user['access_token']
    import trainer_backend.economy.membership as module
    original=module.publish
    def fail(*args):raise RuntimeError('outbox interrupted')
    monkeypatch.setattr(module,'publish',fail)
    with pytest.raises(RuntimeError):support.reconcile(access)
    assert progress.snapshot(access)['balance']==1000
    with account.db.transaction() as c:
        assert c.execute('SELECT count(*) n FROM trainer_backend.support_receipts').fetchone()['n']==0
        assert c.execute("SELECT count(*) n FROM trainer_backend.audit_events WHERE action LIKE 'support.%'").fetchone()['n']==0
    monkeypatch.setattr(module,'publish',original)
    assert support.reconcile(access)['balance']==2000


def test_event_conflicts_out_of_order_and_identity_binding(account):
    progress,user,verifier,support=funded(account);access=user['access_token']
    initial=verifier.fact;support.reconcile(access)
    verifier.fact=replace(initial,multiplier=3)
    with pytest.raises(AuthError,match='event_conflict'):support.reconcile(access)
    verifier.fact=replace(initial,event_id='different',multiplier=3)
    with pytest.raises(AuthError,match='version_conflict'):support.reconcile(access)
    verifier.set(4,1);support.reconcile(access)
    verifier.fact=replace(initial,event_id='delayed',revision=3,multiplier=3)
    assert support.reconcile(access)['support']['multiplier']==1
    account.provider.subject='99999';other=login(account)
    verifier.fact=replace(initial,player_id=other['player_id'],event_id='other-player')
    with pytest.raises(AuthError,match='identity_conflict'):support.reconcile(other['access_token'])
    verifier.fact=replace(initial,subject='replacement',event_id='replacement')
    with pytest.raises(AuthError,match='identity_conflict'):support.reconcile(access)
    assert progress.snapshot(access)['balance']==2000


def test_unconfigured_forged_revoked_and_immutable_boundaries(account,postgres):
    progress,user,verifier,support=funded(account);access=user['access_token']
    with pytest.raises(AuthError,match='not_configured'):Support(progress).reconcile(access)
    valid=verifier.fact
    for bad in [replace(valid,player_id=uuid4()),replace(valid,multiplier=True),replace(valid,multiplier=4),
                replace(valid,revision=0),replace(valid,observed_at=datetime.now()),replace(valid,evidence_hash='fake')]:
        verifier.fact=bad
        with pytest.raises(AuthError,match='invalid_support_proof'):support.reconcile(access)
    verifier.fact=valid;support.reconcile(access)
    for table in ['support_receipts','reward_ledger']:
        with pytest.raises(psycopg.errors.InsufficientPrivilege):
            with account.db.transaction() as c:c.execute('DELETE FROM trainer_backend.'+table)
    def revoked(player):
        account.revoke(access,user['session_id']);return valid
    verifier.membership=revoked
    with pytest.raises(AuthError):support.reconcile(access)
    with TestClient(create_app(Settings(postgres[1],'test',oauth=CONFIG),provider=Provider())) as client:
        assert client.post('/v1/me/support',json={'multiplier':3}).status_code==404
        assert client.post('/v1/me/progression/ra/sync',headers={'Authorization':'Bearer '+access},
            json={'web_key':'k'*32,'games':[1],'multiplier':3}).status_code==422
