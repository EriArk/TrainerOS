"""Explicit server maintenance. Owner DSN stays inside the maintenance container."""
import argparse
from pathlib import Path
from psycopg.conninfo import make_conninfo
from trainer_backend.play.registry import register,void_match
from trainer_backend.economy.maintenance import grant_test_sparks

parser=argparse.ArgumentParser()
sub=parser.add_subparsers(dest='command',required=True)
reg=sub.add_parser('register-ruleset');reg.add_argument('file')
void=sub.add_parser('void-match');void.add_argument('match');void.add_argument('operation')
fund=sub.add_parser('grant-test-sparks')
fund.add_argument('--player',required=True)
fund.add_argument('--subject',required=True)
fund.add_argument('--amount',required=True,type=int)
fund.add_argument('--reason',required=True)
fund.add_argument('--operation',required=True)
args=parser.parse_args()
owner=make_conninfo(host='db',dbname='traineros_backend',user='backend_owner',
    password=Path('/run/secrets/owner-password').read_text().strip())
try:
    if args.command=='register-ruleset':print('rules_hash='+register(owner,args.file))
    elif args.command=='void-match':print('void_receipt='+str(void_match(owner,args.match,args.operation)['id']))
    else:print('grant_receipt='+str(grant_test_sparks(owner,args.player,args.subject,args.amount,args.reason,args.operation)['id']))
except Exception:
    raise SystemExit('maintenance_failed: verify arguments and immutable registry state') from None
