# SPDX-License-Identifier: GPL-3.0-or-later
"""Owner/session-bound support OAuth and fenced, fresh membership verification."""
from datetime import datetime, timezone, timedelta
import hashlib
import hmac
import json
import re
import secrets
from urllib.parse import urlencode
from uuid import uuid4

from cryptography.fernet import Fernet
from psycopg.errors import UniqueViolation
from ..accounts.provider import AuthError
from ..accounts.service import challenge, digest
from ..commands import canonical
from .membership import Membership, record_verified, status as membership_status


class SupportLinks:
    def __init__(self, progression, provider=None):
        self.progression, self.provider = progression, provider
        self.db, self.accounts = progression.db, progression.accounts
        self.config = provider.config if provider else None
        self.cipher = Fernet(self.config.token_key.encode()) if self.config else None

    def configured(self):
        if not self.provider:
            raise AuthError('support_provider_not_configured', 503)

    def secret(self, purpose, attempt):
        return self.accounts.secret('support:' + purpose, attempt)

    def status(self, access):
        with self.db.transaction() as conn:
            actor = self.accounts.authenticate(conn, access)
            row = conn.execute('SELECT state,checked_at FROM trainer_backend.support_links WHERE player_id=%s', (actor.player_id,)).fetchone()
            return {'provider': 'patreon', 'available': bool(self.provider),
                    'state': row['state'] if row else 'unlinked',
                    'checked_at': row['checked_at'] if row else None,
                    **membership_status(conn, actor.player_id)}

    def begin(self, access, verifier_challenge, uri):
        self.configured()
        if (not re.fullmatch(r'[A-Za-z0-9_-]{43}', verifier_challenge)
                or not re.fullmatch(r'http://127\.0\.0\.1:([1-9][0-9]{3,4})/traineros/auth/[A-Za-z0-9_-]{43}', uri)
                or not 1024 <= int(uri.split(':')[2].split('/')[0]) <= 65535):
            raise AuthError('invalid_request', 400)
        attempt = uuid4()
        with self.db.transaction() as conn:
            actor = self.accounts.authenticate(conn, access)
            conn.execute('INSERT INTO trainer_backend.support_links(player_id) VALUES(%s) ON CONFLICT DO NOTHING', (actor.player_id,))
            row = conn.execute('SELECT * FROM trainer_backend.support_links WHERE player_id=%s FOR UPDATE', (actor.player_id,)).fetchone()
            n = conn.execute("SELECT count(*) AS n FROM trainer_backend.support_link_attempts WHERE player_id=%s AND created_at>clock_timestamp()-interval '1 minute'", (actor.player_id,)).fetchone()['n']
            if n >= 5:
                raise AuthError('rate_limited', 429)
            # A new attempt supersedes previous browser returns, including another device.
            generation = row['generation'] + 1
            conn.execute('UPDATE trainer_backend.support_links SET generation=%s,lease=NULL,lease_until=NULL WHERE player_id=%s', (generation, actor.player_id))
            conn.execute("UPDATE trainer_backend.support_link_attempts SET status='failed',credentials=NULL WHERE player_id=%s AND status NOT IN ('claimed','failed')", (actor.player_id,))
            result = conn.execute('''INSERT INTO trainer_backend.support_link_attempts
                (id,player_id,session_id,generation,claim_challenge,state_hash,ticket_hash,return_uri)
                VALUES(%s,%s,%s,%s,%s,%s,%s,%s) RETURNING expires_at''',
                (attempt, actor.player_id, actor.session_id, generation, verifier_challenge,
                 digest(self.secret('state', attempt)), digest(self.secret('ticket', attempt)), uri)).fetchone()
        return {'attempt_id': attempt, 'expires_at': result['expires_at'],
                'authorization_url': self.config.public_base + '/v1/support/patreon/start?' + urlencode({
                    'attempt_id': attempt, 'ticket': self.secret('ticket', attempt)})}

    def start(self, attempt, ticket):
        self.configured()
        browser = secrets.token_urlsafe(32)
        with self.db.transaction() as conn:
            row = conn.execute('''UPDATE trainer_backend.support_link_attempts SET status='authorizing',browser_hash=%s
                WHERE id=%s AND ticket_hash=%s AND status='created' AND expires_at>clock_timestamp() RETURNING id''',
                (digest(browser), attempt, digest(ticket))).fetchone()
            if not row:
                raise AuthError('support_restart_required', 409)
        return browser, self.provider.authorization_url(self.secret('state', attempt))

    def seal(self, tokens):
        value = dict(tokens)
        value['expires_at'] = (datetime.now(timezone.utc) + timedelta(seconds=value.pop('expires_in'))).isoformat()
        return self.cipher.encrypt(canonical(value)).decode()

    def callback(self, state, browser, code, error):
        self.configured()
        with self.db.transaction() as conn:
            row = conn.execute('''SELECT * FROM trainer_backend.support_link_attempts WHERE state_hash=%s
                AND status='authorizing' AND expires_at>clock_timestamp() FOR UPDATE''', (digest(state),)).fetchone()
            if not row or not browser or not hmac.compare_digest(row['browser_hash'] or '', digest(browser)):
                raise AuthError()
            conn.execute("UPDATE trainer_backend.support_link_attempts SET status='exchanging' WHERE id=%s", (row['id'],))
        outcome = {'attempt_id': row['id']}
        try:
            if error or not code:
                raise AuthError('support_cancelled', 400)
            sealed = self.seal(self.provider.tokens(code=code))
            handoff = secrets.token_urlsafe(32)
            with self.db.transaction() as conn:
                saved = conn.execute('''UPDATE trainer_backend.support_link_attempts SET status='verified',credentials=%s,handoff_hash=%s
                    WHERE id=%s AND status='exchanging' AND expires_at>clock_timestamp() RETURNING id''',
                    (sealed, digest(handoff), row['id'])).fetchone()
                if not saved:
                    raise AuthError('support_restart_required', 409)
            outcome['handoff_token'] = handoff
        except Exception:
            with self.db.transaction() as conn:
                conn.execute("UPDATE trainer_backend.support_link_attempts SET status='failed',credentials=NULL WHERE id=%s", (row['id'],))
            outcome['error'] = 'support_cancelled' if error else 'support_restart_required'
        return row['return_uri'] + '?' + urlencode(outcome)

    def claim(self, access, attempt, verifier, handoff):
        self.configured()
        with self.db.transaction() as conn:
            actor = self.accounts.authenticate(conn, access)
            link = conn.execute('SELECT * FROM trainer_backend.support_links WHERE player_id=%s FOR UPDATE', (actor.player_id,)).fetchone()
            row = conn.execute('''SELECT * FROM trainer_backend.support_link_attempts WHERE id=%s
                AND status='verified' AND expires_at>clock_timestamp() FOR UPDATE''', (attempt,)).fetchone()
            if (not row or not link or row['player_id'] != actor.player_id or row['session_id'] != actor.session_id
                    or row['generation'] != link['generation']
                    or not hmac.compare_digest(row['claim_challenge'], challenge(verifier))
                    or not handoff or not hmac.compare_digest(row['handoff_hash'] or '', digest(handoff))):
                raise AuthError('support_restart_required', 409)
            conn.execute("UPDATE trainer_backend.support_link_attempts SET status='claimed',credentials=NULL WHERE id=%s", (attempt,))
            # Credentials are provisional until identity is verified under this lease.
            lease = uuid4()
            conn.execute("UPDATE trainer_backend.support_links SET lease=%s,lease_until=clock_timestamp()+interval '45 seconds' WHERE player_id=%s", (lease, actor.player_id))
        self.verify(actor.player_id, lease, row['credentials'], access=access, session=actor.session_id)
        return self.status(access)

    def acquire(self, player, *, force=False):
        with self.db.transaction() as conn:
            row = conn.execute('''SELECT * FROM trainer_backend.support_links WHERE player_id=%s AND state='connected'
                AND (lease_until IS NULL OR lease_until<clock_timestamp())
                AND (%s OR next_check_at<=clock_timestamp()) FOR UPDATE''', (player, force)).fetchone()
            if not row:
                raise AuthError('support_check_pending', 409)
            lease = uuid4()
            conn.execute("UPDATE trainer_backend.support_links SET lease=%s,lease_until=clock_timestamp()+interval '45 seconds',next_check_at=clock_timestamp()+interval '10 minutes' WHERE player_id=%s", (lease, player))
        return lease, row['credentials']

    def receipt(self, conn, row, fact, evidence):
        revision = row['revision'] + 1
        record_verified(conn, 'patreon', Membership(row['player_id'], fact['subject'], str(uuid4()), revision,
            fact['multiplier'], fact['observed_at'], fact['valid_until'], hashlib.sha256(canonical(evidence)).hexdigest()), row['player_id'])
        conn.execute('UPDATE trainer_backend.support_links SET revision=%s WHERE player_id=%s', (revision, row['player_id']))

    def verify(self, player, lease, sealed, *, access=None, session=None):
        try:
            tokens = json.loads(self.cipher.decrypt(sealed.encode()))
            if datetime.fromisoformat(tokens['expires_at']) <= datetime.now(timezone.utc) + timedelta(minutes=1):
                # Consume refresh capability before exchanging. Ambiguous crashes require relinking,
                # never replaying a rotated refresh token or restoring an old credential.
                with self.db.transaction() as conn:
                    current = conn.execute("UPDATE trainer_backend.support_links SET credentials=NULL,state='needs_relink' WHERE player_id=%s AND lease=%s AND lease_until>clock_timestamp() RETURNING player_id", (player, lease)).fetchone()
                    if not current:
                        raise AuthError('support_restart_required', 409)
                sealed = self.seal(self.provider.tokens(refresh=tokens['refresh_token']))
                tokens = json.loads(self.cipher.decrypt(sealed.encode()))
            evidence = self.provider.identify(tokens['access_token'])
            fact = self.provider.entitlement(evidence)
            with self.db.transaction() as conn:
                if access:
                    actor = self.accounts.authenticate(conn, access)
                    if actor.player_id != player or actor.session_id != session:
                        raise AuthError()
                self.accounts.active(conn, player)
                row = conn.execute('''SELECT * FROM trainer_backend.support_links WHERE player_id=%s
                    AND lease=%s AND lease_until>clock_timestamp() FOR UPDATE''', (player, lease)).fetchone()
                if not row:
                    raise AuthError('support_restart_required', 409)
                if row['subject'] and row['subject'] != fact['subject']:
                    raise AuthError('support_identity_conflict', 409)
                self.receipt(conn, row, fact, evidence)
                conn.execute('''UPDATE trainer_backend.support_links SET subject=%s,member_id=%s,credentials=%s,state='connected',
                    checked_at=clock_timestamp(),next_check_at=clock_timestamp()+interval '10 minutes',lease=NULL,lease_until=NULL
                    WHERE player_id=%s''', (fact['subject'], fact['member_id'], sealed, player))
        except Exception as error:
            with self.db.transaction() as conn:
                conn.execute("UPDATE trainer_backend.support_links SET lease=NULL,lease_until=NULL,next_check_at=clock_timestamp()+interval '1 minute' WHERE player_id=%s AND lease=%s", (player, lease))
            if isinstance(error, UniqueViolation):
                raise AuthError('support_identity_conflict', 409) from None
            if isinstance(error, AuthError):
                raise
            raise AuthError('support_provider_unavailable', 503) from None

    def refresh(self, access):
        self.configured()
        with self.db.transaction() as conn:
            actor = self.accounts.authenticate(conn, access)
        lease, sealed = self.acquire(actor.player_id, force=True)
        self.verify(actor.player_id, lease, sealed, access=access, session=actor.session_id)
        return self.status(access)

    def disconnect(self, access):
        # Works even while provider configuration/network is unavailable.
        with self.db.transaction() as conn:
            actor = self.accounts.authenticate(conn, access)
            row = conn.execute('SELECT * FROM trainer_backend.support_links WHERE player_id=%s FOR UPDATE', (actor.player_id,)).fetchone()
            if row:
                now = datetime.now(timezone.utc)
                if row['subject']:
                    self.receipt(conn, row, {'subject': row['subject'], 'multiplier': 1, 'observed_at': now, 'valid_until': now}, {'action': 'disconnect'})
                conn.execute("UPDATE trainer_backend.support_links SET state='disconnected',credentials=NULL,lease=NULL,lease_until=NULL,generation=generation+1 WHERE player_id=%s", (actor.player_id,))
                conn.execute("UPDATE trainer_backend.support_link_attempts SET status='failed',credentials=NULL WHERE player_id=%s AND status NOT IN ('claimed','failed')", (actor.player_id,))
        return self.status(access)

    def webhook(self, raw, signature, event):
        self.configured()
        if (len(raw) > 16384 or not isinstance(signature, str) or not re.fullmatch(r'[0-9a-fA-F]{32}', signature)
                or not hmac.compare_digest(hmac.new(self.config.webhook_secret.encode(), raw, 'md5').hexdigest(), signature.lower())):
            raise AuthError('invalid_support_webhook', 401)
        if event not in ('members:create', 'members:update', 'members:delete', 'members:pledge:create', 'members:pledge:update', 'members:pledge:delete'):
            return
        try:
            member = json.loads(raw)['data']
            if member['type'] != 'member' or not re.fullmatch(r'[A-Za-z0-9-]{1,128}', member['id']):
                raise ValueError()
        except (ValueError, KeyError, TypeError):
            raise AuthError('invalid_support_webhook', 400) from None
        with self.db.transaction() as conn:
            # Webhook arrival order is NOT a membership version. Invalidate in-flight reads
            # and schedule a fresh authenticated API read; never grant from webhook values.
            conn.execute("UPDATE trainer_backend.support_links SET next_check_at=clock_timestamp(),lease=NULL,lease_until=NULL WHERE member_id=%s AND state='connected'", (member['id'],))

    def once(self):
        if not self.provider:
            return False
        with self.db.transaction() as conn:
            conn.execute("UPDATE trainer_backend.support_link_attempts SET credentials=NULL,status='failed' WHERE expires_at<clock_timestamp() AND status NOT IN ('claimed','failed')")
            row = conn.execute("SELECT player_id FROM trainer_backend.support_links WHERE state='connected' AND next_check_at<=clock_timestamp() AND (lease_until IS NULL OR lease_until<clock_timestamp()) ORDER BY next_check_at LIMIT 1").fetchone()
        if not row:
            return False
        try:
            lease, sealed = self.acquire(row['player_id'])
            self.verify(row['player_id'], lease, sealed)
        except AuthError:
            pass
        return True
