# SPDX-License-Identifier: GPL-3.0-or-later
from dataclasses import dataclass, field
import os
import json
from pathlib import Path
from .accounts.provider import OAuthConfig


@dataclass(frozen=True)
class Settings:
    database_url: str = field(repr=False)
    environment: str = 'local'
    pool_size: int = 4
    port: int = 19120
    host: str = '127.0.0.1'
    oauth: OAuthConfig | None = field(default=None,repr=False)

    def __post_init__(self):
        if not self.database_url:
            raise ValueError('database_url_required')
        if self.environment not in ('local', 'test', 'server'):
            raise ValueError('foundation_not_approved_for_deployment')
        if self.host not in ('127.0.0.1', '0.0.0.0'):
            raise ValueError('invalid_bind_address')
        if not 1 <= self.pool_size <= 16 or not 1024 <= self.port <= 65535:
            raise ValueError('configuration_out_of_bounds')

    @classmethod
    def from_env(cls):
        url_file = os.environ.get('TRAINEROS_BACKEND_DATABASE_URL_FILE')
        url = Path(url_file).read_text().strip() if url_file else os.environ.get('TRAINEROS_BACKEND_DATABASE_URL', '')
        oauth_file = os.environ.get('TRAINEROS_BACKEND_OAUTH_CONFIG_FILE')
        oauth = OAuthConfig(**json.loads(Path(oauth_file).read_text())) if oauth_file else None
        return cls(url,
                   os.environ.get('TRAINEROS_BACKEND_ENV', 'local'),
                   int(os.environ.get('TRAINEROS_BACKEND_POOL_SIZE', '4')),
                   int(os.environ.get('TRAINEROS_BACKEND_PORT', '19120')),
                   os.environ.get('TRAINEROS_BACKEND_HOST', '127.0.0.1'), oauth)
