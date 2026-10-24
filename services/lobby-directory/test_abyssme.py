import json
import sqlite3
import tempfile
import unittest
import uuid
from pathlib import Path
from directory import Directory, decode, encode
from test_directory import API


class AbyssME(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name) / 'directory.db'
        self.directory = Directory(self.path, {'choosing': '10', 'other': '20'})
        self.api = API()
        self.command = dict(v=2, product='abyssme', protocol=1, op='upsert',
                            lobby=str(uuid.uuid4()), revision=1, status='choosing',
                            seats=0, available=0, access='public')

    def tearDown(self):
        self.directory.db.close()
        self.temp.cleanup()

    def accept(self, **changes):
        return self.directory.accept('123', 'Host', self.command | changes, 100)

    def test_scoped_payload_and_legacy_wire_isolation(self):
        self.assertEqual(self.accept(owner='spoof', token='secret'), 'accepted')
        self.directory.reconcile(self.api, 100)
        value = decode(next(iter(self.api.messages.values())))
        self.assertEqual((value['v'], value['product'], value['protocol']), (2, 'abyssme', 1))
        self.assertEqual(value['owner'], '123')
        self.assertNotIn('token', value)
        # The existing TrainerOS reader accepts exactly v=1.
        self.assertNotEqual(value['v'], 1)

    def test_game_capability_and_private_field_allowlist(self):
        game = dict(id='a' * 64, title='Test game', platform='j2me', mode='jsr82-l2cap', adapter='1', path='secret')
        self.assertEqual(self.accept(status='gathering', seats=2, available=1, game=game), 'accepted')
        self.directory.reconcile(self.api, 100)
        value = decode(next(iter(self.api.messages.values())))
        self.assertEqual(value['game']['adapter'], '1')
        self.assertNotIn('path', value['game'])
        for changes in ({'id': 'title-only'}, {'platform': 'ps1'}, {'adapter': ''}, {'mode': 'invalid mode'}):
            self.assertEqual(self.accept(revision=2, status='gathering', seats=2, available=1, game=game | changes), 'invalid')

    def test_rejects_unknown_versions_scopes_private_and_streams(self):
        for change in ({'v': 3}, {'v': True}, {'product': 'traineros'}, {'protocol': 2}, {'protocol': True},
                       {'access': 'private'}, {'streams': 1}, {'v': 1}):
            self.assertEqual(self.accept(**change), 'invalid')
        self.assertIsNone(decode(encode(self.command | {'v': 3})))

    def test_one_account_one_room_across_products(self):
        self.assertEqual(self.accept(), 'accepted')
        legacy = {k: v for k, v in self.command.items() if k not in ('product', 'protocol')}
        legacy.update(v=1, lobby=str(uuid.uuid4()))
        self.assertEqual(self.directory.accept('123', 'Host', legacy, 101), 'already-listed')
        self.assertEqual(self.directory.accept('456', 'Other', legacy, 101), 'accepted')

    def test_product_identity_survives_withdraw_close_and_restart(self):
        self.assertEqual(self.accept(), 'accepted')
        self.assertEqual(self.accept(op='withdraw', revision=2), 'withdrawn')
        self.directory.db.close()
        self.directory = Directory(self.path, {'choosing': '10'})
        legacy = {k: v for k, v in self.command.items() if k not in ('product', 'protocol')}
        legacy.update(v=1, revision=3)
        for op in ('upsert', 'close', 'withdraw'):
            self.assertEqual(self.directory.accept('123', 'Host', legacy | {'op': op}, 102), 'wrong-product')
        self.assertEqual(self.accept(revision=3), 'accepted')
        self.assertEqual(self.accept(op='close', revision=4), 'closed')
        self.assertEqual(self.accept(revision=5), 'closed')

    def test_out_of_order_close_is_scoped(self):
        self.assertEqual(self.accept(op='close', revision=4), 'closed')
        self.assertEqual(self.accept(revision=3), 'closed')

    def test_migrates_legacy_rows_without_losing_ownership(self):
        self.directory.db.close()
        oldpath = Path(self.temp.name) / 'old.db'
        db = sqlite3.connect(oldpath)
        db.execute('CREATE TABLE lobbies (id TEXT PRIMARY KEY, owner TEXT NOT NULL, revision INTEGER NOT NULL, payload TEXT NOT NULL, topic TEXT NOT NULL, expires INTEGER NOT NULL, closed INTEGER NOT NULL DEFAULT 0, dirty INTEGER NOT NULL DEFAULT 1, channel TEXT, message TEXT)')
        db.execute("INSERT INTO lobbies(id,owner,revision,payload,topic,expires) VALUES(?, '123', 1, '{}', 'choosing', 280)", (self.command['lobby'],))
        db.commit(); db.close()
        self.directory = Directory(oldpath, {'choosing': '10'})
        self.assertEqual(self.accept(revision=2), 'wrong-product')
        self.assertEqual(self.directory.db.execute('SELECT product FROM lobbies').fetchone()['product'], 'traineros')


if __name__ == '__main__':
    unittest.main()
