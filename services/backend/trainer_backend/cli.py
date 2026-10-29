# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import os
import sys
import time

import uvicorn

from .app import create_app
from .config import Settings
from .db import Database, grant_runtime, migrate
from .jobs import Worker
from .play.service import verify_result
from .play.registry import register,void_match


def main():
    parser = argparse.ArgumentParser(description='TrainerOS Backend foundation')
    parser.add_argument('command', choices=('api', 'migrate', 'grant-runtime', 'worker','register-ruleset','void-match'))
    parser.add_argument('--file')
    parser.add_argument('--match')
    parser.add_argument('--operation')
    parser.add_argument('--once', action='store_true')
    parser.add_argument('--role')
    args = parser.parse_args()
    try:
        settings = Settings.from_env()
        if args.command in ('migrate', 'grant-runtime','register-ruleset','void-match'):
            migration_url = os.environ.get('TRAINEROS_BACKEND_MIGRATION_DATABASE_URL')
            if not migration_url:
                raise ValueError('migration_url_required')
            if args.command == 'migrate':
                print(f'migrations_applied={migrate(migration_url)}')
            elif args.command=='register-ruleset':
                print('registered_rules_hash='+register(migration_url,args.file))
            elif args.command=='void-match':
                print('void_receipt='+str(void_match(migration_url,args.match,args.operation)['id']))
            else:
                if not args.role:
                    raise ValueError('runtime_role_required')
                grant_runtime(migration_url, args.role)
                print('runtime_grants_applied')
        elif args.command == 'api':
            uvicorn.run(create_app(settings), host=settings.host, port=settings.port,
                        access_log=False, proxy_headers=False)
        else:
            db = Database(settings.database_url, settings.pool_size)
            try:
                db.ready()
                # Register real feature handlers at composition, never from a request/job payload.
                worker = Worker(db, {'results.verify':verify_result})
                support = None
                if settings.patreon:
                    from .accounts.service import Accounts
                    from .accounts.provider import FluxerProvider
                    from .economy.service import Progression
                    from .economy.patreon import PatreonProvider
                    from .economy.support_links import SupportLinks
                    support = SupportLinks(Progression(db, Accounts(db, settings.oauth, FluxerProvider(settings.oauth))), PatreonProvider(settings.patreon))
                while True:
                    state = worker.once()
                    if support:
                        support.once()
                    if args.once:
                        print(state)
                        break
                    time.sleep(1 if state=='idle' else 0.05)
            finally:
                db.close()
    except KeyboardInterrupt:
        return 0
    except Exception:
        # Configuration/driver messages can embed DSNs. Keep CLI failure output bounded.
        print('backend_start_failed: check configuration, database and migration state', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
