# SPDX-License-Identifier: GPL-3.0-or-later
from uuid import UUID
from fastapi import APIRouter, Request
from pydantic import Field
from ..accounts.routes import StrictBody, bearer, wire
from ..accounts.provider import AuthError
from . import outfits

router = APIRouter()


def service(request):
    result = request.app.state.shop
    if result is None:
        raise AuthError('provider_not_configured', 503)
    return result


class Purchase(StrictBody):
    operation_id: str = Field(pattern=r'^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$')
    offer_id: str = Field(pattern=r'^[a-z][a-z0-9_.-]{0,63}$')
    offer_revision: int = Field(ge=1, le=2147483647)
    expected_price: int = Field(ge=1, le=2147483647)


class Appearance(StrictBody):
    offer_id: str | None = Field(default=None, max_length=64)
    revision: str = Field(pattern=r'^[0-9]{1,18}$')


class Outfit(StrictBody):
    name: str = Field(min_length=1, max_length=32)
    recipe: dict
    revision: str = Field(pattern=r'^[0-9]{1,18}$')


@router.put('/v1/me/inventory/outfits/{offer_id}')
def save_outfit(offer_id: str, body: Outfit, request: Request):
    return wire(outfits.save(service(request), bearer(request), offer_id, body.name, body.recipe, int(body.revision)))


@router.get('/v1/shop/items')
def catalogue(request: Request):
    return wire(service(request).catalogue())


@router.get('/v1/me/inventory')
def inventory(request: Request):
    return wire(service(request).snapshot(bearer(request)))


@router.post('/v1/me/purchases')
def purchase(body: Purchase, request: Request):
    return wire(service(request).purchase(bearer(request), UUID(body.operation_id), body.offer_id, body.offer_revision, body.expected_price))


@router.get('/v1/me/purchases/{operation_id}')
def receipt(operation_id: UUID, request: Request):
    return wire(service(request).snapshot(bearer(request), operation_id))


@router.put('/v1/me/inventory/appearance')
def equip(body: Appearance, request: Request):
    return wire(service(request).equip(bearer(request), body.offer_id, int(body.revision)))
