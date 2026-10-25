"""Read-only check of the dedicated HTTPS ingress, without provider credentials."""
import argparse
import json
from urllib.error import HTTPError
from urllib.parse import urlsplit
from urllib.request import Request, urlopen

parser = argparse.ArgumentParser()
parser.add_argument('origin')
parser.add_argument('--accounts', action='store_true')
args = parser.parse_args()
origin = urlsplit(args.origin)
if (origin.scheme != 'https' or not origin.hostname or origin.path
        or origin.query or origin.fragment or origin.username or origin.password):
    parser.error('An HTTPS origin without credentials or a path is required')

checks = []
for path, expected in (
    ('/health/live', 200), ('/health/ready', 404), ('/openapi.json', 404),
    ('/v1/capabilities', 200), ('/v1/me', 401 if args.accounts else 503),
    ('/v1/me/purchases', 404), ('/unpublished', 404),
):
    request = Request(args.origin + path, headers={
        'User-Agent': 'TrainerOS-Backend/0.0.1', 'Accept': 'application/json'})
    try:
        response = urlopen(request, timeout=15)
    except HTTPError as error:
        response = error
    with response:
        status = response.status
        body = response.read(16385)
        assert status == expected, (path, status, expected)
        assert len(body) <= 16384, (path, 'oversized_response')
        if path in ('/health/live', '/v1/capabilities', '/v1/me'):
            assert response.headers.get('Cache-Control') == 'no-store', path
        if path == '/v1/capabilities':
            capabilities = json.loads(body)['capabilities']
            assert capabilities == ({'accounts.fluxer_login': 1, 'accounts.sessions': 1}
                                    if args.accounts else {}), 'unexpected_capabilities'
        if path == '/v1/me':
            assert json.loads(body)['code'] == ('unauthenticated' if args.accounts
                                               else 'provider_not_configured')
        checks.append({'path': path, 'status': status})

print(json.dumps({'origin': args.origin, 'accounts_configured': args.accounts,
                  'checks': checks, 'live_oauth_proven': False}, indent=2))
