# SPDX-License-Identifier: GPL-3.0-or-later
from copy import deepcopy
from datetime import datetime, timezone, timedelta
import hashlib
import hmac
import json
import secrets
from urllib.parse import parse_qs, urlsplit
from uuid import uuid4
from cryptography.fernet import Fernet
import pytest
from trainer_backend.accounts.provider import AuthError
from trainer_backend.accounts.service import Accounts, challenge
from trainer_backend.economy.patreon import PatreonConfig, PatreonProvider
from trainer_backend.economy.service import Progression
from trainer_backend.economy.support_links import SupportLinks
from test_accounts import CONFIG, Provider, login
from test_membership import funded
from trainer_backend.economy.shop import Shop
from fastapi.testclient import TestClient
from trainer_backend.app import create_app
from trainer_backend.config import Settings

PATREON = PatreonConfig('client-test', 'secret-for-test-only', CONFIG.public_base, '101',
                        {'201': 2, '301': 3}, Fernet.generate_key().decode(), 'webhook-test-secret')


def proof(tier='201', subject='1234'):
    now = datetime.now(timezone.utc)
    return {'data': {'type': 'user', 'id': subject, 'relationships': {'memberships': {'data': [{'type': 'member', 'id': 'm1'}]}}},
            'included': [{'type': 'member', 'id': 'm1', 'attributes': {
                'patron_status': 'active_patron', 'last_charge_status': 'Paid',
                'last_charge_date': (now - timedelta(days=1)).isoformat(),
                'next_charge_date': (now + timedelta(days=29)).isoformat(),
                'currently_entitled_amount_cents': 300, 'is_free_trial': False, 'is_gifted': False},
                'relationships': {'campaign': {'data': {'type': 'campaign', 'id': '101'}},
                                  'currently_entitled_tiers': {'data': [{'type': 'tier', 'id': tier}]}}}]}


class Patreon(PatreonProvider):
    def __init__(self):
        super().__init__(PATREON)
        self.proof = proof()
        self.reads = 0
        self.before_read = None
        self.exchanges = 0
        self.expires = 3600
        self.unavailable = False

    def tokens(self, **kwargs):
        self.exchanges += 1
        if self.unavailable:
            raise AuthError('support_provider_unavailable', 503)
        return {'access_token': 'private-access-' + 'x' * 24, 'refresh_token': 'private-refresh-' + 'x' * 24, 'expires_in': self.expires}

    def identify(self, access):
        self.reads += 1
        if self.before_read:
            self.before_read()
        if self.unavailable:
            raise AuthError('support_provider_unavailable', 503)
        return deepcopy(self.proof)


@pytest.fixture
def support(db):
    accounts = Accounts(db, CONFIG, Provider())
    return SupportLinks(Progression(db, accounts), Patreon())


def authorize(support, access):
    verifier = secrets.token_urlsafe(32)
    row = support.begin(access, challenge(verifier), 'http://127.0.0.1:4567/traineros/auth/' + 'a' * 43)
    ticket = parse_qs(urlsplit(row['authorization_url']).query)['ticket'][0]
    browser, url = support.start(row['attempt_id'], ticket)
    state = parse_qs(urlsplit(url).query)['state'][0]
    callback = support.callback(state, browser, 'valid-code', None)
    handoff = parse_qs(urlsplit(callback).query)['handoff_token'][0]
    return row['attempt_id'], verifier, handoff


def connect(support, token):
    return support.claim(token, *authorize(support, token))


def test_binding_encrypted_one_shot_and_disconnect_retains_subject(support, db):
    token = login(support.accounts)['access_token']
    claim = authorize(support, token)
    assert support.provider.reads == 0  # browser alone cannot assign paid support
    result = support.claim(token, *claim)
    assert result['state'] == 'connected' and result['multiplier'] == 2
    with db.transaction() as conn:
        row = conn.execute('SELECT * FROM trainer_backend.support_links').fetchone()
        assert 'private-access' not in row['credentials'] and row['subject'] == '1234'
        assert conn.execute('SELECT credentials FROM trainer_backend.support_link_attempts').fetchone()['credentials'] is None
    with pytest.raises(AuthError):
        support.claim(token, *claim)
    assert support.disconnect(token)['multiplier'] == 1
    assert connect(support, token)['multiplier'] == 2
    support.provider.proof = proof(subject='9876')
    with pytest.raises(AuthError, match='support_identity_conflict'):
        connect(support, token)


def test_binding_rejects_another_session_player_and_revoked_session(support):
    first = login(support.accounts)
    claim = authorize(support, first['access_token'])
    second = login(support.accounts)
    with pytest.raises(AuthError):
        support.claim(second['access_token'], *claim)
    support.accounts.provider.subject = '987654321'
    other = login(support.accounts)
    with pytest.raises(AuthError):
        support.claim(other['access_token'], *claim)
    support.accounts.revoke(first['access_token'], first['session_id'])
    with pytest.raises(AuthError):
        support.claim(first['access_token'], *claim)
    assert support.provider.reads == 0


def test_new_attempt_and_disconnect_fence_late_callback(support):
    token = login(support.accounts)['access_token']
    old = authorize(support, token)
    current = authorize(support, token)
    with pytest.raises(AuthError):
        support.claim(token, *old)
    support.disconnect(token)
    with pytest.raises(AuthError):
        support.claim(token, *current)


def test_duplicate_patreon_cannot_bind_second_player(support):
    token = login(support.accounts)['access_token']
    connect(support, token)
    support.accounts.provider.subject = '987654321'
    other = login(support.accounts)['access_token']
    with pytest.raises(AuthError, match='support_identity_conflict'):
        connect(support, other)
    assert support.status(other)['multiplier'] == 1


def test_webhook_never_grants_and_fences_inflight_read(support, db):
    token = login(support.accounts)['access_token']
    connect(support, token)
    raw = json.dumps({'data': {'type': 'member', 'id': 'm1', 'attributes': {'multiplier': 3}}}).encode()
    sig = hmac.new(PATREON.webhook_secret.encode(), raw, 'md5').hexdigest()
    with pytest.raises(AuthError):
        support.webhook(raw, '0' * 32, 'members:update')
    support.provider.before_read = lambda: support.webhook(raw, sig, 'members:update')
    with pytest.raises(AuthError):
        support.refresh(token)
    assert support.status(token)['multiplier'] == 2
    support.provider.before_read = None
    support.provider.proof = proof('301')
    assert support.once()
    assert support.status(token)['multiplier'] == 3
    support.webhook(raw, sig, 'members:delete')  # stale event just triggers a fresh read
    assert support.once() and support.status(token)['multiplier'] == 3


def test_disconnect_during_read_cannot_reactivate(support):
    token = login(support.accounts)['access_token']
    connect(support, token)
    support.provider.before_read = lambda: support.disconnect(token)
    with pytest.raises(AuthError):
        support.refresh(token)
    assert support.status(token)['state'] == 'disconnected'
    assert support.status(token)['multiplier'] == 1


def test_refresh_failure_does_not_extend_evidence_and_rotation_is_not_replayed(support, db):
    token = login(support.accounts)['access_token']
    connect(support, token)
    until = support.status(token)['verified_until']
    support.provider.unavailable = True
    with pytest.raises(AuthError):
        support.refresh(token)
    assert support.status(token)['verified_until'] == until
    with db.transaction() as conn:
        row = conn.execute('SELECT * FROM trainer_backend.support_links').fetchone()
        tokens = json.loads(support.cipher.decrypt(row['credentials'].encode()))
        tokens['expires_at'] = (datetime.now(timezone.utc) - timedelta(seconds=1)).isoformat()
        conn.execute('UPDATE trainer_backend.support_links SET credentials=%s', (support.cipher.encrypt(json.dumps(tokens).encode()).decode(),))
    with pytest.raises(AuthError):
        support.refresh(token)
    assert support.status(token)['state'] == 'needs_relink'
    n = support.provider.exchanges
    with pytest.raises(AuthError):
        support.refresh(token)
    assert support.provider.exchanges == n


@pytest.mark.parametrize('field,value', [('patron_status', 'declined_patron'), ('last_charge_status', 'Declined'),
    ('is_free_trial', True), ('is_gifted', True), ('currently_entitled_amount_cents', 0),
    ('last_charge_date', None), ('next_charge_date', '2000-01-01T00:00:00Z')])
def test_unpaid_ambiguous_or_expired_entitlement_is_base(field, value):
    value_proof = proof()
    value_proof['included'][0]['attributes'][field] = value
    assert PatreonProvider(PATREON).entitlement(value_proof)['multiplier'] == 1


def test_amount_alone_other_campaign_and_duplicate_members_never_grant():
    provider = PatreonProvider(PATREON)
    value = proof('999')
    value['included'][0]['attributes']['currently_entitled_amount_cents'] = 999999
    assert provider.entitlement(value)['multiplier'] == 1
    value = proof()
    value['included'][0]['relationships']['campaign']['data']['id'] = '999'
    assert provider.entitlement(value)['multiplier'] == 1
    value = proof()
    value['data']['relationships']['memberships']['data'] *= 2
    with pytest.raises(AuthError):
        provider.entitlement(value)


def test_unconfigured_still_authenticates_and_supports_disconnect(support):
    token = login(support.accounts)['access_token']
    connect(support, token)
    unconfigured = SupportLinks(support.progression)
    assert unconfigured.status(token)['available'] is False
    with pytest.raises(AuthError):
        unconfigured.status('invalid')
    assert unconfigured.disconnect(token)['multiplier'] == 1


def test_real_adapter_flow_preserves_purchase_and_pays_only_missing_bonus(support):
    progress, user, _, _ = funded(support.accounts)
    support.progression = progress
    access = user['access_token']
    Shop(progress).purchase(access, uuid4(), 'profile.aurora', 1, 250)
    connect(support, access)
    assert progress.snapshot(access)['balance'] == 1750
    support.refresh(access)
    assert progress.snapshot(access)['balance'] == 1750
    support.provider.proof = proof('301')
    support.refresh(access)
    assert progress.snapshot(access)['balance'] == 2750
    assert progress.snapshot(access)['mastery']['xp'] == 2000
    support.provider.proof['included'][0]['attributes']['patron_status'] = 'former_patron'
    support.refresh(access)
    assert support.status(access)['multiplier'] == 1
    assert progress.snapshot(access)['balance'] == 2750
    assert len(Shop(progress).snapshot(access)['items']) == 1


def test_http_binding_and_credential_redaction(support, postgres):
    user = login(support.accounts)
    with TestClient(create_app(Settings(postgres[1], 'test', oauth=CONFIG, patreon=PATREON),
                              provider=Provider(), support_provider=support.provider), base_url=CONFIG.public_base) as client:
        auth = {'Authorization': 'Bearer ' + user['access_token']}
        assert client.get('/v1/me/support').status_code == 401
        verifier = secrets.token_urlsafe(32)
        row = client.post('/v1/me/support/patreon/attempts', headers=auth, json={
            'claim_challenge': challenge(verifier), 'return_uri': 'http://127.0.0.1:4567/traineros/auth/'+'a'*43})
        assert row.status_code == 201
        started = client.get(row.json()['authorization_url'], follow_redirects=False)
        assert started.status_code == 303 and 'HttpOnly' in started.headers['set-cookie']
        state = parse_qs(urlsplit(started.headers['location']).query)['state'][0]
        callback = client.get('/v1/support/patreon/callback', params={'state': state, 'code': 'valid-code'}, follow_redirects=False)
        assert callback.status_code == 303
        handoff = parse_qs(urlsplit(callback.headers['location']).query)['handoff_token'][0]
        result = client.post('/v1/me/support/patreon/claim', headers=auth, json={
            'attempt_id': row.json()['attempt_id'], 'claim_verifier': verifier, 'handoff_token': handoff})
        assert result.status_code == 200 and result.json()['multiplier'] == 2
        assert 'private-' not in result.text and PATREON.client_secret not in result.text
        assert result.headers['cache-control'] == 'no-store'
        assert client.post('/v1/me/support/patreon/attempts', headers=auth, json={'multiplier': 3}).status_code == 422
        assert client.delete('/v1/me/support/patreon', headers=auth).json()['multiplier'] == 1
