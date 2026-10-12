#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import json
import unittest
from native_lobby import Admission


class NativeLobbyChecks(unittest.TestCase):
    def state(self, slot=1):
        return Admission('a'*32, 'b'*64, 'c'*64, slot, [1, 2, 3, 4])

    def admit(self, state):
        room = dict(state.room)
        del room['password']
        room['protected'] = True
        return state.receive(dict(type='reply_create_room' if state.host else 'reply_join_room',
                                  accept=0, player_name=state.name, room=room))

    def players(self, state, slots):
        prefix = state.name.rsplit('-', 1)[0]+'-'
        return dict(type='reply_players', player_names=[prefix+str(v) if v else '' for v in slots])

    def test_reverse_arrival_uses_native_controller_and_complete_roster(self):
        for slot in (1, 2, 3, 4):
            state = self.state(slot)
            self.admit(state)
            reply = state.receive(self.players(state, [1, 4, 3, 2]))
            self.assertEqual(state.controller, [1, 4, 3, 2].index(slot)+1)
            self.assertEqual(reply['type'] if reply else None, 'request_begin_game' if slot == 1 else None)
            state.receive(dict(type='reply_begin_game', accept=0))
            self.assertTrue(state.running)

    def test_early_start_foreign_member_and_duplicate_are_rejected(self):
        for names in ([1, 2, 0, 0], [1, 2, 5, 4], [1, 2, 2, 4]):
            state = self.state()
            self.admit(state)
            with self.assertRaises(ValueError):
                state.receive(self.players(state, names))
                state.receive(dict(type='reply_begin_game', accept=0))

    def test_room_identity_and_protection_cannot_change(self):
        for key, value in [('MD5', 'd'*64), ('port', 45002), ('room_name', 'other'),
                           ('protected', False), ('features', {'cheat': '1'})]:
            state = self.state()
            room = dict(state.room, protected=True)
            room[key] = value
            with self.assertRaises(ValueError):
                state.receive(dict(type='reply_create_room', accept=0, player_name=state.name, room=room))

    def test_live_roster_loss_or_reorder_ends_session(self):
        for slots in ([1, 0, 3, 4], [1, 3, 2, 4]):
            state = self.state()
            self.admit(state)
            state.receive(self.players(state, [1, 2, 3, 4]))
            state.receive(dict(type='reply_begin_game', accept=0))
            with self.assertRaises(ValueError):
                state.receive(self.players(state, slots))

    def test_public_names_do_not_contain_password(self):
        state = self.state()
        packet = state.first()
        self.assertNotIn('a'*32, packet['player_name'])
        self.assertNotIn('a'*32, packet['room']['room_name'])
        self.assertEqual(packet['room']['password'], 'a'*32)
        self.assertNotIn('password', json.dumps(state.message('request_players')))


if __name__ == '__main__':
    unittest.main()
