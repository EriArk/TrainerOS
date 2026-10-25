# SPDX-License-Identifier: GPL-3.0-or-later
from concurrent.futures import ThreadPoolExecutor
import hashlib
import secrets
import json
import threading
from http.server import BaseHTTPRequestHandler,HTTPServer
from urllib.parse import parse_qs,urlsplit
from uuid import uuid4,UUID

from fastapi.testclient import TestClient
import psycopg
import pytest

from trainer_backend.app import create_app
from trainer_backend.config import Settings
from trainer_backend.accounts.provider import OAuthConfig,FluxerProvider,AuthError
from trainer_backend.accounts.service import Accounts,challenge,digest

CONFIG=OAuthConfig('123456789','test-client-secret-only','https://testserver','test-key-'+'x'*43)


class Provider(FluxerProvider):
    def __init__(self):
        super().__init__(CONFIG)
        self.subject='1234567890123456789'
        self.challenge=None
        self.calls=0

    def authorization_url(self,state,pkce):
        self.challenge=pkce
        return super().authorization_url(state,pkce)

    def identify(self,code,verifier):
        self.calls+=1
        assert challenge(verifier)==self.challenge
        if code!='valid-code':
            raise AuthError('provider_unavailable',503)
        return self.subject


@pytest.fixture
def account(db):
    return Accounts(db,CONFIG,Provider())


def attempt(service):
    verifier=secrets.token_urlsafe(32)
    row=service.begin(uuid4(),{'provider':'fluxer','device_label':'test device',
        'claim_challenge':challenge(verifier),'claim_challenge_method':'S256'})
    browser=service.start(row['attempt_id'],parse_qs(urlsplit(row['authorization_url']).query)['ticket'][0])
    url=service.confirm(row['attempt_id'],browser,browser,row['verification_code'])
    state=parse_qs(urlsplit(url).query)['state'][0]
    return row['attempt_id'],verifier,state,browser


def login(service):
    key,verifier,state,browser=attempt(service)
    service.callback(state,browser,'valid-code',None)
    return service.claim(key,verifier)


def test_complete_http_flow_uses_fluxer_identity_and_secret_claim(postgres,db):
    provider=Provider()
    with TestClient(create_app(Settings(postgres[1],'test',oauth=CONFIG),provider=provider),base_url=CONFIG.public_base) as client:
        verifier=secrets.token_urlsafe(32)
        body={'provider':'fluxer','device_label':'desktop','claim_challenge':challenge(verifier),'claim_challenge_method':'S256'}
        headers={'Idempotency-Key':str(uuid4())}
        begin=client.post('/v1/auth/login-attempts',json=body,headers=headers)
        assert begin.status_code==201
        assert begin.json()['expires_at'].endswith('Z')
        assert client.post('/v1/auth/login-attempts',json=body,headers=headers).json()==begin.json()
        body['device_label']='changed'
        assert client.post('/v1/auth/login-attempts',json=body,headers=headers).status_code==409
        started=client.get(begin.json()['authorization_url'],follow_redirects=False)
        assert started.status_code==200 and 'Secure' in started.headers['set-cookie'] and 'HttpOnly' in started.headers['set-cookie']
        assert begin.json()['verification_code'] not in started.text
        form={'attempt_id':begin.json()['attempt_id'],'proof':client.cookies.get('__Host-traineros-login'),'code':begin.json()['verification_code']}
        assert client.post('/v1/auth/fluxer/confirm',data=form,headers={'Origin':'https://attacker.invalid'},follow_redirects=False).status_code==403
        confirmed=client.post('/v1/auth/fluxer/confirm',data=form,headers={'Origin':CONFIG.public_base},follow_redirects=False)
        assert confirmed.status_code==303
        params=parse_qs(urlsplit(confirmed.headers['location']).query)
        assert params['scope']==['identify'] and params['code_challenge_method']==['S256']
        assert CONFIG.client_secret not in confirmed.headers['location']
        callback=client.get('/v1/auth/callback/fluxer',params={'state':params['state'][0],'code':'valid-code'})
        assert callback.status_code==200 and 'access_token' not in callback.text
        assert client.post('/v1/auth/login-attempts/claim',json={'attempt_id':begin.json()['attempt_id'],'claim_verifier':'z'*43}).status_code==401
        result=client.post('/v1/auth/login-attempts/claim',json={'attempt_id':begin.json()['attempt_id'],'claim_verifier':verifier})
        assert result.status_code==200 and result.headers['cache-control']=='no-store'
        auth={'Authorization':'Bearer '+result.json()['access_token']}
        assert client.get('/v1/me',headers=auth).json()['player_id']==result.json()['player_id']
        assert isinstance(client.get('/v1/me',headers=auth).json()['revision'],str)
        rows=client.get('/v1/auth/sessions',headers=auth).json()['items']
        assert len(rows)==1 and rows[0]['current']
        assert client.delete('/v1/auth/sessions/'+result.json()['session_id'],headers=auth).status_code==204
        assert client.get('/v1/me',headers=auth).status_code==401


def test_same_fluxer_subject_keeps_id_and_other_subject_is_separate(account):
    first,second=login(account),login(account)
    assert first['player_id']==second['player_id'] and first['session_id']!=second['session_id']
    account.provider.subject='9876543210123456789'
    third=login(account)
    assert third['player_id']!=first['player_id']
    assert len(account.sessions(first['access_token'])['items'])==2
    with pytest.raises(AuthError) as error:
        account.revoke(first['access_token'],third['session_id'])
    assert error.value.status==404
    assert account.me(third['access_token'])['status']=='active'


def test_parallel_claim_only_issues_one_session(account,db):
    key,verifier,state,browser=attempt(account)
    account.callback(state,browser,'valid-code',None)
    def claim(_):
        try: return account.claim(key,verifier)
        except AuthError: return None
    with ThreadPoolExecutor(max_workers=4) as pool:
        results=list(pool.map(claim,range(4)))
    assert sum(r is not None for r in results)==1
    with db.transaction() as conn:
        assert conn.execute('SELECT count(*) AS n FROM trainer_backend.device_sessions').fetchone()['n']==1


def test_wrong_browser_state_and_callback_replay_do_not_exchange(account):
    key,verifier,state,browser=attempt(account)
    for s,b in [('invalid',browser),(state,'other-browser')]:
        with pytest.raises(AuthError): account.callback(s,b,'valid-code',None)
    assert account.provider.calls==0
    account.callback(state,browser,'valid-code',None)
    with pytest.raises(AuthError): account.callback(state,browser,'valid-code',None)
    assert account.provider.calls==1
    account.claim(key,verifier)


@pytest.mark.parametrize('code,error',[('bad-code',None),(None,'access_denied')])
def test_cancel_or_provider_failure_cannot_be_claimed(account,code,error):
    key,verifier,state,browser=attempt(account)
    with pytest.raises(AuthError): account.callback(state,browser,code,error)
    with pytest.raises(AuthError): account.claim(key,verifier)


def test_expired_attempt_and_access_are_rejected(postgres,account):
    tokens=login(account)
    key,verifier,state,browser=attempt(account)
    with psycopg.connect(postgres[0]) as conn:
        conn.execute("UPDATE trainer_backend.login_attempts SET expires_at=clock_timestamp()-interval '1 second'")
        conn.execute("UPDATE trainer_backend.device_sessions SET access_expires_at=clock_timestamp()-interval '1 second'")
    with pytest.raises(AuthError): account.callback(state,browser,'valid-code',None)
    with pytest.raises(AuthError): account.claim(key,verifier)
    with pytest.raises(AuthError): account.me(tokens['access_token'])
    fresh=account.refresh(tokens['refresh_token'])
    assert account.me(fresh['access_token'])['player_id']==tokens['player_id']


def test_refresh_rotation_replay_revokes_only_its_session(account):
    old=login(account)
    separate=login(account)
    fresh=account.refresh(old['refresh_token'])
    with pytest.raises(AuthError): account.me(old['access_token'])
    assert account.me(fresh['access_token'])['status']=='active'
    with pytest.raises(AuthError): account.refresh(old['refresh_token'])
    with pytest.raises(AuthError): account.me(fresh['access_token'])
    with pytest.raises(AuthError): account.refresh(fresh['refresh_token'])
    assert account.me(separate['access_token'])['status']=='active'


def test_suspended_player_cannot_refresh_login_or_read(postgres,account):
    old=login(account)
    with psycopg.connect(postgres[0]) as conn:
        conn.execute("UPDATE trainer_backend.principals SET state='suspended' WHERE id=%s",(old['player_id'],))
    with pytest.raises(AuthError): account.me(old['access_token'])
    with pytest.raises(AuthError): account.refresh(old['refresh_token'])
    with pytest.raises(AuthError): login(account)


def test_runtime_cannot_promote_or_reassign_identities(db):
    for sql in ["INSERT INTO trainer_backend.principals(id,kind,state) VALUES(gen_random_uuid(),'operator','active')",
                "UPDATE trainer_backend.external_identities SET subject='123'",
                "SELECT trainer_backend.resolve_fluxer_player('https://evil.invalid','123')"]:
        with pytest.raises(psycopg.Error):
            with db.transaction() as conn: conn.execute(sql)


def test_validation_and_large_request_never_echo_secrets(postgres,db):
    with TestClient(create_app(Settings(postgres[1],'test',oauth=CONFIG),provider=Provider()),base_url=CONFIG.public_base) as client:
        secret='private password must never be echoed'
        response=client.post('/v1/auth/refresh',json={'refresh_token':secret})
        assert response.status_code==422 and secret not in response.text
        response=client.post('/v1/auth/refresh',content='x'*16385)
        assert response.status_code==413
        for _ in range(119): client.get('/v1/me')
        response=client.get('/v1/me')
        assert response.status_code==429 and response.headers['retry-after']=='60'


def test_only_token_hashes_persist(account,db):
    tokens=login(account)
    with db.transaction() as conn:
        session=conn.execute('SELECT * FROM trainer_backend.device_sessions').fetchone()
        refresh=conn.execute('SELECT * FROM trainer_backend.refresh_credentials').fetchone()
    assert session['access_hash']==digest(tokens['access_token'])
    assert refresh['token_hash']==digest(tokens['refresh_token'])
    assert tokens['access_token'] not in str(session) and tokens['refresh_token'] not in str(refresh)


def test_provider_sends_pkce_and_only_identify_token(monkeypatch):
    provider=FluxerProvider(CONFIG)
    calls=[]
    def fake(path,form=None,bearer=None):
        calls.append((path,form,bearer))
        if path=='/oauth2/token': return {'token_type':'Bearer','scope':'identify','access_token':'a'*43}
        return {'sub':'1234567890123456789'}
    monkeypatch.setattr(provider,'_request',fake)
    assert provider.identify('valid-code','verifier')=='1234567890123456789'
    assert calls[0][1]['code_verifier']=='verifier'
    assert calls[1][0]=='/oauth2/userinfo' and calls[1][2]=='a'*43


def test_concurrent_identity_resolution_creates_one_player(db):
    def resolve(_):
        with db.transaction() as conn:
            return conn.execute("SELECT trainer_backend.resolve_fluxer_player('https://fluxer.app','12345') AS id").fetchone()['id']
    with ThreadPoolExecutor(max_workers=4) as pool:
        players=list(pool.map(resolve,range(4)))
    assert len(set(players))==1


def test_parallel_refresh_replay_commits_revocation(account,db):
    old=login(account)
    def refresh(_):
        try: return account.refresh(old['refresh_token'])
        except AuthError: return None
    with ThreadPoolExecutor(max_workers=2) as pool:
        values=list(pool.map(refresh,range(2)))
    fresh=[value for value in values if value]
    assert len(fresh)==1
    with pytest.raises(AuthError): account.me(fresh[0]['access_token'])
    with db.transaction() as conn:
        event=conn.execute("SELECT aggregate_id,aggregate_revision FROM trainer_backend.event_index WHERE event_type='account.session_revoked'").fetchone()
    assert event['aggregate_id']==old['session_id'] and event['aggregate_revision']==2


def test_provider_http_transport_rejects_redirects_and_oversized_bodies():
    seen=[]
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_POST(self):
            form=parse_qs(self.rfile.read(int(self.headers['Content-Length'])).decode())
            seen.append(form)
            self.send_response(200);self.end_headers()
            self.wfile.write(json.dumps({'token_type':'Bearer','scope':'identify','access_token':'a'*43}).encode())
        def do_GET(self):
            if self.path=='/redirect':
                self.send_response(302);self.send_header('Location','/secret-target');self.end_headers();return
            if self.path=='/secret-target': seen.append('redirect-followed')
            self.send_response(200);self.end_headers()
            self.wfile.write(b'x'*65537 if self.path=='/large' else b'{"sub":"12345"}')
    server=HTTPServer(('127.0.0.1',0),Handler)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        provider=FluxerProvider(CONFIG)
        provider.api='http://127.0.0.1:'+str(server.server_port)  # Test-only instance override, not configurable by a client.
        assert provider.identify('code','pkce-verifier')=='12345'
        assert seen[0]['code_verifier']==['pkce-verifier']
        for path in ['/redirect','/large']:
            with pytest.raises(AuthError): provider._request(path,bearer='a'*43)
        assert 'redirect-followed' not in seen
    finally:
        server.shutdown();server.server_close();thread.join(timeout=2)


def test_confirmation_code_is_required_and_five_failures_lock_attempt(account):
    verifier=secrets.token_urlsafe(32)
    row=account.begin(uuid4(),{'provider':'fluxer','device_label':'test','claim_challenge':challenge(verifier),'claim_challenge_method':'S256'})
    browser=account.start(row['attempt_id'],parse_qs(urlsplit(row['authorization_url']).query)['ticket'][0])
    with pytest.raises(AuthError): account.callback(account.secret('state',row['attempt_id']),browser,'valid-code',None)
    assert account.provider.calls==0
    wrong=str((int(row['verification_code'])+1)%100000000).zfill(8)
    for _ in range(5):
        with pytest.raises(AuthError): account.confirm(row['attempt_id'],browser,browser,wrong)
    with pytest.raises(AuthError): account.confirm(row['attempt_id'],browser,browser,row['verification_code'])
    with pytest.raises(AuthError): account.claim(row['attempt_id'],verifier)
