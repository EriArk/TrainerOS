# SPDX-License-Identifier: GPL-3.0-or-later
"""Confidential Patreon API v2 adapter. Client claims never establish a paid tier."""
from dataclasses import dataclass, field
from datetime import datetime, timezone, timedelta
import json
import re
from urllib.parse import urlencode, urlsplit
from urllib.request import Request, build_opener

from ..accounts.provider import AuthError, NoRedirect


@dataclass(frozen=True)
class PatreonConfig:
    client_id: str
    client_secret: str = field(repr=False)
    public_base: str
    campaign_id: str
    tiers: dict[str, int]
    token_key: str = field(repr=False)
    webhook_secret: str = field(repr=False)

    def __post_init__(self):
        from cryptography.fernet import Fernet
        url = urlsplit(self.public_base)
        if (not re.fullmatch(r'[A-Za-z0-9_-]{4,128}', self.client_id)
                or not 16 <= len(self.client_secret) <= 4096
                or url.scheme != 'https' or not url.hostname or url.path or url.query or url.fragment or url.username or url.password
                or not re.fullmatch(r'[0-9]{1,32}', self.campaign_id)
                or not isinstance(self.tiers, dict) or not 2 <= len(self.tiers) <= 32
                or set(self.tiers.values()) != {2, 3}
                or any(not re.fullmatch(r'[0-9]{1,32}', k) or type(v) is not int for k, v in self.tiers.items())
                or not 16 <= len(self.webhook_secret) <= 4096):
            raise ValueError('invalid_patreon_configuration')
        Fernet(self.token_key.encode())

    @property
    def callback_url(self):
        return self.public_base + '/v1/support/patreon/callback'


class PatreonProvider:
    api = 'https://www.patreon.com'

    def __init__(self, config):
        self.config = config

    def authorization_url(self, state):
        # identity exposes memberships of this client's own creator campaign.
        # No email, other creators' memberships, address or write permission.
        return self.api + '/oauth2/authorize?' + urlencode({
            'response_type': 'code', 'client_id': self.config.client_id,
            'redirect_uri': self.config.callback_url, 'scope': 'identity', 'state': state})

    def _request(self, path, form=None, bearer=None):
        headers = {'Accept': 'application/json', 'User-Agent': 'TrainerOS-Backend/0.0.1'}
        body = None
        if form is not None:
            headers['Content-Type'] = 'application/x-www-form-urlencoded'
            body = urlencode(form).encode()
        if bearer:
            headers['Authorization'] = 'Bearer ' + bearer
        try:
            with build_opener(NoRedirect()).open(Request(self.api + path, data=body, headers=headers), timeout=8) as response:
                raw = response.read(262145)
                if len(raw) > 262144:
                    raise ValueError()
                result = json.loads(raw)
                if not isinstance(result, dict):
                    raise ValueError()
                return result
        except Exception:
            raise AuthError('support_provider_unavailable', 503) from None

    def tokens(self, *, code=None, refresh=None):
        form = {'client_id': self.config.client_id, 'client_secret': self.config.client_secret}
        form.update({'grant_type': 'authorization_code', 'code': code, 'redirect_uri': self.config.callback_url}
                    if code is not None else {'grant_type': 'refresh_token', 'refresh_token': refresh})
        value = self._request('/api/oauth2/token', form)
        if (value.get('token_type', '').lower() != 'bearer'
                or any(not isinstance(value.get(k), str) or not 16 <= len(value[k]) <= 4096
                       or any(c.isspace() for c in value[k]) for k in ('access_token', 'refresh_token'))
                or type(value.get('expires_in')) is not int or not 60 <= value['expires_in'] <= 31536000):
            raise AuthError('invalid_support_proof', 502)
        return {k: value[k] for k in ('access_token', 'refresh_token', 'expires_in')}

    def identify(self, access):
        fields = ('patron_status,last_charge_status,last_charge_date,next_charge_date,'
                  'currently_entitled_amount_cents,is_free_trial,is_gifted')
        return self._request('/api/oauth2/v2/identity?' + urlencode({
            'include': 'memberships.currently_entitled_tiers,memberships.campaign',
            'fields[member]': fields}), bearer=access)

    def entitlement(self, value, now=None):
        """Normalize a fresh identity read; amounts never select a tier by themselves."""
        now = now or datetime.now(timezone.utc)
        try:
            user = value['data']
            subject = user['id']
            if user['type'] != 'user' or not isinstance(subject, str) or not re.fullmatch(r'[0-9]{1,32}', subject):
                raise ValueError()
            refs = user['relationships']['memberships']['data']
            included = value.get('included', [])
            if not isinstance(refs, list) or not isinstance(included, list) or len(included) > 256:
                raise ValueError()
            records = {}
            for row in included:
                key = (row['type'], row['id'])
                if key in records:
                    raise ValueError()
                records[key] = row
            matches = []
            for ref in refs:
                if ref['type'] != 'member':
                    raise ValueError()
                member = records[('member', ref['id'])]
                campaign = member['relationships']['campaign']['data']
                if campaign['type'] == 'campaign' and campaign['id'] == self.config.campaign_id:
                    matches.append(member)
            if len(matches) > 1:
                raise ValueError()
            multiplier, member_id = 1, None
            valid_until = now + timedelta(minutes=30)
            if matches:
                member = matches[0]
                member_id = member['id']
                if not isinstance(member_id, str) or not re.fullmatch(r'[A-Za-z0-9-]{1,128}', member_id):
                    raise ValueError()
                attrs = member['attributes']
                tiers = member['relationships']['currently_entitled_tiers']['data']
                if not isinstance(tiers, list) or any(t['type'] != 'tier' for t in tiers):
                    raise ValueError()
                # Null/missing/ambiguous payment facts do not grant paid support.
                charged = attrs.get('last_charge_date')
                charge_date = datetime.fromisoformat(charged.replace('Z', '+00:00')) if isinstance(charged, str) else None
                amount = attrs.get('currently_entitled_amount_cents')
                paid = (attrs.get('patron_status') == 'active_patron' and attrs.get('last_charge_status') == 'Paid'
                        and type(amount) is int and amount > 0 and charge_date is not None
                        and charge_date.tzinfo is not None and charge_date <= now
                        and attrs.get('is_free_trial') is False and attrs.get('is_gifted') is False)
                if paid:
                    multiplier = max([1] + [self.config.tiers.get(t['id'], 1) for t in tiers])
                    following = attrs.get('next_charge_date')
                    if following:
                        following = datetime.fromisoformat(following.replace('Z', '+00:00'))
                        if following.tzinfo is None:
                            raise ValueError()
                        valid_until = min(valid_until, following)
                        if following <= now:
                            multiplier = 1
            return {'subject': subject, 'member_id': member_id, 'multiplier': multiplier,
                    'valid_until': valid_until, 'observed_at': now}
        except (KeyError, TypeError, ValueError, AttributeError):
            raise AuthError('invalid_support_proof', 502) from None
