# SPDX-License-Identifier: GPL-3.0-or-later
import copy
import json
import psycopg
import pytest
from fastapi.testclient import TestClient
from trainer_backend.accounts.profiles import avatar_recipe,display_name,update_name
from trainer_backend.accounts.provider import AuthError
from trainer_backend.app import create_app
from trainer_backend.config import Settings
from test_accounts import CONFIG,Provider,login,account
from test_play import play,players,match,card,call,drain,RULE

RECIPE={'schema':1,'packRevision':'a'*64,'appearance':{
    'schema':1,'pack':'trainer-avatar-1','hair':'bob','eyes':'round',
    'skinColor':'#f0c4a5','hairColor':'#62402c','eyeColor':'#98602d',
    'shirtColor':'#ec9e8d','shortsColor':'#577c87'}}


def test_names_are_normalized_bounded_and_never_identity_keys():
    assert display_name({'global_name':' A\n\u202eB\0  C '})=='AB C'
    assert display_name({'global_name':None,'username':'Alice'})=='Alice'
    assert len(display_name({'global_name':'x'*200}))==64
    assert display_name({'email':'private@example.org'})=='Player'


@pytest.mark.parametrize('mutation',[
    lambda r:r.update(url='https://example.org/evil.glb'),
    lambda r:r.update(schema=True),
    lambda r:r.update(packRevision='../private'),
    lambda r:r['appearance'].update(pack='paid-pack'),
    lambda r:r['appearance'].update(hair='unknown'),
    lambda r:r['appearance'].update(shirtColor='url(secret)'),
    lambda r:r['appearance'].update(glb='/private'),
])
def test_avatar_is_bounded_free_pack_recipe(mutation):
    value=copy.deepcopy(RECIPE);mutation(value)
    with pytest.raises(AuthError,match='invalid_avatar_recipe'):avatar_recipe(value)


@pytest.mark.parametrize('style',['rogue','innocent','sly','calm','sleepy','determined'])
@pytest.mark.parametrize('variant',['base','feminine'])
def test_approved_eye_variants_preserve_recipe_contract(style,variant):
    value=copy.deepcopy(RECIPE);value['appearance']['eyes']=f'{style}-{variant}'
    assert avatar_recipe(value)==value
    modern=copy.deepcopy(value)
    modern['appearance'].update(schema=3,socksColor='#f4f0df',shoesColor='#234f43',headwearColor='#efa58b')
    modern['appearance'].update(top='tank',bottom='base-shorts',socks='none',shoes='sandals',headwear='none')
    assert avatar_recipe(modern)==modern
    for key in ('socksColor','shoesColor','headwearColor'):
        broken=copy.deepcopy(modern);broken['appearance'][key]='url(external)'
        with pytest.raises(AuthError,match='invalid_avatar_recipe'):avatar_recipe(broken)
        del broken['appearance'][key]
        with pytest.raises(AuthError,match='invalid_avatar_recipe'):avatar_recipe(broken)
    value['appearance']['eyes']+='-unknown'
    with pytest.raises(AuthError,match='invalid_avatar_recipe'):avatar_recipe(value)


def test_own_avatar_update_retry_clear_and_revocation(account,postgres):
    user=login(account);token=user['access_token']
    first=account.profile(token,avatar=RECIPE)
    assert account.profile(token,avatar=RECIPE)==first
    assert set(first)=={'player_id','display_name','avatar','revision'}
    assert account.profile(token,avatar=None)['avatar'] is None
    assert account.profile(token)['revision']==first['revision']+1
    account.revoke(token,user['session_id'])
    with pytest.raises(AuthError):account.profile(token,avatar=RECIPE)


def test_profile_http_does_not_accept_target_player_or_name(postgres,db):
    with TestClient(create_app(Settings(postgres[1],'test',oauth=CONFIG),provider=Provider())) as client:
        from trainer_backend.accounts.service import Accounts
        user=login(Accounts(db,CONFIG,Provider()));auth={'Authorization':'Bearer '+user['access_token']}
        assert client.put('/v1/me/profile/avatar',json={'avatar':RECIPE}).status_code==401
        assert client.put('/v1/me/profile/avatar',headers=auth,json={'avatar':RECIPE,'display_name':'Forged'}).status_code==422
        value=client.put('/v1/me/profile/avatar',headers=auth,json={'avatar':RECIPE})
        assert value.status_code==200 and isinstance(value.json()['revision'],str)
        assert client.get('/v1/me/profile',headers=auth).json()==value.json()


def test_board_profile_updates_keep_score_revision_and_mask_inactive(play,postgres):
    people=players(play,5);key=match(play,people,[2,3])
    for p in people:call(play,p,'submit',card(key))
    drain(play);before=play.board(RULE,'teams','records',0,5)
    player=people[0][0]['player_id']
    play.accounts.profile(people[0][0]['access_token'],avatar=RECIPE)
    with play.db.transaction() as conn:update_name(conn,player,'New name')
    after=play.board(RULE,'teams','records',0,5,before['revision'])
    assert after['items']==before['items'] and len(after['profiles'])==5
    assert after['profiles'][str(player)]['display_name']=='New name'
    assert after['profiles'][str(player)]['avatar']==RECIPE
    assert 'fluxer' not in json.dumps(after,default=str) and 'token' not in json.dumps(after,default=str)
    with psycopg.connect(postgres[0]) as conn:conn.execute("UPDATE trainer_backend.principals SET state='suspended' WHERE id=%s",(player,))
    hidden=play.board(RULE,'teams','records',0,5)['profiles'][str(player)]
    assert hidden['display_name']=='Player' and hidden['avatar'] is None
    assert play.board(RULE,'teams','records',0,5)['items']==before['items']


def test_wardrobe_recipe_roundtrip_and_legacy_are_bounded(account):
    value=copy.deepcopy(RECIPE)
    value['appearance'].update(schema=2,top='polo-short-sleeve',bottom='golf-shorts',
                               socks='ankle-socks',shoes='golf-shoes-classic',headwear='golf-cap')
    assert avatar_recipe(value)==value
    user=login(account);token=user['access_token']
    assert account.profile(token,avatar=value)['avatar']==value
    assert account.profile(token,avatar=RECIPE)['avatar']==RECIPE
    for key in ('top','bottom','socks','shoes','headwear'):
        bad=copy.deepcopy(value);bad['appearance'][key]='../external.glb'
        with pytest.raises(AuthError,match='invalid_avatar_recipe'):avatar_recipe(bad)
        del bad['appearance'][key]
        with pytest.raises(AuthError,match='invalid_avatar_recipe'):avatar_recipe(bad)
    bad=copy.deepcopy(value);bad['appearance']['schema']=1
    with pytest.raises(AuthError,match='invalid_avatar_recipe'):avatar_recipe(bad)
