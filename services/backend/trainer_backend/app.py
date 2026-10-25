# SPDX-License-Identifier: GPL-3.0-or-later
from contextlib import asynccontextmanager
from dataclasses import asdict
from uuid import uuid4

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from fastapi.exceptions import RequestValidationError

from . import __version__
from .config import Settings
from .db import Database
from .modules import MODULES
from .accounts.provider import AuthError, FluxerProvider
from .accounts.service import Accounts
from .accounts.routes import router
from .accounts.limits import AuthLimits


def create_app(settings=None, *, provider=None):
    settings = settings or Settings.from_env()

    @asynccontextmanager
    async def lifespan(app):
        app.state.database = Database(settings.database_url, settings.pool_size)
        app.state.accounts = Accounts(app.state.database,settings.oauth,provider or FluxerProvider(settings.oauth)) if settings.oauth else None
        try:
            yield
        finally:
            app.state.database.close()

    app = FastAPI(title='TrainerOS Backend foundation', version=__version__,
                  lifespan=lifespan, docs_url=None, redoc_url=None)
    app.add_middleware(AuthLimits)
    app.include_router(router)

    @app.exception_handler(AuthError)
    async def auth_error(request, error):
        return JSONResponse(status_code=error.status,content={'code':error.code,'message':'Authentication request could not be completed',
            'request_id':request.state.request_id,'retryable':error.status==429},
            headers={'Retry-After':'60'} if error.status==429 else ({'WWW-Authenticate':'Bearer'} if error.status==401 else None))

    @app.exception_handler(RequestValidationError)
    async def invalid_request(request,error):
        # Pydantic's default error detail echoes rejected credential values.
        return JSONResponse(status_code=422,content={'code':'invalid_request','message':'Invalid request',
            'request_id':request.state.request_id,'retryable':False})

    @app.middleware('http')
    async def metadata(request: Request, call_next):
        request_id = str(uuid4())
        request.state.request_id = request_id
        try:
            response = await call_next(request)
        except Exception:
            response = JSONResponse(status_code=503, content={'code': 'service_unavailable',
                'message': 'Backend unavailable', 'request_id': request_id, 'retryable': True})
        response.headers['X-Request-ID'] = request_id
        response.headers['Cache-Control'] = 'no-store'
        response.headers['X-Content-Type-Options'] = 'nosniff'
        # A navigate-mode form POST under no-referrer sends Origin: null.
        # Keep the confirmation form same-origin without leaking its URL to Fluxer.
        response.headers['Referrer-Policy'] = ('same-origin'
            if request.url.path == '/v1/auth/fluxer/start' and response.status_code == 200
            else 'no-referrer')
        response.headers['Content-Security-Policy'] = "default-src 'none'; form-action 'self' https://web.fluxer.app; frame-ancestors 'none'; base-uri 'none'"
        return response

    @app.get('/health/live')
    def live():
        return {'status': 'alive', 'version': __version__}

    @app.get('/health/ready')
    def ready(request: Request):
        try:
            request.app.state.database.ready()
        except Exception:
            return JSONResponse(status_code=503, content={'code': 'service_unavailable',
                'message': 'Database or schema unavailable',
                'request_id': request.state.request_id, 'retryable': True})
        return {'status': 'ready', 'scope': 'accounts-and-foundation' if settings.oauth else 'foundation-only'}

    @app.get('/v1/capabilities')
    def capabilities():
        modules=[asdict(module) for module in MODULES]
        modules[0]['state']='configured' if settings.oauth else 'provider_not_configured'
        return {'version': __version__, 'capabilities': {'accounts.fluxer_login':1,'accounts.sessions':1,'accounts.native_return':1} if settings.oauth else {},
                'modules': modules}

    # Draft OpenAPI operations are NOT mounted as success-returning placeholders.
    return app
