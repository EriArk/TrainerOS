# SPDX-License-Identifier: GPL-3.0-or-later
"""Operator-only immutable registration. No client-supplied executable validators."""
import hashlib
import json
from pathlib import Path
import psycopg
from ..commands import CODE, canonical

def register(database_url,path):
    raw=Path(path).read_bytes()
    if len(raw)>8192: raise ValueError('registry_too_large')
    data=json.loads(raw)
    if set(data)!={'id','game_id','content_id','definition'} or any(not isinstance(data[k],str) or not CODE.fullmatch(data[k]) for k in ('id','game_id','content_id')):
        raise ValueError('invalid_registry')
    rule=data['definition']
    if set(rule)!={'validator','segments','minimum','maximum','direction','trust','label'} or rule['validator']!='scorecard.v1' or rule['trust']!='peer_confirmed':
        raise ValueError('unsupported_validator')
    if any(type(rule[k]) is not int for k in ('segments','minimum','maximum')) or not 1<=rule['segments']<=18 or not 0<=rule['minimum']<=rule['maximum']<=1000000000 or rule['direction'] not in ('asc','desc') or not isinstance(rule['label'],str) or not 1<=len(rule['label'])<=80:
        raise ValueError('invalid_score_policy')
    from psycopg.types.json import Jsonb
    checksum=hashlib.sha256(canonical(data)).hexdigest()
    with psycopg.connect(database_url) as conn:
        conn.execute('SELECT pg_advisory_xact_lock(1414680148)')
        old=conn.execute('SELECT rules_hash FROM trainer_backend.game_rulesets WHERE id=%s',(data['id'],)).fetchone()
        if old:
            if old[0]!=checksum: raise ValueError('immutable_ruleset_conflict')
        else:
            conn.execute('INSERT INTO trainer_backend.game_rulesets(id,game_id,content_id,definition,rules_hash) VALUES(%s,%s,%s,%s,%s)',
                (data['id'],data['game_id'],data['content_id'],Jsonb(rule),checksum))
            conn.execute('INSERT INTO trainer_backend.board_revisions(ruleset_id) VALUES(%s)',(data['id'],))
    return checksum


def void_match(database_url,match_id,operation_id):
    """Privileged maintenance only; append a decision and invalidate boards atomically."""
    from uuid import UUID,uuid5,NAMESPACE_URL
    from psycopg.rows import dict_row
    from ..commands import Principal,Outcome,run_command
    from .service import BoundDatabase,decision,require
    actor=uuid5(NAMESPACE_URL,'traineros-backend:local-maintenance-operator')
    with psycopg.connect(database_url,row_factory=dict_row) as conn:
        conn.execute("INSERT INTO trainer_backend.principals(id,kind,state) VALUES(%s,'operator','active') ON CONFLICT DO NOTHING",(actor,))
        def apply(c,receipt):
            row=c.execute('SELECT * FROM trainer_backend.matches WHERE id=%s FOR UPDATE',(UUID(match_id),)).fetchone()
            require(row is not None,'not_found',404)
            if row['state']!='voided':decision(c,row,'voided','operator_void')
            return Outcome('completed',row['id'])
        return run_command(BoundDatabase(conn),Principal(actor,'operator'),'results.void',operation_id,{'match_id':match_id},apply)
