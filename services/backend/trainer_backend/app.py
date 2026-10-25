# SPDX-License-Identifier: GPL-3.0-or-later
from contextlib import asynccontextmanager
from dataclasses import asdict
from uuid import uuid4

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from . import __version__
from .config import Settings
from .db import Database
from .modules import MODULES


def create_app(settings=None):
    settings = settings or Settings.from_env()

    @asynccontextmanager
    async def lifespan(app):
        app.state.database = Database(settings.database_url, settings.pool_size)
        try:
            yield
        finally:
            app.state.database.close()

    app = FastAPI(title='TrainerOS Backend foundation', version=__version__,
                  lifespan=lifespan, docs_url=None, redoc_url=None)

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
        return {'status': 'ready', 'scope': 'foundation-only'}

    @app.get('/v1/capabilities')
    def capabilities():
        return {'version': __version__, 'capabilities': {},
                'modules': [asdict(module) for module in MODULES]}

    # Draft OpenAPI operations are NOT mounted as success-returning placeholders.
    return app
