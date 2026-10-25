# SPDX-License-Identifier: GPL-3.0-or-later
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from uuid import uuid4
import json
import psycopg
import pytest
from fastapi.testclient import TestClient
from trainer_backend.accounts.service import Accounts
from trainer_backend.accounts.provider import AuthError
from trainer_backend.app import create_app
from trainer_backend.config import Settings
from trainer_backend.jobs import Worker
from trainer_backend.play.service import Play,verify_result,decision
from trainer_backend.play.registry import register,void_match
from test_accounts import CONFIG,Provider,login

RULE='traineros.scorecard-demo.v1'

@pytest.fixture
def play(postgres,db,tmp_path):
    path=tmp_path/'rule.json'
    data={'id':RULE,'game_id':'org.traineros.scorecard-demo','content_id':'three-segment-demo-v1',
          'definition':{'validator':'scorecard.v1','segments':3,'minimum':1,'maximum':99,'direction':'asc','trust':'peer_confirmed','label':'Test'}}
    path.write_text(json.dumps(data))
    register(postgres[0],path)
    service=Play(db,Accounts(db,CONFIG,Provider()))
    service.fixture=(path,data)
    return service

def players(play,n=2):
    result=[]
    for i in range(n):
        play.accounts.provider.subject=str(1000+i)
        account=login(play.accounts)
        grant=play.grant(account['access_token'],{'app_id':'org.test.game','ruleset_id':RULE,'operation_id':str(uuid4())})
        result.append((account,grant['grant_token']))
    return result

def call(play,player,action,payload=None,operation=None):
    return play.execute(player[1],{'action':action,'payload':payload or {},'operation_id':str(operation or uuid4())})

def match(play,people,seats=None):
    seats=seats or [1]*len(people)
    key=str(call(play,people[0],'create',{'mode':'teams' if max(seats)>1 else 'individual','seats':seats})['match']['id'])
    side_list=[side for side,n in enumerate(seats) for _ in range(n)]
    for person,side in zip(people[1:],side_list[1:]): call(play,person,'join',{'match_id':key,'side':side})
    call(play,people[0],'start',{'match_id':key})
    return key

def card(key,values=(3,5)):
    return {'match_id':key,'scores':[{'side':i,'values':[n,n,n]} for i,n in enumerate(values)]}

def drain(play):
    worker=Worker(play.db,{'results.verify':verify_result})
    for _ in range(40):
        state=worker.once()
        if state=='idle': return
        assert state=='completed'
    pytest.fail('worker did not drain')

def test_whole_chain_receipts_boards_stats_and_replay(play):
    people=players(play); key=match(play,people)
    operation=uuid4(); first=call(play,people[0],'submit',card(key),operation)
    assert call(play,people[0],'submit',card(key),operation)['receipt_id']==first['receipt_id']
    drain(play)
    assert call(play,people[0],'get',{'match_id':key})['state']=='active'
    assert not play.board(RULE,'individual','records',0,20)['items']
    call(play,people[1],'submit',card(key)); drain(play)
    result=call(play,people[0],'get',{'match_id':key})
    assert result['state']=='confirmed' and result['trust']=='peer_confirmed'
    assert [o['score'] for o in result['outcomes']]==[9,15]
    board=play.board(RULE,'individual','records',0,20)
    assert [x['place'] for x in board['items']]==[1,2]
    assert call(play,people[0],'stats')=={'matches':1,'wins':1,'draws':0,'losses':0,'unique_defeated':1}
    assert len(call(play,people[0],'history')['items'])==1
    call(play,people[1],'submit',card(key));drain(play)
    assert play.board(RULE,'individual','records',0,20)==board
    with pytest.raises(AuthError,match='operation_payload_conflict'): call(play,people[0],'submit',card(key,(2,4)),operation)

def test_late_conflict_immediately_withdraws_and_invalidates_page(play):
    people=players(play);key=match(play,people)
    for p in people: call(play,p,'submit',card(key))
    drain(play);old=play.board(RULE,'individual','records',0,1)
    result=call(play,people[1],'submit',card(key,(4,3)))
    assert result['match']['state']=='disputed'
    assert not play.board(RULE,'individual','records',0,20)['items'] # before worker
    with pytest.raises(AuthError,match='stale_revision'):play.board(RULE,'individual','records',0,20,old['revision'])
    drain(play)
    assert call(play,people[0],'stats')['wins']==0
    assert len(call(play,people[0],'get',{'match_id':key})['decisions'])==2

def test_invalid_scorecard_never_gets_verified(play):
    people=players(play);key=match(play,people)
    call(play,people[0],'submit',card(key,(0,200)));drain(play)
    assert call(play,people[0],'get',{'match_id':key})['state']=='rejected'
    assert not play.board(RULE,'individual','wins',0,20)['items']

def test_unequal_teams_share_one_score_and_roster_subject(play):
    people=players(play,5);key=match(play,people,[2,3])
    for p in people:call(play,p,'submit',card(key))
    drain(play)
    board=play.board(RULE,'teams','records',0,20)
    assert [len(row['members']) for row in board['items']]==[2,3]
    assert [row['value'] for row in board['items']]==[9,15]
    assert call(play,people[0],'stats')['unique_defeated']==3
    assert call(play,people[1],'stats')['wins']==1
    assert call(play,people[4],'stats')['losses']==1
    assert not play.board(RULE,'individual','records',0,20)['items']

def test_sixteen_participants_concurrent_confirmation_and_rank_ties(play):
    people=players(play,16);key=match(play,people)
    payload=card(key,[3,3]+list(range(5,19)))
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(lambda p:call(play,p,'submit',payload),people))
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(lambda _:drain(play),range(4)))
    board=play.board(RULE,'individual','records',0,20)
    assert [r['place'] for r in board['items'][:3]]==[1,1,3]
    assert len(board['items'])==16
    assert call(play,people[0],'stats')['draws']==1
    assert call(play,people[0],'stats')['wins']==0
    with play.db.transaction() as conn:
        assert conn.execute('SELECT count(*) AS n FROM trainer_backend.result_decisions').fetchone()['n']==1

def test_private_match_and_roster_freeze_and_owner_start(play):
    people=players(play,3)
    key=str(call(play,people[0],'create',{'mode':'individual','seats':[1,1]})['match']['id'])
    with pytest.raises(AuthError,match='not_found'): call(play,people[2],'get',{'match_id':key})
    with pytest.raises(AuthError,match='roster_incomplete'): call(play,people[0],'start',{'match_id':key})
    call(play,people[1],'join',{'match_id':key,'side':1})
    with pytest.raises(AuthError,match='forbidden'): call(play,people[1],'start',{'match_id':key})
    call(play,people[0],'start',{'match_id':key})
    with pytest.raises(AuthError,match='roster_frozen'):call(play,people[2],'join',{'match_id':key,'side':1})
    with pytest.raises(AuthError,match='forbidden'):call(play,people[2],'submit',card(key))

def test_session_revocation_expiry_and_grant_is_not_account_token(play,postgres):
    p=players(play,1)[0]
    with pytest.raises(AuthError): play.accounts.me(p[1])
    play.accounts.revoke(p[0]['access_token'],p[0]['session_id'])
    with pytest.raises(AuthError):call(play,p,'history')
    p=players(play,1)[0]
    with psycopg.connect(postgres[0]) as conn:conn.execute("UPDATE trainer_backend.play_grants SET expires_at=clock_timestamp()-interval '1 second'")
    with pytest.raises(AuthError):call(play,p,'history')

def test_cross_ruleset_scope_and_immutable_registry(play,postgres):
    path,data=play.fixture
    assert register(postgres[0],path)
    data['definition']['maximum']=80;path.write_text(json.dumps(data))
    with pytest.raises(ValueError,match='immutable_ruleset'):register(postgres[0],path)
    data['id']='org.test.other';path.write_text(json.dumps(data));register(postgres[0],path)
    people=players(play);key=match(play,people)
    other=play.grant(people[0][0]['access_token'],{'app_id':'org.test.game','ruleset_id':data['id'],'operation_id':str(uuid4())})
    with pytest.raises(AuthError,match='not_found'):call(play,(people[0][0],other['grant_token']),'get',{'match_id':key})
    with play.db.transaction() as conn:
        with pytest.raises(psycopg.errors.InsufficientPrivilege):conn.execute('UPDATE trainer_backend.game_rulesets SET active=false')

def test_command_replay_after_new_grant_and_no_repeated_match(play):
    p=players(play,1)[0];operation=uuid4();payload={'mode':'individual','seats':[1,1]}
    old=call(play,p,'create',payload,operation)
    call(play,p,'revoke')
    fresh=play.grant(p[0]['access_token'],{'app_id':'org.test.game','ruleset_id':RULE,'operation_id':str(uuid4())})
    new=call(play,(p[0],fresh['grant_token']),'create',payload,operation)
    assert old['receipt_id']==new['receipt_id'] and old['match']['id']==new['match']['id']

def test_match_timeout_and_cancel(play,postgres):
    p=players(play,1)[0]
    key=str(call(play,p,'create',{'mode':'individual','seats':[1,1]})['match']['id'])
    with psycopg.connect(postgres[0]) as conn:conn.execute("UPDATE trainer_backend.matches SET deadline=clock_timestamp()-interval '1 second' WHERE id=%s",(key,))
    assert call(play,p,'get',{'match_id':key})['expired']
    with pytest.raises(AuthError,match='match_expired'):call(play,p,'start',{'match_id':key})
    other=str(call(play,p,'create',{'mode':'individual','seats':[1,1]})['match']['id'])
    assert call(play,p,'cancel',{'match_id':other})['match']['state']=='cancelled'

def test_http_schema_credentials_limits_and_public_boards(play,postgres):
    people=players(play)
    with TestClient(create_app(Settings(postgres[1],'test',oauth=CONFIG),provider=Provider())) as client:
        assert client.get('/v1/games').status_code==200
        assert client.post('/v1/play/grants',json={'app_id':'x','ruleset_id':RULE,'operation_id':str(uuid4())}).status_code==401
        token=people[0][1];headers={'Authorization':'Bearer '+token}
        body={'action':'create','operation_id':str(uuid4()),'payload':{'mode':'individual','seats':[1,1]}}
        response=client.post('/v1/play/commands',json=body,headers=headers)
        assert response.status_code==200 and response.json()['match']['revision']=='1'
        body['payload']['player_id']=str(people[1][0]['player_id'])
        assert client.post('/v1/play/commands',json=body,headers=headers).status_code==422
        assert token not in response.text
        assert client.post('/v1/play/commands',content='x'*16385,headers=headers).status_code==413
        assert client.get('/v1/boards/'+RULE+'/individual/records?limit=1000').status_code==422
        assert client.get('/v1/boards/'+RULE+'/individual/records').status_code==200

def test_all_http_participants_join_submit_and_read_board(play,postgres):
    people=players(play)
    with TestClient(create_app(Settings(postgres[1],'test',oauth=CONFIG),provider=Provider())) as client:
        def send(p,action,payload):
            response=client.post('/v1/play/commands',headers={'Authorization':'Bearer '+p[1]},
                json={'action':action,'operation_id':str(uuid4()),'payload':payload})
            assert response.status_code==200,response.text
            return response.json()
        key=send(people[0],'create',{'mode':'individual','seats':[1,1]})['match']['id']
        send(people[1],'join',{'match_id':key,'side':1});send(people[0],'start',{'match_id':key})
        for p in people:send(p,'submit',card(key))
        drain(play)
        assert send(people[0],'get',{'match_id':key})['state']=='confirmed'
        assert client.get('/v1/boards/'+RULE+'/individual/wins').json()['items'][0]['value']==1

def test_players_behind_one_ingress_have_independent_request_budgets(play,postgres):
    people=players(play)
    with TestClient(create_app(Settings(postgres[1],'test',oauth=CONFIG),provider=Provider())) as client:
        path='/v1/boards/'+RULE+'/individual/records'
        first={'Authorization':'Bearer '+people[0][1]};second={'Authorization':'Bearer '+people[1][1]}
        for _ in range(120):assert client.get(path,headers=first).status_code==200
        assert client.get(path,headers=first).status_code==429
        assert client.get(path,headers=second).status_code==200

def test_void_decision_removes_projections_without_deleting_source(play,postgres):
    people=players(play);key=match(play,people)
    for p in people:call(play,p,'submit',card(key))
    drain(play)
    operation=uuid4();first=void_match(postgres[0],key,operation)
    assert void_match(postgres[0],key,operation)['id']==first['id']
    assert not play.board(RULE,'individual','records',0,20)['items']
    result=call(play,people[0],'get',{'match_id':key})
    assert len(result['outcomes'])==2 and len(result['decisions'])==2
