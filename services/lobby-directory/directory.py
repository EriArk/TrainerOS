"""Fluxer announcement projection. This service grants no game/session access."""
import json
import re
import sqlite3
import uuid

PREFIX = 'TrainerOS lobby directory\n```json\n'
TTL = 180


def decode(content):
    if not isinstance(content, str) or len(content) > 2000 or not content.startswith(PREFIX) or not content.endswith('\n```'):
        return None
    try:
        value = json.loads(content[len(PREFIX):-4])
    except (ValueError, TypeError):
        return None
    return value if isinstance(value, dict) and type(value.get('v')) is int and value['v'] == 1 else None


def encode(value):
    text = PREFIX + json.dumps(value, ensure_ascii=False, separators=(',', ':')) + '\n```'
    if len(text.encode('utf-16-le')) // 2 > 2000:
        raise ValueError('Announcement exceeds message limit')
    return text


def text(value, limit):
    if not isinstance(value, str) or not value.strip() or len(value) > limit or any(ord(c) < 32 for c in value):
        raise ValueError('Invalid text')
    return value.strip()


class Directory:
    def __init__(self, path, channels):
        self.channels = channels
        self.db = sqlite3.connect(path)
        self.db.row_factory = sqlite3.Row
        self.db.execute('PRAGMA journal_mode=WAL')
        self.db.execute('''CREATE TABLE IF NOT EXISTS lobbies (
            id TEXT PRIMARY KEY, owner TEXT NOT NULL, revision INTEGER NOT NULL,
            payload TEXT NOT NULL, topic TEXT NOT NULL, expires INTEGER NOT NULL,
            closed INTEGER NOT NULL DEFAULT 0, dirty INTEGER NOT NULL DEFAULT 1,
            channel TEXT, message TEXT)''')
        self.db.execute('''CREATE TABLE IF NOT EXISTS obsolete (
            channel TEXT NOT NULL, message TEXT NOT NULL, PRIMARY KEY(channel,message))''')
        self.db.commit()

    def accept(self, author, name, command, now, avatar=None):
        """author/name come from Fluxer, never the command's claimed owner."""
        if not re.fullmatch(r'[0-9]{1,20}', author):
            return 'invalid'
        try:
            lobby = command['lobby']
            if str(uuid.UUID(lobby)) != lobby:
                return 'invalid'
            revision = command['revision']
            if type(revision) is not int or not 1 <= revision <= 2**53-1:
                return 'invalid'
            op = command['op']
            if op not in ('upsert', 'close', 'withdraw'):
                return 'invalid'
            old = self.db.execute('SELECT * FROM lobbies WHERE id=?', (lobby,)).fetchone()
            if old and old['owner'] != author:
                return 'forbidden'
            if old and old['closed']:
                return 'closed'
            if old and old['revision'] >= revision:
                return 'stale'
            if op in ('close', 'withdraw'):
                # Retain a tombstone even if close arrives before a delayed upsert.
                with self.db:
                    self.db.execute('''INSERT INTO lobbies(id,owner,revision,payload,topic,expires,closed)
                        VALUES(?,?,?,'{}','choosing',?,?) ON CONFLICT(id) DO UPDATE SET
                        revision=excluded.revision,closed=excluded.closed,dirty=1,expires=excluded.expires''',
                        (lobby, author, revision, now, int(op == 'close')))
                return 'closed' if op == 'close' else 'withdrawn'
            active = self.db.execute('SELECT COUNT(*) AS count FROM lobbies WHERE owner=? AND closed=0 AND expires>? AND id<>?',
                                     (author, now, lobby)).fetchone()
            if active['count'] >= 32:
                return 'too-many-listed'
            topic = command.get('topic', 'choosing')
            if topic not in self.channels:
                return 'invalid'
            status = command['status']
            if status not in ('choosing', 'gathering', 'playing'):
                return 'invalid'
            seats, available = command['seats'], command['available']
            if type(seats) is not int or type(available) is not int or not 0 <= available <= seats <= 16:
                return 'invalid'
            access = command['access']
            if access not in ('public', 'approval'):
                return 'invalid'
            streams = command.get('streams', 0)
            if type(streams) is not int or not 0 <= streams <= 32:
                return 'invalid'
            # Explicit allowlist: never reflect ROM paths, IPs, keys, libraries or
            # arbitrary embedded fields into a public community message.
            room = command.get('room', lobby)
            if str(uuid.UUID(room)) != room:
                return 'invalid'
            payload = {'v': 1, 'lobby': room, 'listing': lobby, 'owner': author, 'name': text(name, 100),
                       'revision': revision, 'listedAt': now, 'status': status, 'access': access,
                       'seats': seats, 'available': available, 'streams': streams, 'expires': now + TTL}
            if command.get('title'):
                payload['title'] = text(command['title'], 80)
            starts = command.get('startsAt', 0)
            members = command.get('members', seats - available)
            if type(starts) is not int or not 0 <= starts <= 4102444800 or type(members) is not int or not 0 <= members <= 32:
                return 'invalid'
            payload.update(startsAt=starts, members=members)
            # Avatar is provider-authoritative, never supplied by a host command.
            if isinstance(avatar, str) and re.fullmatch(r'(?:a_)?[0-9a-f]{32}', avatar):
                payload['avatar'] = avatar
            if status != 'choosing':
                game = command['game']
                payload['game'] = {key: text(game[key], limit) for key, limit in
                                   [('id', 128), ('title', 160), ('platform', 32), ('mode', 64)]}
                if not seats:
                    return 'invalid'
            elif seats or available:
                return 'invalid'
            encode(payload)
            with self.db:
                self.db.execute('''INSERT INTO lobbies(id,owner,revision,payload,topic,expires)
                    VALUES(?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET revision=excluded.revision,
                    payload=excluded.payload,topic=excluded.topic,expires=excluded.expires,dirty=1''',
                    (lobby, author, revision, json.dumps(payload, ensure_ascii=False), topic, now + TTL))
            return 'accepted'
        except (ValueError, TypeError, KeyError, AttributeError):
            return 'invalid'

    def reconcile(self, api, now):
        """Durable projection; API failure leaves work pending for the next pass."""
        for old in self.db.execute('SELECT * FROM obsolete').fetchall():
            api.delete(old['channel'], old['message'])
            with self.db:
                self.db.execute('DELETE FROM obsolete WHERE channel=? AND message=?', (old['channel'], old['message']))
        for row in self.db.execute('SELECT * FROM lobbies WHERE dirty=1 OR (expires<=? AND message IS NOT NULL)', (now,)).fetchall():
            if row['closed'] or row['expires'] <= now:
                if row['message']:
                    api.delete(row['channel'], row['message'])
                with self.db:
                    self.db.execute('UPDATE lobbies SET channel=NULL,message=NULL,dirty=0 WHERE id=?', (row['id'],))
                continue
            channel = self.channels[row['topic']]
            payload = json.loads(row['payload'])
            if row['message'] and channel == row['channel']:
                if api.edit(channel, row['message'], encode(payload)):
                    with self.db:
                        self.db.execute('UPDATE lobbies SET dirty=0 WHERE id=?', (row['id'],))
                    continue
            # A stable nonce permits recovery after a successful create whose
            # response was lost. The API adapter additionally finds an existing
            # bot-authored listing with this lobby ID before creating.
            message = api.ensure(channel, row['id'], encode(payload))
            with self.db:
                if row['message'] and (channel != row['channel'] or message != row['message']):
                    self.db.execute('INSERT OR IGNORE INTO obsolete VALUES(?,?)', (row['channel'], row['message']))
                self.db.execute('UPDATE lobbies SET channel=?,message=?,dirty=0 WHERE id=?', (channel, message, row['id']))
