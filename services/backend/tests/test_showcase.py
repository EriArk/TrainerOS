from datetime import datetime, timezone
from uuid import UUID, uuid4
import pytest
import psycopg
from fastapi.testclient import TestClient
from trainer_backend.app import create_app
from trainer_backend.config import Settings
from trainer_backend.accounts.showcase import Showcase, DEFAULTS
from trainer_backend.accounts.provider import AuthError
from trainer_backend.economy.prizes import grant
from test_accounts import account, login, CONFIG, Provider
from test_profiles import RECIPE


def test_projection_privacy_self_preview_identity_and_no_economy_leaks(account, postgres):
    owner = login(account); token = owner['access_token']; player = UUID(str(owner['player_id']))
    subject = account.provider.subject
    account.profile(token, avatar=RECIPE)
    view = Showcase(account)
    with account.db.transaction() as conn:
        conn.execute('INSERT INTO trainer_backend.progression_accounts(player_id,xp,balance) VALUES(%s,2000,7654)', (player,))
        grant(conn, player, 'test:beaten', 'beaten', 'Verified game', datetime.now(timezone.utc), game=42)
        conn.execute('INSERT INTO trainer_backend.prize_shelf VALUES(%s,0,42)', (player,))
    own = view.read(token, player=player)
    assert own['level']>1 and own['shelf'][0]['game_id']==42 and 'history' not in own
    assert set(own)=={'player_id','subject','display_name','avatar','appearance','visibility','level','shelf'}
    assert own['avatar']==RECIPE
    account.provider.subject='987654321'; guest=login(account)['access_token']
    assert view.read(guest, subject=subject)==own
    settings=view.sharing(token)
    hidden=dict.fromkeys(DEFAULTS,False)
    view.sharing(token,hidden,settings['revision'])
    other=view.read(guest,player=player)
    assert other==view.read(token,player=player)  # Self preview cannot bypass privacy.
    assert not {'level','shelf','history','balance','xp','outfits','purchases'} & other.keys()
    with pytest.raises(AuthError,match='sharing_changed'):
        view.sharing(token,DEFAULTS,settings['revision'])
    assert view.sharing(guest)['level']  # Cannot mutate another account.
    with pytest.raises(AuthError):view.read('invalid',player=player)
    with pytest.raises(AuthError,match='profile_not_found'):view.read(guest,subject='999999')
    with psycopg.connect(postgres[0]) as conn:
        nonplayer=uuid4()
        conn.execute("INSERT INTO trainer_backend.principals(id,kind,state) VALUES(%s,'adapter','active')",(nonplayer,))
        conn.execute("UPDATE trainer_backend.principals SET state='suspended' WHERE id=%s",(player,))
    with pytest.raises(AuthError,match='profile_not_found'):view.read(guest,player=nonplayer)
    with pytest.raises(AuthError,match='profile_not_found'):view.read(guest,player=player)


def test_history_pages_whole_games_and_revocation_applies_to_every_page(account):
    owner=login(account); token=owner['access_token']; player=UUID(str(owner['player_id'])); view=Showcase(account)
    with account.db.transaction() as conn:
        for game in range(1,28):
            grant(conn,player,f'test:{game}','beaten',f'Game {game}',datetime.now(timezone.utc),game=game)
        grant(conn,player,'test:1:mastery','completed','Game 1',datetime.now(timezone.utc),game=1)
    view.sharing(token,{**DEFAULTS,'history':True},0)
    first=view.read(token,player=player)
    assert len(first['history'])==24 and first['history'][0]['completed']
    second=view.read(token,player=player,before=int(first['next']))
    assert len(second['history'])==3 and second['next'] is None
    assert len({r['game_id'] for p in (first,second) for r in p['history']})==27
    view.sharing(token,DEFAULTS,1)
    assert 'history' not in view.read(token,player=player,before=int(first['next']))


def test_favorites_are_explicit_presentation_and_respect_privacy_and_revision(account):
    user=login(account); token=user['access_token']; player=UUID(str(user['player_id'])); view=Showcase(account)
    view.sharing(token)
    result=view.favorites(token,['My favorite adventure','Night racers'],0)
    assert result['revision']==1 and result['favorites']
    assert view.read(token,player=player)['favorites']==['My favorite adventure','Night racers']
    with pytest.raises(AuthError,match='sharing_changed'):view.favorites(token,['Stale device'],0)
    with pytest.raises(AuthError,match='invalid_favorites'):view.favorites(token,['No\ncontrol characters'],1)
    with pytest.raises(AuthError,match='invalid_favorites'):view.favorites(token,['Duplicate','Duplicate'],1)
    view.sharing(token,DEFAULTS,1)
    assert 'favorites' not in view.read(token,player=player)


def test_profile_http_authentication_strict_writes_and_projection(account, postgres):
    owner=login(account); headers={'Authorization':'Bearer '+owner['access_token']}
    with TestClient(create_app(Settings(postgres[1],'test',oauth=CONFIG),provider=Provider())) as client:
        path='/v1/profiles/'+str(owner['player_id'])
        assert client.get(path).status_code==401
        response=client.get(path,headers=headers)
        assert response.status_code==200 and response.headers['cache-control']=='no-store'
        assert 'balance' not in response.json()
        sharing=client.get('/v1/me/profile/sharing',headers=headers).json()
        assert client.put('/v1/me/profile/sharing',headers=headers,json={**sharing,'player_id':str(uuid4())}).status_code==422
        assert client.put('/v1/me/profile/sharing',headers=headers,json={**sharing,'level':'false'}).status_code==422
        assert client.put('/v1/me/profile/favorites',headers=headers,json={'revision':'0','games':['A game'],'balance':999}).status_code==422
