# SPDX-License-Identifier: GPL-3.0-or-later
"""Admit an exact private roster using RMG's existing version-17 lobby.

Emulation, input and temporary progress synchronization remain in the pinned
native server/core. This module runs inside the party's isolated network.
"""
import hashlib
import json
import re
import time


class Admission:
    def __init__(self, token, content, runtime, slot, seats):
        if (not isinstance(token, str) or not re.fullmatch('[a-f0-9]{32}', token)
                or any(not isinstance(v, str) or not re.fullmatch('[a-f0-9]{64}', v)
                       for v in (content, runtime))
                or type(slot) is not int or not isinstance(seats, list)
                or not 2 <= len(seats) <= 4 or any(type(v) is not int or not 1 <= v <= 4 for v in seats)
                or len(set(seats)) != len(seats) or slot not in seats or 1 not in seats):
            raise ValueError('private roster')
        room_id = hashlib.sha256(token.encode()).hexdigest()[:24]
        self.name = room_id+'-'+str(slot)
        self.names = {room_id+'-'+str(v) for v in seats}
        self.host = slot == 1
        self.runtime = runtime
        self.room = dict(port=45001, password=token, MD5=content,
                         room_name='TrainerOS-'+room_id, game_name='Nintendo 64', features={})
        self.admitted = self.begun = self.running = False
        self.roster = []
        self.controller = 0

    def message(self, kind, room=None):
        tick = str(int(time.time()*1000))
        return dict(type=kind, room=room if room is not None else {'port': 45001},
                    player_name=self.name, client_sha=self.runtime, emulator='RMG',
                    netplay_version=17, authTime=tick,
                    auth=hashlib.sha256((tick+'RMG').encode()).hexdigest())

    def first(self):
        return self.message('request_create_room' if self.host else 'request_join_room', self.room)

    def receive(self, packet):
        if not isinstance(packet, dict):
            raise ValueError('native lobby packet')
        kind = packet.get('type')
        if kind in ('reply_create_room', 'reply_join_room'):
            room = packet.get('room')
            if (self.admitted or kind != ('reply_create_room' if self.host else 'reply_join_room')
                    or type(packet.get('accept')) is not int or packet['accept'] != 0
                    or packet.get('player_name') != self.name or not isinstance(room, dict)
                    or room.get('port') != 45001 or room.get('MD5') != self.room['MD5']
                    or room.get('room_name') != self.room['room_name'] or room.get('protected') is not True
                    or room.get('features') != {}):
                raise ValueError('native room admission')
            self.admitted = True
            return self.message('request_players')
        if kind == 'reply_players':
            names = packet.get('player_names')
            if (not self.admitted or not isinstance(names, list) or len(names) != 4
                    or any(not isinstance(v, str) for v in names)):
                raise ValueError('native player list')
            roster = [name for name in names if name]
            if (len(set(roster)) != len(roster) or not set(roster) <= self.names
                    or self.name not in roster or (self.running and roster != self.roster)):
                raise ValueError('changed native roster')
            self.roster = roster
            self.controller = names.index(self.name)+1
            if self.host and not self.begun and set(roster) == self.names:
                self.begun = True
                return self.message('request_begin_game')
        elif kind == 'reply_begin_game':
            if (not self.admitted or self.running or type(packet.get('accept')) is not int
                    or packet['accept'] != 0 or set(self.roster) != self.names
                    or not 1 <= self.controller <= 4):
                raise ValueError('native start before complete roster')
            self.running = True
        elif kind == 'reply_edit_room':
            raise ValueError('changed native settings')
        return None


def join(config, cancelled=lambda: False, room_ready=lambda: None):
    import websocket
    state = Admission(config['token'], config['content'], config['runtime'], config['slot'], config['seats'])
    deadline = time.monotonic()+60
    ws = None
    # The coordinator starts guests only after the host has created its room;
    # refusals cannot be silently retried as a different admission.
    while time.monotonic() < deadline and not cancelled():
        try:
            ws = websocket.create_connection('ws://10.203.0.1:45000', timeout=1,
                                             http_no_proxy=['10.203.0.1'])
            break
        except (OSError, websocket.WebSocketException):
            time.sleep(.1)
    if ws is None:
        raise TimeoutError('native lobby listener')
    try:
        ws.send(json.dumps(state.first(), separators=(',', ':')))
        while time.monotonic() < deadline and not cancelled():
            try:
                data = ws.recv()
            except websocket.WebSocketTimeoutException:
                continue
            if not isinstance(data, str) or len(data) > 16384:
                raise ValueError('native lobby frame')
            admitted = state.admitted
            reply = state.receive(json.loads(data))
            if reply:
                ws.send(json.dumps(reply, separators=(',', ':')))
            if state.host and state.admitted and not admitted:
                room_ready()
            if state.running:
                return ws, state
        raise TimeoutError('native lobby admission')
    except BaseException:
        ws.close(timeout=1)
        raise
