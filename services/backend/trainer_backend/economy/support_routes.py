# SPDX-License-Identifier: GPL-3.0-or-later
from uuid import UUID
from fastapi import APIRouter, Request, Query
from fastapi.responses import RedirectResponse, Response
from pydantic import Field
from .support_links import SupportLinks
from ..accounts.provider import AuthError
from ..accounts.routes import StrictBody, LoginClaim, bearer, wire

router = APIRouter()
COOKIE = '__Host-traineros-support'


def service(request) -> SupportLinks:
    value = request.app.state.support_links
    if value is None:
        raise AuthError('provider_not_configured', 503)
    return value


class Begin(StrictBody):
    claim_challenge: str = Field(pattern=r'^[A-Za-z0-9_-]{43}$')
    return_uri: str = Field(max_length=128)


@router.get('/v1/me/support')
def status(request: Request):
    return wire(service(request).status(bearer(request)))


@router.post('/v1/me/support/patreon/attempts', status_code=201)
def begin(body: Begin, request: Request):
    return wire(service(request).begin(bearer(request), body.claim_challenge, body.return_uri))


@router.get('/v1/support/patreon/start', include_in_schema=False)
def start(request: Request, attempt_id: UUID, ticket: str = Query(pattern=r'^[A-Za-z0-9_-]{43}$')):
    cookie, url = service(request).start(attempt_id, ticket)
    response = RedirectResponse(url, status_code=303)
    response.set_cookie(COOKIE, cookie, secure=True, httponly=True, samesite='lax', path='/', max_age=600)
    return response


@router.get('/v1/support/patreon/callback', include_in_schema=False)
def callback(request: Request, state: str = Query(pattern=r'^[A-Za-z0-9_-]{43}$'),
             code: str | None = Query(default=None, min_length=1, max_length=2048),
             error: str | None = Query(default=None, max_length=128)):
    url = service(request).callback(state, request.cookies.get(COOKIE, ''), code, error)
    response = RedirectResponse(url, status_code=303)
    response.delete_cookie(COOKIE, secure=True, httponly=True, samesite='lax', path='/')
    return response


@router.post('/v1/me/support/patreon/claim')
def claim(body: LoginClaim, request: Request):
    return wire(service(request).claim(bearer(request), UUID(body.attempt_id), body.claim_verifier, body.handoff_token))


@router.post('/v1/me/support/patreon/check')
def check(request: Request):
    return wire(service(request).refresh(bearer(request)))


@router.delete('/v1/me/support/patreon')
def disconnect(request: Request):
    return wire(service(request).disconnect(bearer(request)))


@router.post('/v1/support/patreon/webhook', include_in_schema=False)
async def webhook(request: Request):
    from starlette.concurrency import run_in_threadpool
    await run_in_threadpool(service(request).webhook, await request.body(),
        request.headers.get('x-patreon-signature', ''), request.headers.get('x-patreon-event', ''))
    return Response(status_code=200)
