# SPDX-License-Identifier: GPL-3.0-or-later
import base64
from dataclasses import dataclass
import hashlib
import hmac
import secrets
import re
from uuid import uuid4
from urllib.parse import urlencode

from ..commands import Principal, canonical, require_principal, Forbidden
from ..events import EventSpec,publish
from .provider import AuthError
from .profiles import public_profiles, update_name, update_avatar, avatar_recipe


def digest(value):
    return hashlib.sha256(value.encode()).hexdigest()


def challenge(value):
    return base64.urlsafe_b64encode(hashlib.sha256(value.encode()).digest()).decode().rstrip('=')


@dataclass(frozen=True)
class Identity:
    player_id: object
    session_id: object


def validate_revocation(payload):
    if set(payload)!={'expected_revision'} or not isinstance(payload['expected_revision'],str) or not re.fullmatch(r'[1-9][0-9]{0,18}',payload['expected_revision']):
        raise ValueError('invalid_revocation_event')


SESSION_REVOKED=EventSpec('account.session_revoked','accounts',validate_revocation)


class Accounts:
    def __init__(self, db, config, provider):
        self.db, self.config, self.provider = db, config, provider

    def secret(self, purpose, attempt_id):
        return base64.urlsafe_b64encode(hmac.digest(self.config.key.encode(),
            (purpose+':'+str(attempt_id)).encode(), 'sha256')).decode().rstrip('=')

    def verification_code(self,attempt):
        return str(int.from_bytes(hmac.digest(self.config.key.encode(),('confirm:'+str(attempt)).encode(),'sha256'),'big')%100000000).zfill(8)

    def audit(self, conn, player, action, resource):
        conn.execute('INSERT INTO trainer_backend.audit_events(id,actor_id,action,resource_id) VALUES(%s,%s,%s,%s)',
                     (uuid4(),player,action,resource))

    def active(self, conn, player):
        try:
            require_principal(conn, Principal(player,'player'))
        except Forbidden:
            raise AuthError() from None

    def begin(self, operation_id, payload):
        uri = payload.get('return_uri')
        if uri is not None and not re.fullmatch(r'http://127\.0\.0\.1:([1-9][0-9]{3,4})/traineros/auth/[A-Za-z0-9_-]{43}',uri):
            raise AuthError('invalid_request',400)
        if uri and not 1024 <= int(uri.split(':')[2].split('/')[0]) <= 65535:
            raise AuthError('invalid_request',400)
        payload_hash = hashlib.sha256(canonical(payload)).hexdigest()
        attempt = uuid4()
        with self.db.transaction() as conn:
            # Serialize bounded creation across API instances, including retries.
            conn.execute('SELECT pg_advisory_xact_lock(1414680147)')
            old = conn.execute('SELECT *,expires_at>clock_timestamp() AS live FROM trainer_backend.login_attempts WHERE operation_id=%s', (operation_id,)).fetchone()
            if old:
                if old['payload_hash'] != payload_hash:
                    raise AuthError('conflict',409)
                if not old['live'] or old['status']!='created':
                    raise AuthError('login_restart_required',409)
                row = old
            else:
                n = conn.execute("SELECT count(*) AS n FROM trainer_backend.login_attempts WHERE created_at>clock_timestamp()-interval '1 minute'").fetchone()['n']
                if n>=60:
                    raise AuthError('rate_limited',429)
                row = conn.execute('''INSERT INTO trainer_backend.login_attempts
                    (id,operation_id,payload_hash,claim_challenge,device_label,state_hash,status,return_uri,expected_subject)
                    VALUES(%s,%s,%s,%s,%s,%s,'created',%s,%s) RETURNING id,expires_at''',
                    (attempt,operation_id,payload_hash,payload['claim_challenge'],payload['device_label'],
                     digest(self.secret('state',attempt)),uri,payload.get('expected_subject'))).fetchone()
        return {'attempt_id':row['id'], 'expires_at':row['expires_at'],
            'verification_code':self.verification_code(row['id']),
            'authorization_url':self.config.public_base+'/v1/auth/fluxer/start?attempt_id='+str(row['id'])+'&ticket='+self.secret('ticket',row['id'])}

    def start(self, attempt, ticket):
        if not hmac.compare_digest(ticket,self.secret('ticket',attempt)):
            raise AuthError()
        browser = secrets.token_urlsafe(32)
        with self.db.transaction() as conn:
            row = conn.execute('''UPDATE trainer_backend.login_attempts
                SET status=CASE WHEN return_uri IS NULL THEN 'confirming' ELSE 'authorizing' END,browser_hash=%s
                WHERE id=%s AND status='created' AND expires_at>clock_timestamp() RETURNING id,return_uri''',
                (digest(browser),attempt)).fetchone()
            if not row:
                raise AuthError('login_restart_required',409)
        return browser, (self.provider.authorization_url(self.secret('state',attempt),challenge(self.secret('pkce',attempt))) if row['return_uri'] else None)

    def confirm(self,attempt,browser,proof,code):
        if not browser or not hmac.compare_digest(browser,proof):
            raise AuthError()
        denied=False
        with self.db.transaction() as conn:
            row=conn.execute("SELECT * FROM trainer_backend.login_attempts WHERE id=%s AND status='confirming' AND expires_at>clock_timestamp() FOR UPDATE",(attempt,)).fetchone()
            if not row or not hmac.compare_digest(row['browser_hash'] or '',digest(browser)):
                raise AuthError()
            if not hmac.compare_digest(code,self.verification_code(attempt)):
                conn.execute("UPDATE trainer_backend.login_attempts SET confirm_failures=confirm_failures+1,status=CASE WHEN confirm_failures>=4 THEN 'failed' ELSE 'confirming' END WHERE id=%s",(attempt,))
                denied=True
            else:
                conn.execute("UPDATE trainer_backend.login_attempts SET status='authorizing' WHERE id=%s",(attempt,))
        if denied:
            raise AuthError('confirmation_rejected',403)
        return self.provider.authorization_url(self.secret('state',attempt),challenge(self.secret('pkce',attempt)))

    def callback(self, state, browser, code, error):
        with self.db.transaction() as conn:
            row = conn.execute('''SELECT * FROM trainer_backend.login_attempts
                WHERE state_hash=%s AND status='authorizing' AND expires_at>clock_timestamp() FOR UPDATE''', (digest(state),)).fetchone()
            if not row or not hmac.compare_digest(row['browser_hash'] or '',digest(browser)):
                raise AuthError()
            conn.execute("UPDATE trainer_backend.login_attempts SET status=%s WHERE id=%s", ('failed' if error or not code else 'verifying',row['id']))
        if error or not code:
            if row['return_uri']:
                return row['return_uri']+'?'+urlencode({'attempt_id':row['id'],'error':'login_cancelled'})
            raise AuthError('login_cancelled',400)
        try:
            # One-shot code exchange outside the database transaction; ambiguous failures require a new login.
            identity = self.provider.identify(code,self.secret('pkce',row['id']))
            subject = identity['subject']
            if row['expected_subject'] and subject != row['expected_subject']:
                raise AuthError('account_mismatch',409)
            handoff = secrets.token_urlsafe(32) if row['return_uri'] else None
            with self.db.transaction() as conn:
                current = conn.execute("SELECT id FROM trainer_backend.login_attempts WHERE id=%s AND status='verifying' AND expires_at>clock_timestamp() FOR UPDATE",(row['id'],)).fetchone()
                if not current:
                    raise AuthError('login_restart_required',409)
                player = conn.execute('SELECT trainer_backend.resolve_fluxer_player(%s,%s) AS id', (self.provider.instance,subject)).fetchone()['id']
                self.active(conn,player)
                update_name(conn,player,identity['display_name'])
                conn.execute("UPDATE trainer_backend.login_attempts SET status='verified',player_id=%s,handoff_hash=%s WHERE id=%s", (player,digest(handoff) if handoff else None,row['id']))
        except Exception:
            with self.db.transaction() as conn:
                conn.execute("UPDATE trainer_backend.login_attempts SET status='failed' WHERE id=%s AND status='verifying'", (row['id'],))
            if row['return_uri']:
                return row['return_uri']+'?'+urlencode({'attempt_id':row['id'],'error':'login_restart_required'})
            raise AuthError('login_restart_required',409) from None
        if row['return_uri']:
            return row['return_uri']+'?'+urlencode({'attempt_id':row['id'],'handoff_token':handoff})

    def issue(self, conn, session_id, player):
        access, refresh = secrets.token_urlsafe(32), secrets.token_urlsafe(32)
        row = conn.execute('''UPDATE trainer_backend.device_sessions SET access_hash=%s,
            access_expires_at=LEAST(clock_timestamp()+interval '15 minutes',expires_at)
            WHERE id=%s RETURNING access_expires_at''', (digest(access),session_id)).fetchone()
        conn.execute('INSERT INTO trainer_backend.refresh_credentials(token_hash,session_id) VALUES(%s,%s)', (digest(refresh),session_id))
        subject = conn.execute("SELECT subject FROM trainer_backend.external_identities WHERE player_id=%s AND instance=%s",(player,self.provider.instance)).fetchone()['subject']
        return {'player_id':player,'session_id':session_id,'fluxer_subject':subject,'access_token':access,'refresh_token':refresh,'expires_at':row['access_expires_at']}

    def claim(self, attempt, verifier, handoff=None):
        with self.db.transaction() as conn:
            row = conn.execute('''SELECT *,expires_at>clock_timestamp() AS live FROM trainer_backend.login_attempts
                WHERE id=%s FOR UPDATE''', (attempt,)).fetchone()
            if not row or not row['live'] or not hmac.compare_digest(row['claim_challenge'],challenge(verifier)):
                raise AuthError()
            if row['return_uri'] and (not handoff or not row['handoff_hash'] or not hmac.compare_digest(row['handoff_hash'],digest(handoff))):
                raise AuthError()
            if row['status'] in ('created','confirming','authorizing','verifying'):
                raise AuthError('authorization_pending',409)
            if row['status']!='verified':
                raise AuthError('login_restart_required',409)
            player = row['player_id']
            self.active(conn,player)
            conn.execute('SELECT id FROM trainer_backend.players WHERE id=%s FOR UPDATE',(player,))
            n = conn.execute('''SELECT count(*) AS n FROM trainer_backend.device_sessions WHERE player_id=%s
                AND revoked_at IS NULL AND expires_at>clock_timestamp()''',(player,)).fetchone()['n']
            if n>=10:
                raise AuthError('session_limit',409)
            session = uuid4()
            conn.execute('''INSERT INTO trainer_backend.device_sessions(id,player_id,device_label,access_hash,access_expires_at)
                VALUES(%s,%s,%s,%s,clock_timestamp())''',(session,player,row['device_label'],digest(secrets.token_urlsafe(32))))
            result = self.issue(conn,session,player)
            conn.execute("UPDATE trainer_backend.login_attempts SET status='claimed' WHERE id=%s",(attempt,))
            conn.execute('UPDATE trainer_backend.players SET revision=revision+1 WHERE id=%s',(player,))
            self.audit(conn,player,'accounts.session_created',session)
            return result

    def authenticate(self, conn, access):
        row = conn.execute('''SELECT id,player_id FROM trainer_backend.device_sessions
            WHERE access_hash=%s AND access_expires_at>clock_timestamp() AND expires_at>clock_timestamp()
            AND revoked_at IS NULL FOR SHARE''',(digest(access),)).fetchone()
        if not row:
            raise AuthError()
        self.active(conn,row['player_id'])
        return Identity(row['player_id'],row['id'])

    def refresh(self, token):
        replay = False
        with self.db.transaction() as conn:
            entry = conn.execute('SELECT session_id FROM trainer_backend.refresh_credentials WHERE token_hash=%s',(digest(token),)).fetchone()
            if not entry:
                raise AuthError()
            row = conn.execute('''SELECT *,expires_at>clock_timestamp() AS live FROM trainer_backend.device_sessions
                WHERE id=%s FOR UPDATE''',(entry['session_id'],)).fetchone()
            if not row or not row['live'] or row['revoked_at']:
                raise AuthError()
            self.active(conn,row['player_id'])
            current = conn.execute('SELECT consumed_at FROM trainer_backend.refresh_credentials WHERE token_hash=%s',(digest(token),)).fetchone()
            if current['consumed_at']:
                self.revoke_row(conn,row['player_id'],row['id'],'accounts.refresh_replay')
                replay = True
            else:
                conn.execute('UPDATE trainer_backend.refresh_credentials SET consumed_at=clock_timestamp() WHERE token_hash=%s',(digest(token),))
                result = self.issue(conn,row['id'],row['player_id'])
        # Commit revocation before rejecting a reused refresh token.
        if replay:
            raise AuthError()
        return result

    def revoke_row(self,conn,player,session,action):
        row=conn.execute('UPDATE trainer_backend.device_sessions SET revoked_at=clock_timestamp(),revision=revision+1 WHERE id=%s RETURNING revision',(session,)).fetchone()
        conn.execute('UPDATE trainer_backend.players SET revision=revision+1 WHERE id=%s',(player,))
        self.audit(conn,player,action,session)
        publish(conn,SESSION_REVOKED,uuid4(),session,row['revision'],{'expected_revision':str(row['revision']-1)})

    def sessions(self,access):
        with self.db.transaction() as conn:
            who = self.authenticate(conn,access)
            rows = conn.execute('''SELECT id AS session_id,device_label,created_at,expires_at,id=%s AS current
                FROM trainer_backend.device_sessions WHERE player_id=%s AND revoked_at IS NULL
                AND expires_at>clock_timestamp() ORDER BY created_at,id LIMIT 10''',(who.session_id,who.player_id)).fetchall()
            revision = conn.execute('SELECT revision FROM trainer_backend.players WHERE id=%s',(who.player_id,)).fetchone()['revision']
            return {'items':rows,'revision':revision}

    def me(self,access):
        with self.db.transaction() as conn:
            who = self.authenticate(conn,access)
            revision = conn.execute('SELECT revision FROM trainer_backend.players WHERE id=%s',(who.player_id,)).fetchone()['revision']
            return {'player_id':who.player_id,'status':'active','revision':revision}

    def profile(self, access, *, avatar=...):
        if avatar is not ...:
            avatar_recipe(avatar)
        with self.db.transaction() as conn:
            who = self.authenticate(conn,access)
            if avatar is not ...:
                update_avatar(conn,who.player_id,avatar)
            return public_profiles(conn,[who.player_id])[str(who.player_id)]

    def revoke(self,access,session):
        with self.db.transaction() as conn:
            who = self.authenticate(conn,access)
            row = conn.execute('SELECT id,revoked_at FROM trainer_backend.device_sessions WHERE id=%s AND player_id=%s FOR UPDATE',
                               (session,who.player_id)).fetchone()
            if not row:
                raise AuthError('not_found',404)
            if not row['revoked_at']:
                self.revoke_row(conn,who.player_id,session,'accounts.session_revoked')
