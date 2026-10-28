from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timedelta, timezone
import copy
import pytest
import psycopg
from fastapi.testclient import TestClient
from trainer_backend.app import create_app
from trainer_backend.config import Settings
from trainer_backend.accounts.provider import AuthError
from trainer_backend.economy.service import Progression
from trainer_backend.economy.policy import achievement, completion, mastery
from trainer_backend.economy.provider import RetroAchievements
from test_accounts import account, login, CONFIG, Provider

RA = '01K00000000000000000000001'


class Evidence:
    motto = ''
    eligible = True
    when = datetime.now(timezone.utc)
    def awards(self, key, user):
        return []
    def profile(self, key, user):
        return {'id': RA, 'name': 'Tester', 'motto': self.motto, 'eligible': self.eligible}
    def game(self, key, user, game):
        return {'id': game, 'title': 'Verified game', 'achievements': [
            {'id': str(game * 100 + i), 'title': 'Goal', 'points': 5, 'earned': self.when} for i in range(30)]}


def release(db):
    with db.transaction() as c:
        c.execute('UPDATE trainer_backend.progression_accounts SET next_check_at=NULL')


def linked(account):
    proof = Evidence()
    service = Progression(account.db, account, proof)
    user = login(account)
    token = user['access_token']
    pending = service.challenge(token, 'Tester', 'k' * 32)
    proof.motto = pending['challenge']['challenge']
    release(account.db)
    service.verify(token, 'k' * 32)
    proof.when = datetime.now(timezone.utc)
    release(account.db)
    return service, user, proof


def test_policy_first_month_and_level_boundaries():
    assert achievement(5) == (30, 15)
    assert achievement(10) == (50, 25)
    assert completion(150) == (1100, 550)
    for level in range(1, 1000):
        floor = 50 * (level - 1) * (level + 4)
        assert mastery(floor)['level'] == level
        assert mastery(floor)['level_xp'] == 0
        if floor:
            assert mastery(floor - 1)['level'] == level - 1
    assert mastery(2000)['level'] == 5


def test_official_web_payload_is_validated_without_caching_trigger_data():
    provider = RetroAchievements()
    data = {'ID': 1, 'ConsoleID': 1, 'NumAchievements': 1, 'NumAwardedToUser': 1,
            'Title': 'A game', 'Achievements': {'9': {'ID': 9, 'Title': 'A goal', 'Points': 5,
            'DateEarned': '2026-10-28 12:00:00', 'DateEarnedHardcore': '2026-10-28 13:00:00', 'MemAddr': 'not-stored'}}}
    provider.request = lambda *args, **kwargs: copy.deepcopy(data)
    result = provider.game('k'*32, RA, 1)
    assert result['achievements'][0]['earned'].hour == 12
    assert 'MemAddr' not in str(result)
    data['Achievements']['9']['DateEarned'] = 'invalid'
    with pytest.raises(AuthError, match='ra_invalid_date'):
        provider.game('k'*32, RA, 1)
    data['Achievements'] = {};data['NumAchievements'] = 0;data['NumAwardedToUser'] = 0
    assert provider.game('k'*32, RA, 1)['achievements'] == []
    data['NumAchievements'] = 30
    with pytest.raises(AuthError, match='ra_incomplete_game'):
        provider.game('k'*32, RA, 1)


def test_real_database_reward_replay_reopen_and_two_devices(account):
    service, user, proof = linked(account)
    first = service.sync(user['access_token'], 'k' * 32, [1])
    assert first['balance'] == 1000 and first['mastery']['xp'] == 2000
    assert len(first['history']) == 31
    second_device = login(account)
    service = Progression(account.db, account, proof)
    release(account.db)
    repeat = service.sync(second_device['access_token'], 'k' * 32, [1, 1])
    assert repeat['revision'] == first['revision'] and repeat['balance'] == 1000
    assert repeat['player_id'] == first['player_id']
    account.revoke(user['access_token'], user['session_id'])
    with pytest.raises(AuthError):
        service.snapshot(user['access_token'])
    assert service.snapshot(second_device['access_token'])['balance'] == 1000


def test_initial_history_is_not_paid_and_second_account_cannot_claim_link(account):
    service, user, proof = linked(account)
    proof.when = datetime.now(timezone.utc) - timedelta(days=100)
    result = service.sync(user['access_token'], 'k' * 32, [1])
    assert result['balance'] == 0 and result['mastery']['xp'] == 0
    account.provider.subject = '99999'
    other = login(account)
    assert service.snapshot(other['access_token'])['balance'] == 0
    with pytest.raises(AuthError, match='ra_already_linked'):
        service.challenge(other['access_token'], 'Tester', 'k' * 32)
    assert service.snapshot(other['access_token'])['history'] == []


def test_link_requires_live_owner_proof_and_does_not_store_credentials(account):
    proof = Evidence()
    service = Progression(account.db, account, proof)
    user = login(account)
    service.challenge(user['access_token'], 'Tester', 'k' * 32)
    release(account.db)
    with pytest.raises(AuthError, match='ra_challenge_missing'):
        service.verify(user['access_token'], 'k' * 32)
    assert service.snapshot(user['access_token'])['link'] is None
    assert 'kkkk' not in str(service.snapshot(user['access_token']))


def test_ineligible_provider_and_revocation_during_network_do_not_mint(account):
    service, user, proof = linked(account)
    proof.eligible = False
    with pytest.raises(AuthError, match='ineligible'):
        service.sync(user['access_token'], 'k' * 32, [1])
    proof.eligible = True
    original = proof.game
    def revoked(*args):
        result = original(*args)
        account.revoke(user['access_token'], user['session_id'])
        return result
    proof.game = revoked
    release(account.db)
    with pytest.raises(AuthError):
        service.sync(user['access_token'], 'k' * 32, [1])
    with account.db.transaction() as c:
        assert c.execute('SELECT count(*) n FROM trainer_backend.reward_ledger').fetchone()['n'] == 0


def test_concurrent_retry_and_transaction_rollback(account):
    service, user, proof = linked(account)
    # Bypass only the rate limiter to exercise actual competing reward transactions.
    service.reserve_check = lambda token: None
    with ThreadPoolExecutor(2) as pool:
        results = list(pool.map(lambda _: service.sync(user['access_token'], 'k'*32, [1]), range(2)))
    assert all(r['balance'] == 1000 for r in results)
    original = service.grant
    def failure(*args):
        original(*args)
        raise RuntimeError('transaction interrupted')
    service.grant = failure
    with pytest.raises(RuntimeError):
        service.sync(user['access_token'], 'k'*32, [2])
    assert service.snapshot(user['access_token'])['balance'] == 1000


def test_ledger_is_append_only_for_runtime_role(account):
    service, user, proof = linked(account)
    service.sync(user['access_token'], 'k'*32, [1])
    for sql in ('UPDATE trainer_backend.reward_ledger SET sparks=0', 'DELETE FROM trainer_backend.reward_ledger'):
        with pytest.raises(psycopg.errors.InsufficientPrivilege):
            with account.db.transaction() as c:
                c.execute(sql)


def test_http_rejects_forged_amounts_and_crates_events(postgres, db, account):
    user = login(account)
    auth = {'Authorization': 'Bearer ' + user['access_token']}
    with TestClient(create_app(Settings(postgres[1], 'test', oauth=CONFIG), provider=Provider(), reward_provider=Evidence())) as client:
        assert client.get('/v1/me/progression').status_code == 401
        value = client.get('/v1/me/progression', headers=auth)
        assert value.status_code == 200 and value.json()['balance'] == 0
        for extra in ({'sparks': 1000}, {'xp': 1000}, {'source': 'crates.win'}, {'player_id': str(user['player_id'])}):
            assert client.post('/v1/me/progression/ra/sync', headers=auth,
                json={'web_key': 'k'*32, 'games': [1], **extra}).status_code == 422
        assert client.post('/v1/me/progression/ra/sync', headers=auth, json={'web_key': 'k'*32, 'games': [True]}).status_code == 422
