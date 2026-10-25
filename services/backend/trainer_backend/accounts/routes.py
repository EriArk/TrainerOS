# SPDX-License-Identifier: GPL-3.0-or-later
from typing import Literal
from uuid import UUID
from urllib.parse import parse_qs
from datetime import datetime,timezone

from fastapi import APIRouter, Request, Header, Query
from fastapi.responses import HTMLResponse, RedirectResponse, Response
from pydantic import BaseModel, ConfigDict, Field
from starlette.concurrency import run_in_threadpool

from .provider import AuthError

router = APIRouter()
COOKIE = '__Host-traineros-login'


def wire(value):
    """Wire timestamps are UTC Z; revisions are decimal strings as in the SDK contract."""
    if isinstance(value,dict):
        return {key:str(item) if key=='revision' else wire(item) for key,item in value.items()}
    if isinstance(value,list):
        return [wire(item) for item in value]
    if isinstance(value,datetime):
        return value.astimezone(timezone.utc).isoformat().replace('+00:00','Z')
    if isinstance(value,UUID):
        return str(value)
    return value


class StrictBody(BaseModel):
    model_config = ConfigDict(extra='forbid', strict=True)


class LoginBegin(StrictBody):
    provider: Literal['fluxer']
    device_label: str = Field(min_length=1,max_length=64,pattern=r'^[^\x00-\x1f\x7f]+$')
    claim_challenge: str = Field(pattern=r'^[A-Za-z0-9_-]{43}$')
    claim_challenge_method: Literal['S256']


class LoginClaim(StrictBody):
    attempt_id: str = Field(pattern=r'^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$')
    claim_verifier: str = Field(pattern=r'^[A-Za-z0-9._~-]{43,128}$')


class Refresh(StrictBody):
    refresh_token: str = Field(pattern=r'^[A-Za-z0-9_-]{43}$')


def service(request):
    result = request.app.state.accounts
    if result is None:
        raise AuthError('provider_not_configured',503)
    return result


def bearer(request):
    header = request.headers.get('authorization','')
    if not header.startswith('Bearer ') or len(header)!=50:
        raise AuthError()
    return header[7:]


@router.post('/v1/auth/login-attempts',status_code=201)
def begin(body: LoginBegin, request: Request, idempotency_key: UUID = Header()):
    return wire(service(request).begin(idempotency_key,body.model_dump()))


@router.get('/v1/auth/fluxer/start',include_in_schema=False)
def start(request: Request, attempt_id: UUID, ticket: str=Query(pattern=r'^[A-Za-z0-9_-]{43}$')):
    cookie = service(request).start(attempt_id,ticket)
    response = HTMLResponse('<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
        '<title>Вход в TrainerOS</title><h1>Вход в TrainerOS</h1>'
        '<p>Введите 8 цифр, которые показаны в TrainerOS на вашем устройстве. '
        'Если вы не начинали вход, закройте это окно. Не вводите код, присланный другим человеком.</p>'
        '<form method="post" action="/v1/auth/fluxer/confirm">'
        f'<input type="hidden" name="attempt_id" value="{attempt_id}">'
        f'<input type="hidden" name="proof" value="{cookie}">'
        '<label>Код из TrainerOS <input name="code" inputmode="numeric" pattern="[0-9]{8}" minlength="8" maxlength="8" autocomplete="off" required></label>'
        '<button type="submit">Продолжить через Fluxer</button></form></html>')
    response.set_cookie(COOKIE,cookie,secure=True,httponly=True,samesite='lax',path='/',max_age=600)
    return response


@router.post('/v1/auth/fluxer/confirm',include_in_schema=False)
async def confirm(request: Request):
    accounts=service(request)
    if request.headers.get('origin')!=accounts.config.public_base:
        raise AuthError('confirmation_rejected',403)
    try:
        form=parse_qs((await request.body()).decode('ascii'),strict_parsing=True,max_num_fields=3)
        if set(form)!={'attempt_id','proof','code'} or any(len(values)!=1 for values in form.values()):
            raise ValueError()
        attempt=UUID(form['attempt_id'][0]);code=form['code'][0];proof=form['proof'][0]
        if len(code)!=8 or not code.isascii() or not code.isdigit() or len(proof)!=43:
            raise ValueError()
    except (ValueError,UnicodeError):
        raise AuthError('invalid_request',400) from None
    url=await run_in_threadpool(accounts.confirm,attempt,request.cookies.get(COOKIE,''),proof,code)
    return RedirectResponse(url,status_code=303)


@router.get('/v1/auth/callback/fluxer',include_in_schema=False)
def callback(request: Request, state: str=Query(pattern=r'^[A-Za-z0-9_-]{43}$'),
             code: str|None=Query(default=None,min_length=1,max_length=2048),
             error: str|None=Query(default=None,max_length=128)):
    service(request).callback(state,request.cookies.get(COOKIE,''),code,error)
    response = HTMLResponse('<!doctype html><html lang="ru"><meta charset="utf-8"><title>TrainerOS</title>'
        '<h1>Аккаунт Fluxer подтверждён</h1><p>Вернитесь в TrainerOS, чтобы завершить вход. Это окно можно закрыть.</p></html>')
    response.delete_cookie(COOKIE,secure=True,httponly=True,samesite='lax',path='/')
    return response


@router.post('/v1/auth/login-attempts/claim')
def claim(body: LoginClaim, request: Request):
    return wire(service(request).claim(UUID(body.attempt_id),body.claim_verifier))


@router.post('/v1/auth/refresh')
def refresh(body: Refresh, request: Request):
    return wire(service(request).refresh(body.refresh_token))


@router.get('/v1/auth/sessions')
def sessions(request: Request):
    return wire(service(request).sessions(bearer(request)))


@router.delete('/v1/auth/sessions/{session_id}',status_code=204)
def revoke(session_id: UUID, request: Request):
    service(request).revoke(bearer(request),session_id)
    return Response(status_code=204)


@router.get('/v1/me')
def me(request: Request):
    return wire(service(request).me(bearer(request)))
