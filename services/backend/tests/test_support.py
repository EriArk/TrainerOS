from uuid import uuid4
import pytest
from trainer_backend.accounts.provider import AuthError
from trainer_backend.economy.service import Progression
from trainer_backend.economy.shop import Shop
from trainer_backend.economy.support import preview, MAX_SAFE_INTEGER
from test_accounts import account, login
from test_progression import linked, release


def test_preview_uses_all_rewards_not_history_page_or_spendable_balance(account):
    service, user, proof = linked(account)
    access = user['access_token']
    service.sync(access, 'k'*32, [1, 2, 3])
    shop = Shop(service)
    shop.purchase(access, uuid4(), 'profile.aurora', 1, 250)
    before = service.snapshot(access)
    assert len(before['history']) == 50
    estimate = before['support_preview']
    assert estimate['state'] == 'calculation_only' and not estimate['activation_available']
    assert estimate['base_sparks'] == 3000 and estimate['eligible_rewards'] == 93
    assert estimate['credited_reward_sparks'] == 3000
    assert [(o['multiplier'], o['top_up'], o['potential_balance']) for o in estimate['options']] == [
        (2, 3000, 5750), (3, 6000, 8750)]
    second = login(account)
    restarted = Progression(account.db, account, proof)
    for _ in range(3):
        after = restarted.snapshot(second['access_token'])
        assert after['support_preview'] == estimate
        assert after['revision'] == before['revision'] and after['balance'] == 2750
        assert after['mastery']['xp'] == 6000
    assert len(shop.snapshot(access)['items']) == 1
    release(account.db)
    assert service.sync(access, 'k'*32, [1, 2, 3])['support_preview'] == estimate
    account.revoke(access, user['session_id'])
    with pytest.raises(AuthError):
        service.snapshot(access)
    account.provider.subject = '99999'
    other = service.snapshot(login(account)['access_token'])
    assert other['support_preview']['eligible_rewards'] == 0
    assert all(o['top_up'] == o['potential_balance'] == 0 for o in other['support_preview']['options'])


def test_preview_keeps_spent_rewards_eligible_and_does_not_mint(account):
    service, user, _ = linked(account)
    access = user['access_token']
    first = service.sync(access, 'k'*32, [1])
    Shop(service).purchase(access, uuid4(), 'profile.orbit', 1, 250)
    after = service.snapshot(access)
    assert first['support_preview']['options'][1]['potential_balance'] == 3000
    assert after['balance'] == 750
    assert [(o['top_up'], o['potential_balance']) for o in after['support_preview']['options']] == [(1000, 1750), (2000, 2750)]
    with account.db.transaction() as c:
        assert c.execute('SELECT count(*) n, sum(sparks) total FROM trainer_backend.reward_ledger').fetchone() == {'n': 32, 'total': 750}


def test_preview_fails_closed_outside_client_numeric_range():
    class Connection:
        def execute(self, *_): return self
        def fetchone(self): return self.totals
    conn = Connection()
    conn.totals = dict(rewards=1, base=1000, credited=1000, topup2=1000, topup3=2000, invalid=0)
    assert preview(conn, 'unused', MAX_SAFE_INTEGER-1000) is None
    conn.totals['invalid'] = 1
    assert preview(conn, 'unused', 0) is None
    conn.totals.update(invalid=0, base=MAX_SAFE_INTEGER+1)
    assert preview(conn, 'unused', 0) is None
