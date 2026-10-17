"""TrainerOS directory bot: authenticated Fluxer DMs -> public announcements."""
import argparse
import datetime
import hashlib
import json
import logging
from pathlib import Path
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

import websocket
from directory import Directory, decode

LOG = logging.getLogger('traineros-lobby')


class Retry(Exception):
    def __init__(self, seconds=5):
        self.seconds = max(1, min(300, seconds))


class API:
    def __init__(self, credentials):
        self.base = credentials['api_base'].rstrip('/')
        if self.base != 'https://api.fluxer.app':
            raise ValueError('Unreviewed Fluxer instance')
        self.token = credentials['bot_token']
        self.bot = credentials['application_id']
        self.guild = credentials['guild_id']
        self.blocked = 0

    def call(self, method, path, body=None, missing=False, denied=False):
        if time.monotonic() < self.blocked:
            raise Retry(self.blocked-time.monotonic())
        req = urllib.request.Request(self.base + path, method=method,
            data=json.dumps(body, ensure_ascii=False).encode() if body is not None else None,
            headers={'Authorization': 'Bot ' + self.token, 'Content-Type': 'application/json',
                     'User-Agent': 'TrainerOS Lobby Directory/1.0'})
        try:
            with urllib.request.urlopen(req, timeout=20) as response:
                raw = response.read(2*1024*1024+1)
                if len(raw) > 2*1024*1024:
                    raise Retry(30)
                return json.loads(raw) if raw else {}
        except urllib.error.HTTPError as error:
            if (error.code == 404 and missing) or (error.code == 403 and denied):
                return None
            if error.code == 429:
                try:
                    delay = float(json.loads(error.read(4096)).get('retry_after', 5))
                except (ValueError, TypeError):
                    delay = 5
                self.blocked = time.monotonic() + max(1, min(300, delay))
                raise Retry(delay) from None
            # Never log bodies, headers, tokens, or authentication-bearing URLs.
            LOG.warning('Fluxer operation failed: HTTP %d', error.code)
            raise Retry(30) from None
        except (urllib.error.URLError, TimeoutError, ValueError):
            raise Retry(5) from None

    @staticmethod
    def body(content):
        return {'content': content, 'allowed_mentions': {'parse': [], 'users': [], 'roles': []}, 'flags': 4096}

    def edit(self, channel, message, content):
        return self.call('PATCH', f'/v1/channels/{channel}/messages/{message}', self.body(content), missing=True) is not None

    def delete(self, channel, message):
        self.call('DELETE', f'/v1/channels/{channel}/messages/{message}', missing=True)

    def ensure(self, channel, lobby, content):
        before = ''
        for _ in range(50):
            page = self.call('GET', f'/v1/channels/{channel}/messages?limit=100' + ('&before='+before if before else ''))
            for message in page:
                value = decode(message.get('content'))
                if message.get('author', {}).get('id') == self.bot and value and value.get('listing', value.get('lobby')) == lobby:
                    if self.edit(channel, message['id'], content):
                        return message['id']
            if len(page) < 100:
                nonce = str(int.from_bytes(hashlib.sha256((channel+lobby).encode()).digest()[:8], 'big') >> 1)
                return self.call('POST', f'/v1/channels/{channel}/messages',
                                 self.body(content) | {'nonce': nonce, 'enforce_nonce': True})['id']
            before = page[-1]['id']
        # Do not create a duplicate when recovery history could not be exhausted.
        raise Retry(60)

    def authorized(self, author, channel):
        private = self.call('GET', f'/v1/channels/{channel}', missing=True, denied=True)
        if not private or private.get('type') != 1:
            return False
        recipients = {person['id'] for person in private.get('recipients', [])}
        if author not in recipients or recipients - {author, self.bot}:
            return False
        return self.call('GET', f'/v1/guilds/{self.guild}/members/{author}', missing=True, denied=True) is not None


def run(config, state_path):
    credentials = json.loads((config/'credentials.json').read_text())
    directory_config = json.loads((config/'directory.json').read_text())
    api = API(credentials)
    assert api.call('GET', '/v1/users/@me')['id'] == api.bot
    discovery = api.call('GET', '/.well-known/fluxer')
    gateway = discovery['endpoints']['gateway']
    if gateway != 'wss://gateway.fluxer.app':
        raise ValueError('Unreviewed Gateway endpoint')
    directory = Directory(state_path, directory_config['channels'])
    session = None
    sequence = None
    while True:
        stop = threading.Event()
        socket = None
        heartbeat = None
        try:
            socket = websocket.create_connection(gateway+'/?v=1&encoding=json', timeout=20, enable_multithread=True)
            hello = json.loads(socket.recv())
            if hello.get('op') != 10:
                raise Retry()
            interval = hello['d']['heartbeat_interval']/1000
            if not 1 <= interval <= 120:
                raise Retry()
            acknowledged = time.monotonic()
            def beat():
                while not stop.wait(interval*0.8):
                    try:
                        if time.monotonic()-acknowledged > interval*2:
                            stop.set()
                            socket.close()
                            return
                        socket.send(json.dumps({'op': 1, 'd': sequence}))
                    except Exception:
                        stop.set()
            heartbeat = threading.Thread(target=beat, daemon=True)
            heartbeat.start()
            identify = {'token': api.token, 'properties': {'os': 'Linux', 'browser': 'TrainerOS Lobby Directory', 'device': 'server'}}
            if session and sequence is not None:
                socket.send(json.dumps({'op': 6, 'd': {'token': api.token, 'session_id': session, 'seq': sequence}}))
            else:
                socket.send(json.dumps({'op': 2, 'd': identify}))
            socket.settimeout(1)
            reconcile_at = 0
            while not stop.is_set():
                try:
                    raw = socket.recv()
                    if not raw:
                        break
                    if len(raw) > 4*1024*1024:
                        raise Retry()
                    event = json.loads(raw)
                    op = event.get('op')
                    if op == 1:
                        socket.send(json.dumps({'op': 1, 'd': sequence}))
                    elif op == 11:
                        acknowledged = time.monotonic()
                    elif op == 7:
                        break
                    elif op == 9:
                        session = sequence = None
                        break
                    elif op == 0:
                        data = event.get('d', {})
                        if event.get('t') == 'READY':
                            session = data['session_id']
                            LOG.info('Directory Gateway ready')
                        elif event.get('t') == 'MESSAGE_CREATE' and not data.get('guild_id'):
                            author = data.get('author', {})
                            command = decode(data.get('content'))
                            if command and not author.get('bot'):
                                try:
                                    stamp = datetime.datetime.fromisoformat(data['timestamp'].replace('Z', '+00:00')).timestamp()
                                except (ValueError, KeyError):
                                    stamp = 0
                                now = int(time.time())
                                if abs(now-stamp) <= 120 and api.authorized(author['id'], data['channel_id']):
                                    result = directory.accept(author['id'], author.get('global_name') or author.get('username'), command, now, author.get('avatar'))
                                    LOG.info('Directory update: %s', result)
                        # Persist command state before acknowledging this Dispatch.
                        sequence = event.get('s', sequence)
                except websocket.WebSocketTimeoutException:
                    pass
                if time.monotonic() >= reconcile_at:
                    try:
                        directory.reconcile(api, int(time.time()))
                        reconcile_at = time.monotonic()+5
                    except Retry as retry:
                        reconcile_at = time.monotonic()+retry.seconds
        except Retry as retry:
            LOG.info('Directory reconnect deferred')
            time.sleep(min(retry.seconds, 30))
        except (websocket.WebSocketException, OSError, ValueError, KeyError):
            LOG.warning('Directory connection interrupted')
            time.sleep(5)
        finally:
            stop.set()
            if heartbeat:
                heartbeat.join(timeout=2)
            if socket:
                socket.close()
        time.sleep(5)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('--state', type=Path, required=True)
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s')
    run(args.config, args.state)
