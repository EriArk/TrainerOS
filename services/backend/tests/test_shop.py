from concurrent.futures import ThreadPoolExecutor
from uuid import uuid4
import psycopg
import pytest
from fastapi.testclient import TestClient
from trainer_backend.app import create_app
from trainer_backend.config import Settings
from trainer_backend.accounts.provider import AuthError
from trainer_backend.economy.shop import Shop
from test_accounts import account, login, CONFIG, Provider
from test_progression import linked


def funded(account):
    progress, user, _ = linked(account)
    progress.sync(user['access_token'], 'k'*32, [1])
    return Shop(progress), user


def test_purchase_ledger_inventory_restart_two_devices_and_owner_isolation(account):
    shop, user = funded(account)
    access = user['access_token'];operation = uuid4()
    result = shop.purchase(access, operation, 'profile.aurora', 1, 250)
    assert len(result['items']) == 1 and result['receipt']['price'] == 250
    progress = shop.progression.snapshot(access)
    assert progress['balance'] == 750 and progress['mastery']['xp'] == 2000
    assert progress['history'][0]['sparks'] == -250
    assert sum(r['sparks'] for r in progress['history']) == 750
    shop.equip(access, 'profile.aurora', result['revision'])
    second = login(account)
    restored = Shop(shop.progression).snapshot(second['access_token'], operation)
    assert restored['equipped'] == 'profile.aurora' and restored['receipt']['id'] == result['receipt']['id']
    replay = shop.purchase(second['access_token'], operation, 'profile.aurora', 1, 250)
    assert replay['receipt']['id'] == result['receipt']['id']
    assert shop.progression.snapshot(access)['balance'] == 750
    account.provider.subject='99999';other=login(account)
    assert shop.snapshot(other['access_token'])['items'] == []
    with pytest.raises(AuthError, match='not_found'):shop.snapshot(other['access_token'], operation)
    with pytest.raises(AuthError, match='not_owned'):shop.equip(other['access_token'], 'profile.aurora', 0)
    assert shop.equip(access, None, restored['revision'])['equipped'] is None


def test_replayed_conflicting_duplicate_stale_and_insufficient_purchases(account):
    shop, user = funded(account);access=user['access_token'];op=uuid4()
    shop.purchase(access, op, 'profile.aurora', 1, 250)
    for args, code in [((op,'profile.dusk',1,250),'identity_conflict'),((uuid4(),'profile.aurora',1,250),'already_owned'),
                       ((uuid4(),'profile.dusk',2,250),'offer_changed'),((uuid4(),'profile.dusk',1,1),'offer_changed')]:
        with pytest.raises(AuthError, match=code):shop.purchase(access,*args)
    account.provider.subject='99999';empty=login(account)
    with pytest.raises(AuthError, match='insufficient'):shop.purchase(empty['access_token'],uuid4(),'profile.dusk',1,250)
    assert shop.progression.snapshot(access)['balance'] == 750


def test_concurrent_purchase_and_atomic_rollback(account, monkeypatch):
    shop, user=funded(account);access=user['access_token'];op=uuid4()
    with ThreadPoolExecutor(2) as pool:results=list(pool.map(lambda _:shop.purchase(access,op,'profile.aurora',1,250),range(2)))
    assert results[0]['receipt']['id'] == results[1]['receipt']['id']
    import trainer_backend.economy.shop as module
    def fail(*args):raise RuntimeError('outbox failure')
    monkeypatch.setattr(module,'publish',fail)
    with pytest.raises(RuntimeError):shop.purchase(access,uuid4(),'profile.orbit',1,250)
    assert len(shop.snapshot(access)['items']) == 1
    assert shop.progression.snapshot(access)['balance'] == 750
    with account.db.transaction() as c:
        assert c.execute("SELECT count(*) n FROM trainer_backend.outbox o JOIN trainer_backend.event_index e ON e.id=o.event_id WHERE e.event_type='inventory.purchase_completed'").fetchone()['n'] == 1


def test_offer_retirement_and_mastery_revalidate_without_erasing_owned(account, postgres):
    shop,user=funded(account);access=user['access_token']
    result=shop.purchase(access,uuid4(),'profile.aurora',1,250)
    try:
        with psycopg.connect(postgres[0]) as c:
            c.execute("UPDATE trainer_backend.shop_offers SET available=false WHERE id='profile.aurora'")
            c.execute("UPDATE trainer_backend.shop_offers SET minimum_level=20 WHERE id='profile.orbit'")
        assert shop.equip(access,'profile.aurora',result['revision'])['equipped']=='profile.aurora'
        with pytest.raises(AuthError,match='level_required'):shop.purchase(access,uuid4(),'profile.orbit',1,250)
        with pytest.raises(AuthError,match='offer_unavailable'):shop.purchase(access,uuid4(),'profile.aurora',1,250)
        with pytest.raises(AuthError,match='inventory_changed'):shop.equip(access,None,result['revision'])
    finally:
        with psycopg.connect(postgres[0]) as c:c.execute('UPDATE trainer_backend.shop_offers SET available=true,minimum_level=1')


def test_competing_operations_cannot_double_buy_or_overspend(account, postgres):
    shop,user=funded(account);access=user['access_token']
    def buy(offer, price):
        try:
            shop.purchase(access,uuid4(),offer,1,price)
            return 'bought'
        except AuthError as error:
            return str(error)
    with ThreadPoolExecutor(2) as pool:
        results=list(pool.map(lambda _:buy('profile.aurora',250),range(2)))
    assert sorted(results)==['already_owned','bought']
    try:
        with psycopg.connect(postgres[0]) as c:
            c.execute("UPDATE trainer_backend.shop_offers SET price=600 WHERE id IN ('profile.dusk','profile.orbit')")
        with ThreadPoolExecutor(2) as pool:
            results=list(pool.map(lambda offer:buy(offer,600),['profile.dusk','profile.orbit']))
        assert sorted(results)==['bought','insufficient_sparks']
        result=shop.progression.snapshot(access)
        assert result['balance']==150 and result['mastery']['xp']==2000
        assert sum(row['sparks'] for row in result['history'])==150
        assert len(shop.snapshot(access)['items'])==2
    finally:
        with psycopg.connect(postgres[0]) as c:c.execute('UPDATE trainer_backend.shop_offers SET price=250')


def test_purchase_http_rejects_forgery_and_immutable_receipts(account, postgres):
    shop,user=funded(account);auth={'Authorization':'Bearer '+user['access_token']}
    with TestClient(create_app(Settings(postgres[1],'test',oauth=CONFIG),provider=Provider())) as client:
        assert len(client.get('/v1/shop/items').json()['catalogue'])==3
        assert client.get('/v1/me/inventory').status_code==401
        body={'operation_id':str(uuid4()),'offer_id':'profile.dusk','offer_revision':1,'expected_price':250}
        for extra in ({'player_id':str(user['player_id'])},{'sparks':999},{'expected_price':True}):
            assert client.post('/v1/me/purchases',headers=auth,json={**body,**extra}).status_code==422
        assert client.post('/v1/me/purchases',headers=auth,json=body).status_code==200
    for table in ['purchases','owned_items','shop_offers']:
        with pytest.raises(psycopg.errors.InsufficientPrivilege):
            with account.db.transaction() as c:c.execute('DELETE FROM trainer_backend.'+table)
    account.revoke(user['access_token'],user['session_id'])
    with pytest.raises(AuthError):shop.purchase(user['access_token'],uuid4(),'profile.orbit',1,250)
