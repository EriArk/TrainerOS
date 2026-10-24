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
            entry = decode(value)
            if c == channel and entry.get('listing', entry['lobby']) == lobby:
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

    def test_schedule_and_provider_avatar_projection(self):
        command = dict(self.command, title='Friday racing', startsAt=2000, members=3, avatar='forged')
        avatar = 'a_' + 'a'*32
        self.assertEqual(self.directory.accept('123', 'Host', command, 100, avatar), 'accepted')
        self.directory.reconcile(self.api, 100)
        listing = decode(next(iter(self.api.messages.values())))
        self.assertEqual((listing['title'], listing['startsAt'], listing['members'], listing['avatar']), ('Friday racing', 2000, 3, avatar))
        for value in (-1, True, '2000', 4102444801):
            self.assertEqual(self.directory.accept('123', 'Host', dict(command, revision=2, startsAt=value), 101), 'invalid')

    def test_host_category_routes_without_game_and_moves_same_room(self):
        self.channels.update({'party-games': '30', 'competitive': '40'})
        for revision, topic in enumerate(('party-games', 'competitive'), 1):
            command = dict(self.command, revision=revision, topic=topic)
            self.assertEqual(self.directory.accept('123', 'Host', command, 100), 'accepted')
            self.directory.reconcile(self.api, 100)
            self.directory.reconcile(self.api, 101)  # Finish the durable old-channel deletion.
            self.assertEqual(len(self.api.messages), 1)
            (channel, _), content = next(iter(self.api.messages.items()))
            self.assertEqual(channel, self.channels[topic])
            self.assertEqual(decode(content)['lobby'], command['lobby'])

    def test_live_stream_count_is_bounded_and_projected_without_media_credentials(self):
        command = dict(self.command, streams=2, endpoint='private', token='private')
        self.assertEqual(self.directory.accept('123', 'Host', command, 100), 'accepted')
        self.directory.reconcile(self.api, 100)
        listing = decode(next(iter(self.api.messages.values())))
        self.assertEqual(listing['streams'], 2)
        self.assertNotIn('endpoint', listing); self.assertNotIn('token', listing)
        for count in (-1, 33, True, '1'):
            self.assertEqual(self.directory.accept('123', 'Host', dict(command, revision=2, streams=count), 101), 'invalid')

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
        self.assertEqual(self.accept(lobby=str(uuid.uuid4())), 'accepted')

    def test_multiple_rooms_have_independent_organizer_listings(self):
        room = str(uuid.uuid4())
        first, second = str(uuid.uuid4()), str(uuid.uuid4())
        self.assertEqual(self.accept(lobby=first, room=room), 'accepted')
        self.assertEqual(self.accept('2', lobby=second, room=room), 'accepted')
        self.assertEqual(self.accept('2', lobby=first, revision=2), 'forbidden')
        self.directory.reconcile(self.api, 100)
        rows = [decode(m) for m in self.api.messages.values()]
        self.assertEqual({r['lobby'] for r in rows}, {room})
        self.assertEqual({r['owner'] for r in rows}, {'1', '2'})
        self.assertEqual(self.accept(lobby=first, op='withdraw', revision=2), 'withdrawn')
        self.directory.reconcile(self.api, 101)
        self.assertEqual(len(self.api.messages), 1)
        for _ in range(32):
            self.assertEqual(self.accept('3', lobby=str(uuid.uuid4())), 'accepted')
        self.assertEqual(self.accept('3', lobby=str(uuid.uuid4())), 'too-many-listed')

    def test_deployed_scheduled_listing_metadata_and_authoritative_avatar(self):
        room = str(uuid.uuid4())
        command = self.command | dict(room=room, title='Evening race', startsAt=2000,
                                      members=3, avatar='spoofed', token='private')
        avatar = 'a_' + 'a' * 32
        self.assertEqual(self.directory.accept('1', 'Host', command, 100, avatar), 'accepted')
        self.api.lose_response = True
        with self.assertRaises(TimeoutError):
            self.directory.reconcile(self.api, 100)
        self.directory.reconcile(self.api, 101)
        self.assertEqual(len(self.api.messages), 1)
        entry = decode(next(iter(self.api.messages.values())))
        self.assertEqual(entry['lobby'], room)
        self.assertEqual(entry['listing'], command['lobby'])
        self.assertEqual(entry['listedAt'], 100)
        self.assertEqual(entry['title'], 'Evening race')
        self.assertEqual(entry['startsAt'], 2000)
        self.assertEqual(entry['members'], 3)
        self.assertEqual(entry['avatar'], avatar)
        self.assertNotIn('token', entry)
        self.assertEqual(self.directory.accept('1', 'Host', command | {'revision': 2}, 102, 'invalid'), 'accepted')
        self.directory.reconcile(self.api, 102)
        self.assertNotIn('avatar', decode(next(iter(self.api.messages.values()))))

    def test_scheduled_listing_validation(self):
        for values in ({'room': 'bad'}, {'title': 'x' * 81}, {'startsAt': True},
                       {'startsAt': -1}, {'startsAt': 4102444801},
                       {'members': True}, {'members': -1}, {'members': 33}):
            self.assertEqual(self.accept(**values), 'invalid')

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
