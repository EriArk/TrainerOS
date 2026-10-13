import json
import tempfile
import unittest
import uuid
from pathlib import Path
from directory import Directory, decode


class API:
    def __init__(self):
        self.messages = {}
        self.serial = 0
        self.lose_response = False

    def ensure(self, channel, lobby, content):
        for (c, m), value in self.messages.items():
            if c == channel and decode(value)['lobby'] == lobby:
                self.messages[c, m] = content
                return m
        self.serial += 1
        message = str(self.serial)
        self.messages[channel, message] = content
        if self.lose_response:
            self.lose_response = False
            raise TimeoutError()
        return message

    def edit(self, channel, message, content):
        if (channel, message) not in self.messages:
            return False
        self.messages[channel, message] = content
        return True

    def delete(self, channel, message):
        self.messages.pop((channel, message), None)


class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name) / 'directory.db'
        self.channels = {'choosing': '10', 'racing': '20'}
        self.directory = Directory(self.path, self.channels)
        self.api = API()
        self.command = {'v': 1, 'op': 'upsert', 'lobby': str(uuid.uuid4()), 'revision': 1,
                        'status': 'choosing', 'seats': 0, 'available': 0, 'access': 'public'}

    def tearDown(self):
        self.directory.db.close()
        self.tmp.cleanup()

    def accept(self, author='1', **changes):
        return self.directory.accept(author, 'Host', self.command | changes, 100)

    def test_authority_and_private_field_allowlist(self):
        self.assertEqual(self.accept(owner='2', library=['private'], endpoint='secret'), 'accepted')
        self.assertEqual(self.accept('2', revision=2), 'forbidden')
        self.directory.reconcile(self.api, 100)
        entry = decode(next(iter(self.api.messages.values())))
        self.assertEqual(entry['owner'], '1')
        self.assertNotIn('library', entry)
        self.assertNotIn('endpoint', entry)
        self.assertEqual(self.accept(lobby=str(uuid.uuid4())), 'already-listed')

    def test_close_is_permanent_even_out_of_order(self):
        self.assertEqual(self.accept(op='close', revision=5), 'closed')
        self.assertEqual(self.accept(revision=4), 'closed')
        self.assertEqual(self.accept(revision=6), 'closed')
        self.directory.reconcile(self.api, 100)
        self.assertFalse(self.api.messages)

    def test_private_lobby_can_be_published_again_without_changing_identity(self):
        self.accept()
        self.directory.reconcile(self.api, 100)
        self.assertEqual(self.accept(op='withdraw', revision=2), 'withdrawn')
        self.directory.reconcile(self.api, 101)
        self.assertFalse(self.api.messages)
        self.assertEqual(self.accept(revision=1), 'stale')
        self.assertEqual(self.accept(revision=3), 'accepted')
        self.directory.reconcile(self.api, 102)
        self.assertEqual(decode(next(iter(self.api.messages.values())))['lobby'], self.command['lobby'])

    def test_recovery_after_lost_create_response(self):
        self.assertEqual(self.accept(), 'accepted')
        self.api.lose_response = True
        with self.assertRaises(TimeoutError):
            self.directory.reconcile(self.api, 100)
        self.directory.db.close()
        self.directory = Directory(self.path, self.channels)
        self.directory.reconcile(self.api, 101)
        self.assertEqual(len(self.api.messages), 1)

    def test_move_preserves_lobby_identity_and_cleans_previous_message(self):
        self.accept()
        self.directory.reconcile(self.api, 100)
        self.accept(revision=2, topic='racing', status='gathering', seats=2, available=1,
                    game={'id': 'x', 'title': 'Race', 'platform': 'ps1', 'mode': 'netplay', 'path': 'private'})
        self.directory.reconcile(self.api, 101)
        self.directory.reconcile(self.api, 102)
        self.assertEqual(len(self.api.messages), 1)
        (channel, _), content = next(iter(self.api.messages.items()))
        self.assertEqual(channel, '20')
        self.assertEqual(decode(content)['lobby'], self.command['lobby'])
        self.assertNotIn('path', decode(content)['game'])

    def test_stale_updates_do_not_extend_lease(self):
        self.accept()
        self.directory.reconcile(self.api, 100)
        self.assertEqual(self.directory.accept('1', 'Host', self.command, 270), 'stale')
        self.directory.reconcile(self.api, 281)
        self.assertFalse(self.api.messages)

    def test_validation_and_no_false_game_capabilities(self):
        for values in [{'revision': True}, {'seats': -1}, {'available': 1}, {'access': 'private'},
                       {'status': 'playing'}, {'topic': 'unknown'}, {'lobby': 'bad'}]:
            self.assertEqual(self.accept(**values), 'invalid')


if __name__ == '__main__':
    unittest.main()
