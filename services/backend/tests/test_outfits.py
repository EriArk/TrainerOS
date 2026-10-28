# SPDX-License-Identifier: GPL-3.0-or-later
from concurrent.futures import ThreadPoolExecutor
from copy import deepcopy
from uuid import uuid4
import psycopg
import pytest
from fastapi.testclient import TestClient
from trainer_backend.accounts.provider import AuthError
from trainer_backend.app import create_app
from trainer_backend.config import Settings
from trainer_backend.economy import outfits
from test_accounts import account, login, CONFIG, Provider
from test_shop import funded
from test_profiles import RECIPE


def test_slots_purchase_save_restart_restore_and_scene_separation(account):
    shop,user=funded(account);access=user['access_token'];op=uuid4()
    bought=shop.purchase(access,op,'outfit.slot.1',1,100)
    assert bought['items']==[] and len(bought['outfit_items'])==1 and bought['outfits']==[]
    assert shop.progression.snapshot(access)['balance']==900
    saved=outfits.save(shop,access,'outfit.slot.1','Golf',RECIPE,0)
    assert saved['outfits'][0]['revision']==1 and saved['outfits'][0]['recipe']==RECIPE
    retried=outfits.save(shop,access,'outfit.slot.1','Golf',RECIPE,0)
    assert retried['revision']==saved['revision']
    other_session=login(account)['access_token']
    restored=shop.snapshot(other_session)
    assert restored['outfits']==saved['outfits']
    assert shop.purchase(access,op,'outfit.slot.1',1,100)['receipt']['id']==bought['receipt']['id']
    assert shop.progression.snapshot(access)['balance']==900
    with pytest.raises(AuthError,match='purpose'):shop.equip(access,'outfit.slot.1',restored['revision'])
    scene=shop.purchase(access,uuid4(),'profile.aurora',1,250)
    assert len(scene['items'])==1 and len(scene['outfit_items'])==1
    with pytest.raises(AuthError,match='not_owned'):outfits.save(shop,access,'profile.aurora','Bad',RECIPE,0)
    assert shop.equip(access,'profile.aurora',scene['revision'])['equipped']=='profile.aurora'


def test_stale_save_and_concurrent_edit_do_not_overwrite(account):
    shop,user=funded(account);access=user['access_token']
    shop.purchase(access,uuid4(),'outfit.slot.1',1,100)
    outfits.save(shop,access,'outfit.slot.1','First',RECIPE,0)
    def edit(name):
        try:outfits.save(shop,access,'outfit.slot.1',name,RECIPE,1);return 'saved'
        except AuthError as e:return str(e)
    with ThreadPoolExecutor(2) as pool:result=list(pool.map(edit,['Second','Third']))
    assert result.count('saved')==1 and any('outfit_changed' in r for r in result)
    row=shop.snapshot(access)['outfits'][0]
    assert row['revision']==2
    altered=deepcopy(RECIPE);altered['appearance']['hair']='crop'
    with pytest.raises(AuthError,match='changed'):outfits.save(shop,access,'outfit.slot.1','Old',altered,1)
    assert shop.snapshot(access)['outfits'][0]==row


def test_unowned_owner_changed_revoked_and_invalid_recipe(account):
    shop,user=funded(account);access=user['access_token']
    with pytest.raises(AuthError,match='not_owned'):outfits.save(shop,access,'outfit.slot.1','Golf',RECIPE,0)
    shop.purchase(access,uuid4(),'outfit.slot.1',1,100)
    for name,recipe in [('',RECIPE),('line\nline',RECIPE),('a'*33,RECIPE),('Golf',None),('Golf',{'path':'/secret'})]:
        with pytest.raises(AuthError):outfits.save(shop,access,'outfit.slot.1',name,recipe,0)
    account.provider.subject='99999';other=login(account)
    assert shop.snapshot(other['access_token'])['outfits']==[]
    with pytest.raises(AuthError,match='not_owned'):outfits.save(shop,other['access_token'],'outfit.slot.1','Golf',RECIPE,0)
    account.revoke(access,user['session_id'])
    with pytest.raises(AuthError):outfits.save(shop,access,'outfit.slot.1','Golf',RECIPE,0)


def test_http_contract_no_public_writes_or_client_ownership(account, postgres):
    shop,user=funded(account);access=user['access_token']
    shop.purchase(access,uuid4(),'outfit.slot.1',1,100)
    with TestClient(create_app(Settings(postgres[1],'test',oauth=CONFIG),provider=Provider())) as client:
        path='/v1/me/inventory/outfits/outfit.slot.1';body={'name':'Golf','recipe':RECIPE,'revision':'0'}
        assert client.put(path,json=body).status_code==401
        headers={'Authorization':'Bearer '+access}
        assert client.put(path,headers=headers,json={**body,'owned':True}).status_code==422
        response=client.put(path,headers=headers,json=body)
        assert response.status_code==200
        assert response.json()['outfits'][0]['revision']=='1'
        assert all(o['purpose']=='profile_scene' for o in client.get('/v1/shop/items').json()['catalogue'])
