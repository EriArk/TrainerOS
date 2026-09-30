import importlib.util
import json
from pathlib import Path
import queue
import socket
import types
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('bluetooth', Path(__file__).resolve().parents[1]/'packaging/integrations/nearby-bluetooth.py')
b = importlib.util.module_from_spec(spec); spec.loader.exec_module(b)
A = '11111111-1111-4111-8111-111111111111'
B = '22222222-2222-4222-8222-222222222222'


class BluetoothTests(unittest.TestCase):
    def setUp(self):
        self.sessions = []; self.sockets = []

    def tearDown(self):
        for session in self.sessions: session.stop()
        for session in self.sessions:
            if session.thread.is_alive(): session.thread.join(2)
            self.assertFalse(session.thread.is_alive(), 'Worker did not stop')
        for endpoint in self.sockets: endpoint.close()

    def session(self, incoming=True):
        events = queue.Queue()
        radio = types.SimpleNamespace(key=A, name='Misty', target=B,
            g=types.SimpleNamespace(idle_add=lambda fn, *args: fn(*args)),
            session_event=lambda _session, event, fields: events.put((event, fields)))
        local, remote = socket.socketpair(); self.sockets.append(remote)
        remote.settimeout(2)
        session = b.Session(radio, incoming, '/device', local.detach())
        self.sessions.append(session)
        return session, remote, events

    def listener(self):
        listener = socket.socket(); listener.bind(('127.0.0.1', 0)); listener.listen(); listener.settimeout(2)
        self.sockets.append(listener); return listener

    def hello(self, remote):
        remote.sendall(json.dumps({'btLink': 1, 'id': B, 'name': 'Brock'}).encode()+b'\n')
        data = b''
        while not data.endswith(b'\n'): data += remote.recv(1)
        self.assertEqual(json.loads(data)['id'], A)

    def test_discovery_filters_service_and_validates_names_and_addresses(self):
        props = {'UUIDs': [b.PROFILE], 'Address': '00:11:22:33:44:55', 'Name': 'Brock'}
        self.assertEqual(b.discovered(props)['name'], 'Brock')
        self.assertEqual(b.discovered(props)['id'], b.discovered(props)['id'])
        for key, value in [('UUIDs', []), ('Address', '../bad'), ('Name', 'bad\nname'), ('Name', 'x'*33)]:
            self.assertIsNone(b.discovered({**props, key: value}))
        for invalid in (None, 123, [], {}):
            with self.assertRaises(ValueError): b.identity({'id': invalid, 'name': 'Brock'})

    def test_partial_handshake_and_following_payload_are_separated(self):
        session, remote, _ = self.session()
        remote.sendall(b'{"btLink":'); remote.sendall(b'1}\nopaque\x00bytes')
        self.assertEqual(session.line(1), {'btLink': 1})
        self.assertEqual(session.buffer, b'opaque\x00bytes')

    def test_oversized_or_non_object_control_is_rejected(self):
        for packet in [b'x'*1025, b'[]\n', b'x'*1025+b'\n', b'{\xff}\n']:
            session, remote, _ = self.session(); remote.sendall(packet)
            with self.assertRaises(ValueError): session.line(1)

    def test_no_game_channel_exists_until_incoming_consent(self):
        listener = self.listener(); session, remote, events = self.session()
        with patch.object(b, 'LINK_PORT', listener.getsockname()[1]), patch.object(b, 'diagnostic'):
            session.thread.start(); self.hello(remote)
            event, fields = events.get(timeout=2)
            self.assertEqual(event, 'invite'); self.assertEqual(fields['peer'], B)
            listener.settimeout(.1)
            with self.assertRaises(socket.timeout): listener.accept()
            session.accepted = True; session.decision.set()
            listener.settimeout(2); local, _ = listener.accept(); self.sockets.append(local)
            self.assertEqual(json.loads(remote.recv(1024)), {'accepted': True})
            self.assertEqual(events.get(timeout=2)[0], 'ready')
            local.sendall(b'{"version":2,"type":"hello"}\n')
            self.assertEqual(remote.recv(1024), b'{"version":2,"type":"hello"}\n')
            session.stop(); session.thread.join(2)

    def test_decline_never_opens_local_link_or_sends_game_payload(self):
        session, remote, events = self.session(); session.decision.set()
        with patch.object(b.socket, 'create_connection') as connect, patch.object(b, 'diagnostic'):
            session.thread.start(); self.hello(remote)
            self.assertEqual(events.get(timeout=2)[0], 'invite')
            self.assertEqual(json.loads(remote.recv(1024)), {'accepted': False})
            session.thread.join(2); connect.assert_not_called()
            self.assertEqual(events.get(timeout=2)[0], 'closed')

    def test_outgoing_waits_for_accept_then_bridges_duplex_in_order(self):
        session, remote, events = self.session(False)
        with patch.object(b, 'diagnostic'):
            session.thread.start(); self.hello(remote)
            with self.assertRaises(queue.Empty): events.get(timeout=.1)
            remote.sendall(b'{"accepted":true}\n')
            event, fields = events.get(timeout=2)
            self.assertEqual(event, 'ready'); self.assertEqual(fields['identity'], B)
            local = socket.create_connection(('127.0.0.1', fields['port']), timeout=2); self.sockets.append(local)
            def receive(endpoint, count):
                data = b''
                while len(data) < count: data += endpoint.recv(count-len(data))
                return data
            forward, backward = b'\x00framed-game-data\n'*3500, b'other-direction\n'*3500
            local.sendall(forward); self.assertEqual(receive(remote, len(forward)), forward)
            remote.sendall(backward); self.assertEqual(receive(local, len(backward)), backward)
            session.stop(); session.thread.join(2)

    def test_self_identity_is_rejected_before_invitation(self):
        session, remote, events = self.session(); session.thread.start()
        remote.sendall(json.dumps({'btLink': 1, 'id': A, 'name': 'Imposter'}).encode()+b'\n')
        self.assertEqual(events.get(timeout=2)[0], 'closed')

    def test_cancellation_wakes_waiting_worker(self):
        session, remote, events = self.session(); session.thread.start(); self.hello(remote)
        self.assertEqual(events.get(timeout=2)[0], 'invite')
        session.stop(); session.thread.join(2); self.assertFalse(session.thread.is_alive())

    def test_stale_callback_does_not_release_new_session(self):
        radio = object.__new__(b.NearbyBluetooth); radio.session = object()
        with patch.object(b, 'emit') as emit:
            radio.session_event(object(), 'closed', {'error': 'old'}); emit.assert_not_called()


if __name__ == '__main__': unittest.main()
