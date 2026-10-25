# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded in-process ingress protection; shared creation budget additionally lives in SQL."""
import asyncio
from collections import OrderedDict
import time
from uuid import uuid4

from fastapi.responses import JSONResponse


class AuthLimits:
    def __init__(self, app):
        self.app, self.clients = app, OrderedDict()

    async def __call__(self, scope, receive, send):
        if scope['type']!='http' or not (scope['path'].startswith('/v1/auth/') or scope['path']=='/v1/me'):
            return await self.app(scope,receive,send)
        async def reject(code,status):
            await JSONResponse({'code':code,'message':'Request rejected','retryable':status==429,
                'request_id':scope.get('state',{}).get('request_id',str(uuid4()))},status_code=status,
                headers={'Cache-Control':'no-store', **({'Retry-After':'60'} if status==429 else {})})(scope,receive,send)
        now = time.monotonic()
        client = (scope.get('client') or ('unknown',0))[0]
        start,n = self.clients.pop(client,(now,0))
        if now-start>=60:
            start,n = now,0
        self.clients[client]=(start,n+1)
        if len(self.clients)>1024:
            self.clients.popitem(last=False)
        if n>=120:
            return await reject('rate_limited',429)
        if len(scope.get('query_string',b''))>4096:
            return await reject('invalid_request',414)
        if scope['method'] in ('POST','PUT','PATCH'):
            body=bytearray()
            try:
                async with asyncio.timeout(10):
                    while True:
                        message=await receive()
                        if message['type']=='http.disconnect':
                            return
                        body.extend(message.get('body',b''))
                        if len(body)>16384:
                            return await reject('body_too_large',413)
                        if not message.get('more_body',False):
                            break
            except TimeoutError:
                return await reject('request_timeout',408)
            original_receive=receive
            sent=False
            async def buffered():
                nonlocal sent
                if not sent:
                    sent=True
                    return {'type':'http.request','body':bytes(body),'more_body':False}
                return await original_receive()
            receive=buffered
        await self.app(scope,receive,send)
