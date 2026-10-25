# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded peer-confirmed scorecards, not authoritative game simulation.

Session credentials remain in the host. Grants bind one account session and one
immutable ruleset. Local app names are namespaces, never publisher attestation.
"""
from contextlib import contextmanager
from uuid import UUID, uuid4
import hashlib
from psycopg.types.json import Jsonb
from ..accounts.provider import AuthError
from ..accounts.service import digest
from ..commands import Principal, Outcome, Conflict, canonical, run_command
from ..jobs import enqueue
from ..events import EventSpec, publish


class BoundDatabase:
    def __init__(self, conn): self.conn = conn
    @contextmanager
    def transaction(self): yield self.conn


def problem(code, status=409):
    raise AuthError(code, status)


def require(condition, code='conflict', status=409):
    if not condition: problem(code, status)


def validate_event(payload):
    require(set(payload)=={'state','trust'} and payload['trust']=='peer_confirmed', 'invalid_event')


DECIDED = EventSpec('results.decided', 'results', validate_event)


def decision(conn, match, state, reason):
    row = conn.execute('''UPDATE trainer_backend.matches SET state=%s,revision=revision+1
        WHERE id=%s RETURNING revision''', (state,match['id'])).fetchone()
    conn.execute('''INSERT INTO trainer_backend.result_decisions(id,match_id,revision,state,reason)
        VALUES(%s,%s,%s,%s,%s)''',(uuid4(),match['id'],row['revision'],state,reason))
    conn.execute('UPDATE trainer_backend.board_revisions SET revision=revision+1 WHERE ruleset_id=%s',(match['ruleset_id'],))
    publish(conn,DECIDED,uuid4(),match['id'],row['revision'],{'state':state,'trust':'peer_confirmed'})


def validate_scores(conn, match, payload):
    rule=conn.execute('SELECT definition FROM trainer_backend.game_rulesets WHERE id=%s',(match['ruleset_id'],)).fetchone()['definition']
    roster=conn.execute('SELECT player_id,side FROM trainer_backend.match_participants WHERE match_id=%s ORDER BY side,player_id',(match['id'],)).fetchall()
    sides={r['side'] for r in roster}
    cards=payload['scores']
    if len(cards)!=len(sides) or {c['side'] for c in cards}!=sides: return None
    for card in cards:
        if len(card['values'])!=rule['segments'] or any(not rule['minimum']<=x<=rule['maximum'] for x in card['values']): return None
    totals={c['side']:sum(c['values']) for c in cards}
    result=[]
    for side,score in totals.items():
        members=sorted(str(r['player_id']) for r in roster if r['side']==side)
        subject=members[0] if match['mode']=='individual' else hashlib.sha256(','.join(members).encode()).hexdigest()
        place=1+sum(other<score if rule['direction']=='asc' else other>score for other in totals.values())
        outcome='won' if place==1 and list(totals.values()).count(score)==1 else ('draw' if place==1 else 'lost')
        result.append((subject,members,score,place,outcome))
    return result


def verify_result(conn, job):
    submission=conn.execute('SELECT * FROM trainer_backend.result_submissions WHERE id=%s',(job.resource_id,)).fetchone()
    match=conn.execute('SELECT * FROM trainer_backend.matches WHERE id=%s FOR UPDATE',(submission['match_id'],)).fetchone()
    if submission['state']!='pending': return
    if match['state'] not in ('active','confirmed'):
        conn.execute("UPDATE trainer_backend.result_submissions SET state='rejected' WHERE id=%s",(submission['id'],))
        return
    outcomes=validate_scores(conn,match,submission['payload'])
    conn.execute('UPDATE trainer_backend.result_submissions SET state=%s WHERE id=%s',('accepted' if outcomes else 'rejected',submission['id']))
    if outcomes is None:
        decision(conn,match,'rejected','invalid_scorecard')
        return
    hashes=conn.execute('SELECT DISTINCT payload_hash FROM trainer_backend.result_submissions WHERE match_id=%s',(match['id'],)).fetchall()
    if len(hashes)>1:
        decision(conn,match,'disputed','conflicting_scorecards')
        return
    confirmed=conn.execute("SELECT count(DISTINCT player_id) AS n FROM trainer_backend.result_submissions WHERE match_id=%s AND state='accepted'",(match['id'],)).fetchone()['n']
    participants=conn.execute('SELECT count(*) AS n FROM trainer_backend.match_participants WHERE match_id=%s',(match['id'],)).fetchone()['n']
    if confirmed==participants and match['state']=='active':
        for subject,members,score,place,outcome in outcomes:
            conn.execute('''INSERT INTO trainer_backend.result_outcomes(match_id,subject,members,score,place,outcome)
                VALUES(%s,%s,%s,%s,%s,%s)''',(match['id'],subject,Jsonb(members),score,place,outcome))
        decision(conn,match,'confirmed','all_participants_agree')


class Play:
    def __init__(self, db, accounts): self.db,self.accounts=db,accounts

    def rules(self):
        with self.db.transaction() as conn:
            return {'items':conn.execute('SELECT * FROM trainer_backend.game_rulesets WHERE active ORDER BY id LIMIT 100').fetchall()}

    def command(self, conn, who, kind, operation, payload, handler):
        try:
            return run_command(BoundDatabase(conn),Principal(who,'player'),kind,operation,payload,handler)
        except Conflict: problem('operation_payload_conflict')

    def grant(self, access, body):
        with self.db.transaction() as conn:
            who=self.accounts.authenticate(conn,access)
            rule=conn.execute('SELECT * FROM trainer_backend.game_rulesets WHERE id=%s AND active',(body['ruleset_id'],)).fetchone()
            require(rule is not None,'unsupported_ruleset',422)
            def create(c,receipt):
                # Account row serializes grant/create quotas across sessions and API replicas.
                c.execute('SELECT id FROM trainer_backend.players WHERE id=%s FOR UPDATE',(who.player_id,))
                count=c.execute('''SELECT count(*) AS n FROM trainer_backend.play_grants WHERE session_id=%s
                    AND NOT revoked AND expires_at>clock_timestamp()''',(who.session_id,)).fetchone()['n']
                require(count<16,'rate_limited',429)
                key=uuid4(); token=self.accounts.secret('play-grant',key)
                c.execute('''INSERT INTO trainer_backend.play_grants(id,session_id,player_id,app_id,ruleset_id,token_hash)
                    VALUES(%s,%s,%s,%s,%s,%s)''',(key,who.session_id,who.player_id,body['app_id'],body['ruleset_id'],digest(token)))
                return Outcome('completed',key)
            result=self.command(conn,who.player_id,'accounts.play_grant',body['operation_id'],{**body,'session_id':str(who.session_id)},create)
            grant=conn.execute('SELECT id,expires_at,revoked,expires_at>clock_timestamp() AS live FROM trainer_backend.play_grants WHERE id=%s',(result['resource_id'],)).fetchone()
            require(grant['live'] and not grant['revoked'],'grant_expired',401)
            return {'grant_id':grant['id'],'grant_token':self.accounts.secret('play-grant',grant['id']),
                    'expires_at':grant['expires_at'],'ruleset':rule,'trust':'peer_confirmed'}

    def authenticate(self, conn, token):
        row=conn.execute('''SELECT g.* FROM trainer_backend.play_grants g JOIN trainer_backend.device_sessions s ON s.id=g.session_id
            JOIN trainer_backend.game_rulesets r ON r.id=g.ruleset_id
            WHERE g.token_hash=%s AND NOT g.revoked AND g.expires_at>clock_timestamp()
            AND r.active AND s.revoked_at IS NULL AND s.expires_at>clock_timestamp() FOR SHARE OF s,g''',(digest(token),)).fetchone()
        require(row is not None,'unauthenticated',401)
        self.accounts.active(conn,row['player_id'])
        return row

    def execute(self, token, body):
        with self.db.transaction() as conn:
            grant=self.authenticate(conn,token)
            action=body['action']; payload=body['payload']; operation=body['operation_id']
            if action=='history':
                return {'items':conn.execute('''SELECT m.id,m.state,m.mode,m.revision,m.deadline FROM trainer_backend.matches m
                    JOIN trainer_backend.match_participants p ON p.match_id=m.id WHERE p.player_id=%s AND m.ruleset_id=%s
                    ORDER BY m.created_at DESC,m.id LIMIT 50''',(grant['player_id'],grant['ruleset_id'])).fetchall()}
            if action=='get': return self.match(conn,grant,payload['match_id'])
            if action=='stats':
                row=conn.execute('''SELECT count(*) AS matches,count(*) FILTER(WHERE o.outcome='won') AS wins,
                    count(*) FILTER(WHERE o.outcome='draw') AS draws,count(*) FILTER(WHERE o.outcome='lost') AS losses
                    FROM trainer_backend.result_outcomes o JOIN trainer_backend.matches m ON m.id=o.match_id
                    WHERE m.ruleset_id=%s AND m.state='confirmed' AND o.members ? %s''',(grant['ruleset_id'],str(grant['player_id']))).fetchone()
                row['unique_defeated']=conn.execute('''SELECT count(DISTINCT other_player) AS n FROM trainer_backend.result_outcomes o
                    JOIN trainer_backend.matches m ON m.id=o.match_id JOIN trainer_backend.result_outcomes other ON other.match_id=o.match_id AND other.subject<>o.subject
                    CROSS JOIN LATERAL jsonb_array_elements_text(other.members) other_player
                    WHERE m.ruleset_id=%s AND m.state='confirmed' AND o.outcome='won' AND o.members ? %s''',(grant['ruleset_id'],str(grant['player_id']))).fetchone()['n']
                return row
            if action=='revoke':
                # Lock order remains session -> grant. No secret reaches the game.
                conn.execute('UPDATE trainer_backend.play_grants SET revoked=true WHERE id=%s',(grant['id'],))
                return {'state':'revoked'}
            def mutate(c,receipt):
                if action=='create': return self.create(c,grant,payload)
                key=UUID(payload['match_id'])
                match=c.execute('SELECT *,deadline>clock_timestamp() AS live FROM trainer_backend.matches WHERE id=%s AND ruleset_id=%s FOR UPDATE',
                    (key,grant['ruleset_id'])).fetchone()
                require(match is not None,'not_found',404)
                require(match['live'],'match_expired')
                if action=='join':
                    require(match['state']=='waiting','roster_frozen')
                    side=payload['side']; require(0<=side<len(match['seats']),'invalid_side',422)
                    old=c.execute('SELECT side FROM trainer_backend.match_participants WHERE match_id=%s AND player_id=%s',(key,grant['player_id'])).fetchone()
                    if old: require(old['side']==side,'already_joined')
                    else:
                        count=c.execute('SELECT count(*) AS n FROM trainer_backend.match_participants WHERE match_id=%s AND side=%s',(key,side)).fetchone()['n']
                        require(count<match['seats'][side],'side_full')
                        c.execute('INSERT INTO trainer_backend.match_participants VALUES(%s,%s,%s)',(key,grant['player_id'],side))
                        c.execute('UPDATE trainer_backend.matches SET revision=revision+1 WHERE id=%s',(key,))
                else:
                    member=c.execute('SELECT 1 FROM trainer_backend.match_participants WHERE match_id=%s AND player_id=%s',(key,grant['player_id'])).fetchone()
                    require(member is not None,'forbidden',403)
                    if action in ('start','cancel'):
                        require(match['creator_id']==grant['player_id'],'forbidden',403)
                        require(match['state']=='waiting','already_started')
                        if action=='start':
                            count=c.execute('SELECT count(*) AS n FROM trainer_backend.match_participants WHERE match_id=%s',(key,)).fetchone()['n']
                            require(count==sum(match['seats']),'roster_incomplete')
                            c.execute("UPDATE trainer_backend.matches SET state='active',started_at=clock_timestamp(),deadline=clock_timestamp()+interval '2 hours',revision=revision+1 WHERE id=%s",(key,))
                        else: decision(c,match,'cancelled','creator_cancelled')
                    elif action=='submit':
                        require(match['state'] in ('active','confirmed'),'match_closed')
                        cards={'scores':sorted(payload['scores'],key=lambda x:x['side'])}
                        hashed=hashlib.sha256(canonical(cards)).hexdigest()
                        old=c.execute('SELECT id FROM trainer_backend.result_submissions WHERE match_id=%s AND player_id=%s AND payload_hash=%s',(key,grant['player_id'],hashed)).fetchone()
                        if old: return Outcome('completed',key)
                        sub=uuid4()
                        c.execute('INSERT INTO trainer_backend.result_submissions(id,match_id,player_id,payload,payload_hash) VALUES(%s,%s,%s,%s,%s)',(sub,key,grant['player_id'],Jsonb(cards),hashed))
                        # Withdraw an already published result immediately; never wait for a worker to hide it.
                        different=c.execute('SELECT 1 FROM trainer_backend.result_submissions WHERE match_id=%s AND payload_hash<>%s LIMIT 1',(key,hashed)).fetchone()
                        if different: decision(c,match,'disputed','conflicting_scorecards')
                        enqueue(c,'results.verify',sub,receipt)
                return Outcome('completed',key)
            result=self.command(conn,grant['player_id'],'results.'+action,operation,
                {'ruleset_id':grant['ruleset_id'],'payload':payload},mutate)
            return {'receipt_id':result['id'],'match':self.match(conn,grant,result['resource_id'])}

    def create(self,conn,grant,payload):
        seats=payload['seats']; mode=payload['mode']
        require((mode=='individual' and 2<=len(seats)<=16 and all(n==1 for n in seats)) or
                (mode=='teams' and 2<=len(seats)<=8 and all(n in (2,3) for n in seats) and sum(seats)<=16),'invalid_roster',422)
        conn.execute('SELECT id FROM trainer_backend.players WHERE id=%s FOR UPDATE',(grant['player_id'],))
        n=conn.execute("SELECT count(*) AS n FROM trainer_backend.matches WHERE creator_id=%s AND created_at>clock_timestamp()-interval '1 hour'",(grant['player_id'],)).fetchone()['n']
        require(n<12,'rate_limited',429)
        key=uuid4()
        conn.execute('INSERT INTO trainer_backend.matches(id,ruleset_id,creator_id,mode,seats) VALUES(%s,%s,%s,%s,%s)',(key,grant['ruleset_id'],grant['player_id'],mode,Jsonb(seats)))
        conn.execute('INSERT INTO trainer_backend.match_participants VALUES(%s,%s,0)',(key,grant['player_id']))
        return Outcome('completed',key)

    def match(self,conn,grant,key):
        row=conn.execute('''SELECT m.*,m.deadline<=clock_timestamp() AND m.state IN ('waiting','active') AS expired
            FROM trainer_backend.matches m JOIN trainer_backend.match_participants p ON p.match_id=m.id
            WHERE m.id=%s AND p.player_id=%s AND m.ruleset_id=%s''',(key,grant['player_id'],grant['ruleset_id'])).fetchone()
        require(row is not None,'not_found',404)
        row['participants']=conn.execute('SELECT player_id,side FROM trainer_backend.match_participants WHERE match_id=%s ORDER BY side,player_id',(key,)).fetchall()
        row['submissions']=conn.execute('''SELECT s.player_id,s.state,j.state AS verification_job,j.error_code FROM trainer_backend.result_submissions s
            LEFT JOIN trainer_backend.jobs j ON j.resource_id=s.id AND j.kind='results.verify' WHERE s.match_id=%s ORDER BY s.player_id,s.id''',(key,)).fetchall()
        row['decisions']=conn.execute('SELECT revision,state,reason FROM trainer_backend.result_decisions WHERE match_id=%s ORDER BY revision',(key,)).fetchall()
        row['outcomes']=conn.execute('SELECT subject,members,score,place,outcome FROM trainer_backend.result_outcomes WHERE match_id=%s ORDER BY place,subject',(key,)).fetchall()
        row['trust']='peer_confirmed' if row['state']=='confirmed' else 'unconfirmed'
        return row

    def board(self,ruleset,mode,kind,offset,limit,revision=None,around=None):
        with self.db.transaction() as conn:
            # One consistent snapshot: revision and page cannot observe different commits.
            conn.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')
            rule=conn.execute('SELECT definition FROM trainer_backend.game_rulesets WHERE id=%s AND active',(ruleset,)).fetchone()
            require(rule is not None,'not_found',404)
            current=conn.execute('SELECT revision FROM trainer_backend.board_revisions WHERE ruleset_id=%s',(ruleset,)).fetchone()['revision']
            require(revision is None or revision==str(current),'stale_revision')
            direction='ASC' if rule['definition']['direction']=='asc' else 'DESC'
            metric=('min(o.score)' if direction=='ASC' else 'max(o.score)') if kind=='records' else "count(*) FILTER(WHERE o.outcome='won')"
            order=direction if kind=='records' else 'DESC'
            # Only registered enums choose SQL syntax, never request strings.
            query=f'''WITH grouped AS (SELECT o.subject,o.members,{metric} AS value,count(*) AS matches,
                count(*) FILTER(WHERE o.outcome='won') AS wins FROM trainer_backend.result_outcomes o
                JOIN trainer_backend.matches m ON m.id=o.match_id WHERE m.ruleset_id=%s AND m.mode=%s AND m.state='confirmed'
                GROUP BY o.subject,o.members), ranked AS (SELECT *,rank() OVER(ORDER BY value {order}) AS place,
                row_number() OVER(ORDER BY value {order},subject)-1 AS position,count(*) OVER() AS total FROM grouped)
                SELECT * FROM ranked'''
            if around:
                center=conn.execute(query+' WHERE members ? %s ORDER BY position LIMIT 1',(ruleset,mode,str(around))).fetchone()
                offset=max(0,center['position']-limit//2) if center else 0
            rows=conn.execute(query+' ORDER BY position OFFSET %s LIMIT %s',(ruleset,mode,offset,limit)).fetchall()
            total=rows[0]['total'] if rows else 0
            from ..accounts.profiles import public_profiles
            profiles=public_profiles(conn,[player for row in rows for player in row['members']])
            return {'ruleset_id':ruleset,'mode':mode,'kind':kind,'trust':'peer_confirmed','revision':str(current),'profiles':profiles,
                    'items':rows,'offset':offset,'next_offset':offset+limit if offset+limit<total else None}
