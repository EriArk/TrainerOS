# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only per-user RA Web API. No shared key, emulated awards or trigger cache."""
import json
import re
from datetime import datetime, timezone
from urllib.request import Request, HTTPRedirectHandler, build_opener
from urllib.parse import urlencode
from urllib.error import URLError
from ..accounts.provider import AuthError


def timestamp(value):
    if not isinstance(value, str):
        return None
    try:
        result = datetime.fromisoformat(value.replace('Z', '+00:00'))
        return result.replace(tzinfo=timezone.utc) if result.tzinfo is None else result.astimezone(timezone.utc)
    except ValueError:
        return None


class RetroAchievements:
    def request(self, endpoint, key, **params):
        try:
            class NoRedirect(HTTPRedirectHandler):
                def redirect_request(self, *args, **kwargs):
                    return None
            url = 'https://retroachievements.org/API/' + endpoint + '.php?' + urlencode({'y': key, **params})
            request = Request(url, headers={'User-Agent': 'TrainerOS-Progression/1.0'})
            with build_opener(NoRedirect).open(request, timeout=12) as response:
                if response.status != 200:
                    raise AuthError('ra_unavailable', 503)
                raw = response.read(2 * 1024 * 1024 + 1)
                if len(raw) > 2 * 1024 * 1024:
                    raise AuthError('ra_invalid_response', 503)
                value = json.loads(raw)
            if not isinstance(value, dict) or value.get('Error'):
                raise AuthError('ra_invalid_response', 503)
            return value
        except (URLError, OSError, ValueError):
            # Never include a credential-bearing request URL or upstream text.
            raise AuthError('ra_unavailable', 503) from None

    def profile(self, key, user):
        value = self.request('API_GetUserProfile', key, u=user)
        if not re.fullmatch(r'[0-9A-HJKMNP-TV-Z]{26}', str(value.get('ULID', ''))):
            raise AuthError('ra_invalid_identity', 503)
        name = value.get('User', '')
        if not isinstance(name, str) or not re.fullmatch(r'[A-Za-z0-9_]{2,32}', name):
            raise AuthError('ra_invalid_identity', 503)
        return {'id': value['ULID'], 'name': name, 'motto': value.get('Motto', ''),
                'eligible': value.get('Untracked') in (0, '0') and value.get('Permissions') in (1, 2, 3, 4)}

    def game(self, key, user, game):
        value = self.request('API_GetGameInfoAndUserProgress', key, u=user, g=game, a=1)
        if type(value.get('ID')) is not int or value['ID'] != game or type(value.get('ConsoleID')) is not int:
            raise AuthError('ra_invalid_game', 503)
        if value['ConsoleID'] in (100, 101):
            return {'id': game, 'title': 'Unsupported catalogue', 'achievements': []}
        rows = value.get('Achievements')
        if not isinstance(rows, dict) or len(rows) > 4096:
            raise AuthError('ra_invalid_game', 503)
        result = []
        for identity, row in rows.items():
            if not isinstance(row, dict) or str(row.get('ID')) != identity or not identity.isdigit():
                raise AuthError('ra_invalid_game', 503)
            points = row.get('Points')
            if type(points) is not int or not 0 < points <= 100:
                raise AuthError('ra_invalid_game', 503)
            dates = [timestamp(row[k]) for k in ('DateEarned', 'DateEarnedHardcore') if k in row]
            if any(d is None for d in dates):
                raise AuthError('ra_invalid_date', 503)
            dates = [d for d in dates if d is not None]
            result.append({'id': identity, 'points': points, 'title': str(row.get('Title', 'Achievement'))[:256],
                           'earned': min(dates) if dates else None})
        if value.get('NumAchievements') != len(result):
            raise AuthError('ra_incomplete_game', 503)
        if value.get('NumAwardedToUser') != sum(r['earned'] is not None for r in result):
            raise AuthError('ra_incomplete_progress', 503)
        return {'id': game, 'title': str(value.get('Title', 'Game'))[:256], 'achievements': result}
