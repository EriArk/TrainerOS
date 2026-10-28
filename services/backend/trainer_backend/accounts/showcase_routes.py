# SPDX-License-Identifier: GPL-3.0-or-later
from uuid import UUID
from fastapi import APIRouter, Request, Query
from pydantic import Field
from .routes import StrictBody, bearer, wire, service
from .showcase import Showcase

router = APIRouter()


class Sharing(StrictBody):
    revision: str = Field(pattern=r'^(0|[1-9][0-9]{0,17})$')
    level: bool
    shelf: bool
    history: bool
    activity: bool
    favorites: bool


class Favorites(StrictBody):
    revision: str = Field(pattern=r'^(0|[1-9][0-9]{0,17})$')
    games: list[str] = Field(max_length=5)


@router.put('/v1/me/profile/favorites')
def update_favorites(body: Favorites, request: Request):
    return wire(Showcase(service(request)).favorites(bearer(request),body.games,int(body.revision)))


@router.get('/v1/me/profile/sharing')
def sharing(request: Request):
    return wire(Showcase(service(request)).sharing(bearer(request)))


@router.put('/v1/me/profile/sharing')
def update_sharing(body: Sharing, request: Request):
    return wire(Showcase(service(request)).sharing(bearer(request), body.model_dump(exclude={'revision'}), int(body.revision)))


@router.get('/v1/profiles/fluxer/{subject}')
def by_subject(request: Request, subject: str, before: int|None = Query(default=None, ge=1)):
    from .provider import AuthError
    if not subject.isascii() or not subject.isdigit() or not 1<=len(subject)<=32:
        raise AuthError('invalid_subject', 422)
    return wire(Showcase(service(request)).read(bearer(request), subject=subject, before=before))


@router.get('/v1/profiles/{player}')
def by_player(request: Request, player: UUID, before: int|None = Query(default=None, ge=1)):
    return wire(Showcase(service(request)).read(bearer(request), player=player, before=before))
