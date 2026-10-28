# SPDX-License-Identifier: GPL-3.0-or-later
from fastapi import APIRouter, Request
from pydantic import Field
from ..accounts.routes import StrictBody, bearer, wire
from ..accounts.provider import AuthError

router = APIRouter()


class Key(StrictBody):
    web_key: str = Field(min_length=16, max_length=128, pattern=r'^[A-Za-z0-9]+$')


class Link(Key):
    username: str = Field(pattern=r'^[A-Za-z0-9_]{2,32}$')


class Sync(Key):
    games: list[int] = Field(min_length=1, max_length=5)


def service(request):
    result = request.app.state.progression
    if result is None:
        raise AuthError('provider_not_configured', 503)
    return result


@router.get('/v1/me/progression')
def snapshot(request: Request):
    return wire(service(request).snapshot(bearer(request)))


@router.post('/v1/me/progression/ra/challenge')
def challenge(body: Link, request: Request):
    return wire(service(request).challenge(bearer(request), body.username, body.web_key))


@router.post('/v1/me/progression/ra/verify')
def verify(body: Key, request: Request):
    return wire(service(request).verify(bearer(request), body.web_key))


@router.post('/v1/me/progression/ra/sync')
def sync(body: Sync, request: Request):
    if any(type(i) is not int or not 0 < i <= 2147483647 for i in body.games):
        raise AuthError('invalid_game_ids', 422)
    return wire(service(request).sync(bearer(request), body.web_key, body.games))
