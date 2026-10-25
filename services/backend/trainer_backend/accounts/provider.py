# SPDX-License-Identifier: GPL-3.0-or-later
"""Confidential Fluxer OAuth client. No Social user token, password or bot token."""
from dataclasses import dataclass, field
import json
import re
from urllib.parse import urlencode, urlsplit
from urllib.request import Request, HTTPRedirectHandler, build_opener


class AuthError(Exception):
    def __init__(self, code='unauthenticated', status=401):
        self.code, self.status = code, status
        super().__init__(code)


@dataclass(frozen=True)
class OAuthConfig:
    client_id: str
    client_secret: str = field(repr=False)
    public_base: str
    key: str = field(repr=False)

    def __post_init__(self):
        url = urlsplit(self.public_base)
        if (not re.fullmatch(r'[0-9]{1,20}', self.client_id) or len(self.client_secret)<16
                or len(self.key)<43 or url.scheme != 'https' or not url.hostname
                or url.username or url.password or url.query or url.fragment or url.path):
            raise ValueError('invalid_oauth_configuration')

    @property
    def callback_url(self):
        return self.public_base + '/v1/auth/callback/fluxer'


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        raise AuthError('provider_unavailable', 503)


class FluxerProvider:
    instance = 'https://fluxer.app'
    api = 'https://api.fluxer.app'

    def __init__(self, config):
        self.config = config

    def authorization_url(self, state, challenge):
        return 'https://web.fluxer.app/oauth2/authorize?' + urlencode({
            'client_id': self.config.client_id, 'redirect_uri': self.config.callback_url,
            'response_type': 'code', 'scope': 'identify', 'state': state,
            'code_challenge': challenge, 'code_challenge_method': 'S256'})

    def _request(self, path, form=None, bearer=None):
        headers = {'Accept': 'application/json', 'User-Agent': 'TrainerOS-Backend/0.0.1'}
        body = None
        if form is not None:
            headers['Content-Type'] = 'application/x-www-form-urlencoded'
            body = urlencode(form).encode()
        if bearer:
            headers['Authorization'] = 'Bearer ' + bearer
        try:
            with build_opener(NoRedirect()).open(Request(self.api+path, data=body, headers=headers), timeout=8) as response:
                raw = response.read(65537)
                if len(raw)>65536:
                    raise ValueError('provider_body_too_large')
                value = json.loads(raw)
                if not isinstance(value, dict):
                    raise ValueError('provider_object_required')
                return value
        except Exception:
            # Never surface a provider response/exception containing secrets or identity.
            raise AuthError('provider_unavailable', 503) from None

    def identify(self, code, verifier):
        token = self._request('/oauth2/token', {'grant_type': 'authorization_code',
            'code': code, 'redirect_uri': self.config.callback_url,
            'client_id': self.config.client_id, 'client_secret': self.config.client_secret,
            'code_verifier': verifier})
        access = token.get('access_token')
        if (token.get('token_type', '').lower() != 'bearer' or
                'identify' not in str(token.get('scope','')).split() or
                not isinstance(access,str) or not 16<=len(access)<=4096 or
                any(c.isspace() for c in access)):
            raise AuthError('provider_unavailable', 503)
        # Provider credentials are used only for this exchange and never persisted.
        user = self._request('/oauth2/userinfo', bearer=access)
        subject = user.get('sub', user.get('id'))
        if not isinstance(subject,str) or not re.fullmatch(r'[0-9]{1,20}',subject) or user.get('bot') or user.get('system'):
            raise AuthError('unauthenticated', 401)
        return subject
