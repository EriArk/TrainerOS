# SPDX-License-Identifier: GPL-3.0-or-later
from typing import Literal, Annotated
from uuid import UUID
from fastapi import APIRouter, Request, Query
from pydantic import Field, TypeAdapter, ValidationError
from ..accounts.routes import StrictBody, bearer, service, wire
from ..accounts.provider import AuthError

router=APIRouter()
Id=Annotated[str,Field(pattern=r'^[a-z][a-z0-9_.-]{0,63}$')]
Key=Annotated[str,Field(pattern=r'^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$')]

class Grant(StrictBody):
    app_id: Id
    ruleset_id: Id
    operation_id: Key

class Create(StrictBody):
    mode: Literal['individual','teams']
    seats: list[Annotated[int,Field(ge=1,le=3)]]=Field(min_length=2,max_length=16)

class Match(StrictBody):
    match_id: Key

class Join(Match):
    side: int=Field(ge=0,le=15)

class Score(StrictBody):
    side: int=Field(ge=0,le=15)
    values: list[Annotated[int,Field(ge=0,le=1000000000)]]=Field(min_length=1,max_length=18)

class Submit(Match):
    scores: list[Score]=Field(min_length=2,max_length=16)

class Empty(StrictBody): pass

class Command(StrictBody):
    action: Literal['create','join','start','cancel','submit','get','history','stats','revoke']
    operation_id: Key
    payload: dict

PAYLOADS={'create':Create,'join':Join,'start':Match,'cancel':Match,'submit':Submit,'get':Match,'history':Empty,'stats':Empty,'revoke':Empty}

def play(request):
    service(request)
    return request.app.state.play

@router.get('/v1/games')
def games(request:Request):
    return wire(play(request).rules())

@router.post('/v1/play/grants')
def grant(request:Request,body:Grant):
    return wire(play(request).grant(bearer(request),body.model_dump()))

@router.post('/v1/play/commands')
def command(request:Request,body:Command):
    try: payload=TypeAdapter(PAYLOADS[body.action]).validate_python(body.payload).model_dump()
    except ValidationError: raise AuthError('invalid_request',422) from None
    return wire(play(request).execute(bearer(request),{**body.model_dump(),'payload':payload}))

@router.get('/v1/boards/{ruleset_id}/{mode}/{kind}')
def board(request:Request,ruleset_id:Id,mode:Literal['individual','teams'],kind:Literal['records','wins'],
          offset:int=Query(default=0,ge=0,le=10000),limit:int=Query(default=20,ge=1,le=50),
          revision:str|None=Query(default=None,pattern=r'^[1-9][0-9]{0,18}$'),around:UUID|None=None):
    return wire(play(request).board(ruleset_id,mode,kind,offset,limit,revision,around))
