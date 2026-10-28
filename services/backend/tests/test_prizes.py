from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timedelta, timezone
from uuid import uuid4
import copy
import pytest
import psycopg
from fastapi.testclient import TestClient
from trainer_backend.app import create_app
from trainer_backend.config import Settings
from trainer_backend.accounts.provider import AuthError
from trainer_backend.economy.prizes import Prizes, grant
from trainer_backend.economy.provider import RetroAchievements
from trainer_backend.economy.shop import Shop
from trainer_backend.economy import outfits
from test_accounts import account, login, CONFIG, Provider
from test_progression import linked, release, RA, Evidence
from test_profiles import RECIPE


def test_collection_pages_whole_games_and_levels(account):
    service, user, proof = linked(account); token = user['access_token']
    prizes = Prizes(service); now = datetime.now(timezone.utc)
    with account.db.transaction() as conn:
        player = service.account(conn, token).player_id
        for game in range(1, 104):
            grant(conn, player, f'test:{game}:beaten', 'beaten', f'Game {game}', now, game=game)
        # A later mastery upgrades the old game's single collection card.
        grant(conn, player, 'test:1:completed', 'completed', 'Game 1', now, game=1)
        grant(conn, player, 'test:1:hardcore', 'hardcore', 'Game 1', now, game=1)
    first = prizes.snapshot(token)
    assert len(first['collection']) == 100
    assert first['collection'][0]['key'] == 'game:1'
    assert [g['kind'] for g in first['collection'][0]['grants']] == ['beaten', 'completed', 'hardcore']
    assert len([g for g in first['waiting'] if g['game_id'] == 1]) == 3
    assert len(first['waiting']) == 100
    second = prizes.snapshot(token, group_before=int(first['collection_before']))
    assert len(second['collection']) == 3 and second['collection_before'] is None
    assert len({g['key'] for page in (first, second) for g in page['collection']}) == 103
    account.provider.subject = '99999'; other = login(account)['access_token']
    assert prizes.snapshot(other)['collection'] == []


def test_levels_restore_free_slots_and_do_not_replace_purchases(account):
    service, user, proof = linked(account)
    service.sync(user['access_token'], 'k'*32, [1])  # level 5
    token = user['access_token']; prizes = Prizes(service); shop = Shop(service)
    result = prizes.snapshot(token)
    assert [r['level'] for r in result['items'] if r['kind'] == 'clothing'] == [5, 4, 3, 2]
    assert result['pending'] == 5
    assert result['upcoming'][0] == {'level': 6, 'slot': True, 'clothing': True}
    slots = shop.snapshot(token)['outfit_items']
    assert len(slots) == 1 and slots[0]['id'] == 'outfit.level.3' and slots[0]['price'] == 0
    bought = shop.purchase(token, uuid4(), 'outfit.slot.1', 1, 100)
    assert len(bought['outfit_items']) == 2
    saved = outfits.save(shop, token, 'outfit.level.3', 'Earned look', RECIPE, 0)
    assert saved['outfits'][0]['name'] == 'Earned look'
    with ThreadPoolExecutor(2) as pool:
        snapshots = list(pool.map(lambda _: Prizes(service).snapshot(token), range(2)))
    assert all(len(s['items']) == 5 for s in snapshots)
    assert service.snapshot(token)['balance'] == 900


def test_historical_completion_mode_upgrade_and_explicit_shelf(account):
    service, user, proof = linked(account); token = user['access_token']
    old = datetime.now(timezone.utc) - timedelta(days=100)
    proof.when = old
    award = {'game': 7, 'kind': 'beaten', 'title': 'An old adventure', 'earned': old}
    proof.awards = lambda *args: [award]
    service.sync(token, 'k'*32, []); prizes = Prizes(service)
    result = prizes.snapshot(token)
    assert result['pending'] == 1 and result['trophies'][0]['beaten']
    assert service.snapshot(token)['balance'] == 0
    assert prizes.shelf(token, [7])['shelf'][0]['game_id'] == 7
    with pytest.raises(AuthError, match='trophy_not_owned'):
        prizes.shelf(token, [8])
    with pytest.raises(AuthError, match='invalid_shelf'):
        prizes.shelf(token, [7, 7])
    prizes.acknowledge(token, [result['items'][0]['id']])
    assert prizes.snapshot(token)['pending'] == 0
    proof.awards = lambda *args: [award, {**award, 'kind': 'hardcore'}, {**award, 'kind': 'completed'}]
    release(account.db);service.sync(token, 'k'*32, [1])
    result = prizes.snapshot(token)
    assert result['pending'] == 2 and len(result['trophies']) == 1
    assert result['shelf'][0]['hardcore'] and result['shelf'][0]['completed']
    proof.awards = lambda *args: []  # hidden upstream afterwards, no revocation
    release(account.db);service.sync(token, 'k'*32, [1])
    assert len(prizes.snapshot(token)['items']) == 3
    assert service.snapshot(token)['mastery']['xp'] == 0


def test_owned_grants_are_append_only_and_seen_is_owner_scoped(account):
    service, user, proof = linked(account); token = user['access_token']
    service.sync(token, 'k'*32, [1]); prizes = Prizes(service)
    ids = [r['id'] for r in prizes.snapshot(token)['items']]
    account.provider.subject = '99999'; other = login(account)['access_token']
    assert prizes.acknowledge(other, ids)['pending'] == 0
    assert prizes.snapshot(token)['pending'] == len(ids)
    with pytest.raises(AuthError, match='outfit_slot_not_owned'):
        outfits.save(Shop(service), other, 'outfit.level.3', 'Other', RECIPE, 0)
    for statement in ('DELETE FROM trainer_backend.prize_grants', 'UPDATE trainer_backend.prize_grants SET title=\'Forged\''):
        with pytest.raises(psycopg.errors.InsufficientPrivilege):
            with account.db.transaction() as c:c.execute(statement)


def test_official_beaten_and_typed_hidden_progress():
    provider = RetroAchievements()
    data = {'VisibleUserAwards': [{'AwardType': 'Game Beaten', 'AwardData': 7, 'AwardDataExtra': 1,
            'AwardedAt': '2026-01-01T00:00:00Z', 'Title': 'Old game', 'ConsoleID': 1}]}
    provider.request = lambda *a, **k: copy.deepcopy(data)
    assert [a['kind'] for a in provider.awards('secret', RA)] == ['beaten', 'hardcore']
    data['VisibleUserAwards'][0]['AwardData'] = True
    with pytest.raises(AuthError):provider.awards('secret', RA)
    data = {'ID': 1, 'ConsoleID': 1, 'ParentGameID': None, 'NumAchievements': 3, 'NumAwardedToUser': 2,
        'Achievements': {'1': {'ID': 1, 'Points': 5, 'type': 'progression', 'DateEarned': '2026-01-01'},
                         '2': {'ID': 2, 'Points': 5, 'type': 'win_condition', 'DateEarned': '2026-01-02'},
                         '3': {'ID': 3, 'Points': 5, 'type': 'missable'}}}
    assert [a['kind'] for a in provider.game('secret', RA, 1)['awards']] == ['beaten']
    data['ParentGameID'] = 99
    assert provider.game('secret', RA, 1)['awards'] == []
    data['ParentGameID'] = None;data['Achievements']['2']['type'] = 'missable'
    del data['Achievements']['1']['type']
    assert provider.game('secret', RA, 1)['awards'] == []  # no vacuous win
    for row in data['Achievements'].values():row['DateEarnedHardcore'] = '2026-01-03'
    data['NumAwardedToUser'] = 3
    assert [a['kind'] for a in provider.game('secret', RA, 1)['awards']] == ['completed', 'hardcore']


def test_prize_http_rejects_client_grants_and_requires_auth(postgres, db, account):
    user = login(account);auth = {'Authorization': 'Bearer '+user['access_token']}
    with TestClient(create_app(Settings(postgres[1], 'test', oauth=CONFIG), provider=Provider(), reward_provider=Evidence())) as client:
        assert client.get('/v1/me/prizes').status_code == 401
        assert client.get('/v1/me/prizes', headers=auth).json()['pending'] == 0
        assert client.post('/v1/me/prizes', headers=auth, json={'level': 99}).status_code == 405
        assert client.post('/v1/me/prizes/seen', headers=auth, json={'ids': [str(uuid4())]}).status_code == 200
        assert client.put('/v1/me/prizes/shelf', headers=auth, json={'games': [True]}).status_code == 422
