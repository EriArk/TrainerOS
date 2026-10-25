"""Explicit server maintenance. Owner DSN stays inside the maintenance container."""
import argparse
from pathlib import Path
from psycopg.conninfo import make_conninfo
from trainer_backend.play.registry import register,void_match

parser=argparse.ArgumentParser()
sub=parser.add_subparsers(dest='command',required=True)
reg=sub.add_parser('register-ruleset');reg.add_argument('file')
void=sub.add_parser('void-match');void.add_argument('match');void.add_argument('operation')
args=parser.parse_args()
owner=make_conninfo(host='db',dbname='traineros_backend',user='backend_owner',
    password=Path('/run/secrets/owner-password').read_text().strip())
try:
    if args.command=='register-ruleset':print('rules_hash='+register(owner,args.file))
    else:print('void_receipt='+str(void_match(owner,args.match,args.operation)['id']))
except Exception:
    raise SystemExit('maintenance_failed: verify arguments and immutable registry state') from None
