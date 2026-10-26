#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real ICE/DTLS/socket boundary checks; no codec, game or uinput needed."""
import json
import os
from pathlib import Path
import queue
import re
import socket
import struct
import subprocess
import sys
import threading
import time
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())


class Pair:
    def __init__(self, different_identity=False, bad_fingerprint=False, ice=None, modes=None):
        self.events = queue.Queue()
        self.ready, self.paths, self.output = {}, [], []
        self.processes = [subprocess.Popen([BINARY], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                          for _ in range(2)]
        self.identity = ['ab'*32, ('cd' if different_identity else 'ab')*32]
        self.bad_fingerprint = bad_fingerprint
        for i, p in enumerate(self.processes):
            threading.Thread(target=self.read, args=(i, p), daemon=True).start()
            config = {'host': i == 0, 'identity': self.identity[i], 'seconds': 30, 'ice': ice or []}
            if modes and modes[i]:
                config['mode'] = modes[i]
            self.send(i, config)

    def send(self, i, value):
        self.processes[i].stdin.write(json.dumps(value)+'\n')
        self.processes[i].stdin.flush()

    def read(self, i, p):
        for line in p.stdout:
            self.events.put((i, json.loads(line)))
        self.events.put((i, {'event': 'ended'}))

    def connect(self, timeout=8):
        end = time.monotonic()+timeout
        while len(self.ready) < 2 and time.monotonic() < end:
            try:
                i, value = self.events.get(timeout=max(.01, end-time.monotonic()))
            except queue.Empty:
                return False
            self.output.append(value)
            if value['event'] == 'ended':
                return False
            if value['event'] == 'description':
                description = value['description']
                if self.bad_fingerprint and i == 0:
                    description = re.sub(r'(a=fingerprint:sha-256 )[A-Fa-f0-9:]+',
                                         lambda m: m[1]+':'.join(['00']*32), description)
                # The trusted parent binds the remote description to the local
                # session. Different encrypted identity still must fail inside.
                self.send(1-i, {'identity': self.identity[1-i], 'description': description})
            elif value['event'] == 'ready':
                self.ready[i] = Path(value['config'])
                self.paths.append(self.ready[i])
            elif value['event'] == 'candidates':
                self.send(1-i, {'identity': self.identity[1-i], 'candidates': value['candidates']})
        return len(self.ready) == 2

    def close(self):
        for i, p in enumerate(self.processes):
            if p.poll() is None:
                try:
                    self.send(i, {'close': True})
                except (BrokenPipeError, OSError):
                    pass
        for p in self.processes:
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
                raise AssertionError('transport did not close promptly')
            try:
                p.stdin.close()
            except BrokenPipeError:
                pass
            p.stdout.close();p.stderr.close()


class TransportTests(unittest.TestCase):
    def setUp(self):
        self.pair = None

    def tearDown(self):
        if self.pair:
            self.pair.close()
            for path in self.pair.paths:
                self.assertFalse(path.exists(), 'private material survived transport exit')

    def test_cabinet_native_packets_private_bounded_and_bidirectional(self):
        self.pair=Pair(modes=['cabinet','cabinet'])
        self.assertTrue(self.pair.connect())
        configs=[dict(line.split('=',1) for line in self.pair.ready[i].read_text().splitlines()) for i in (0,1)]
        clients=[]
        def exact(sock,n):
            data=b''
            while len(data)<n:
                part=sock.recv(n-len(data));self.assertTrue(part);data+=part
            return data
        try:
            for i in (0,1):
                self.assertEqual(set(configs[i]),{'mode','host','socket'})
                self.assertEqual(configs[i]['host'],'true' if i==0 else 'false')
                self.assertEqual(self.pair.ready[i].stat().st_mode & 0o777,0o600)
                self.assertEqual(Path(configs[i]['socket']).stat().st_mode & 0o777,0o600)
                sock=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);sock.settimeout(3)
                sock.connect(configs[i]['socket']);clients.append(sock)
            time.sleep(.05)
            for sender in (0,1):
                for size in (1,10,256,4096,0x4004):
                    packet=struct.pack('!H',size)+os.urandom(size)
                    clients[sender].sendall(packet[:1]);clients[sender].sendall(packet[1:])
                    self.assertEqual(exact(clients[1-sender],len(packet)),packet)
                # A valid coalesced stream crosses the old 64 KiB queue boundary
                # and ends reads in the middle of the next native packet.
                burst=b''.join(struct.pack('!H',0x4004)+bytes([n])*0x4004 for n in range(32))
                clients[sender].sendall(burst)
                self.assertEqual(exact(clients[1-sender],len(burst)),burst)
            with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as duplicate:
                duplicate.settimeout(2);duplicate.connect(configs[0]['socket'])
                self.assertEqual(duplicate.recv(1),b'')
            clients[0].sendall(struct.pack('!H',0x4005))
            self.assertNotEqual(self.pair.processes[0].wait(timeout=5),0)
        finally:
            for sock in clients:sock.close()

    def test_cabinet_cannot_join_other_modes(self):
        for other in ('bba','ps2','psp','game',None):
            with self.subTest(other=other):
                pair=Pair(modes=['cabinet',other])
                try:
                    self.assertFalse(pair.connect());self.assertFalse(pair.ready)
                finally:pair.close()

    def test_bba_native_frames_are_private_bounded_and_bidirectional(self):
        self.pair = Pair(modes=['bba', 'bba'])
        self.assertTrue(self.pair.connect())
        configs = [dict(line.split('=', 1) for line in self.pair.ready[i].read_text().splitlines()) for i in range(2)]
        clients = [socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) for _ in range(2)]
        try:
            for i, client in enumerate(clients):
                self.assertEqual(configs[i]['mode'], 'bba')
                self.assertEqual(configs[i]['host'], 'true' if i == 0 else 'false')
                self.assertRegex(configs[i]['mac'], r'^00:09:bf:[0-9a-f]{2}:[0-9a-f]{2}:0[12]$')
                self.assertEqual(self.pair.ready[i].stat().st_mode & 0o777, 0o600)
                self.assertEqual(Path(configs[i]['socket']).stat().st_mode & 0o777, 0o600)
                client.connect(configs[i]['socket']);client.settimeout(2)
            self.assertNotEqual(configs[0]['mac'], configs[1]['mac'])
            def exact(client, n):
                data = b''
                while len(data) < n:
                    part = client.recv(n-len(data))
                    self.assertTrue(part)
                    data += part
                return data
            for sender in (0, 1):
                for size in (14, 60, 1518):
                    frame = b'\xff'*6 + bytes.fromhex(configs[sender]['mac'].replace(':', '')) + b'\x08\x06' + os.urandom(size-14)
                    wire = struct.pack('<H', len(frame)) + frame
                    clients[sender].sendall(wire[:1]);clients[sender].sendall(wire[1:9]);clients[sender].sendall(wire[9:])
                    self.assertEqual(exact(clients[1-sender], len(wire)), wire)
            # A second local client cannot replace the admitted Dolphin socket.
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as duplicate:
                duplicate.settimeout(2);duplicate.connect(configs[0]['socket'])
                self.assertEqual(duplicate.recv(1), b'')
            clients[0].sendall(struct.pack('<H', 1519))
            self.assertNotEqual(self.pair.processes[0].wait(timeout=5), 0)
        finally:
            for client in clients: client.close()

    def test_bba_cannot_join_ps2_transport(self):
        self.pair = Pair(modes=['bba', 'ps2'])
        self.assertFalse(self.pair.connect())
        self.assertFalse(self.pair.ready)

    def test_ps2_native_envelopes_and_private_material(self):
        self.pair = Pair(modes=['ps2', 'ps2'])
        self.assertTrue(self.pair.connect())
        configs = [dict(line.split('=', 1) for line in self.pair.ready[i].read_text().splitlines()) for i in range(2)]
        for i, config in enumerate(configs):
            self.assertEqual(config['mode'], 'ps2')
            self.assertEqual(config['address'], '127.0.0.1')
            self.assertEqual(config['host'], 'true' if i == 0 else 'false')
            self.assertEqual(config['peer'], str(i+1))
            self.assertRegex(config['room'], r'^[a-f0-9]{12}$')
            self.assertEqual(self.pair.ready[i].stat().st_mode & 0o777, 0o600)
            self.assertEqual(self.pair.ready[i].parent.stat().st_mode & 0o777, 0o700)
        self.assertEqual(configs[0]['room'], configs[1]['room'])
        self.assertNotIn(configs[0]['room'], json.dumps(self.pair.output))
        def envelope(kind, peer, payload=b'', index=0, count=1):
            return struct.pack('!4sBBHIIIHHHHQQ', b'ECLX', 1, kind, 44, peer, 1, 1,
                               index, count, len(payload), 0, 1, 1)+payload
        def exact(s, n):
            result=b''
            while len(result)<n:
                data=s.recv(n-len(result));self.assertTrue(data);result+=data
            return result
        def send(s, data):s.sendall(struct.pack('!H',len(data))+data)
        def receive(s):return exact(s,struct.unpack('!H',exact(s,2))[0])
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as host, socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as guest:
            host.settimeout(3);guest.settimeout(3)
            host.connect(configs[0]['socket']);guest.connect(configs[1]['socket'])
            time.sleep(.05)
            hello = envelope(1, 2)
            send(guest,hello)
            self.assertEqual(receive(host), hello)
            ack = envelope(2, 1);send(host,ack)
            self.assertEqual(receive(guest), ack)
            for packet in (envelope(3, 2, b'a'*1000, 0, 2), envelope(3, 2, b'b'*514, 1, 2)):
                send(guest,packet)
                self.assertEqual(receive(host), packet)
                send(host,packet)
                self.assertEqual(receive(guest), packet)

    def test_ps2_cannot_be_admitted_as_psp_or_media(self):
        for modes in (['ps2', 'psp'], ['psp', 'ps2'], ['ps2', None], [None, 'ps2']):
            with self.subTest(modes=modes):
                pair = Pair(modes=modes)
                try:
                    self.assertFalse(pair.connect())
                    self.assertFalse(pair.ready)
                finally:
                    pair.close()

    def test_psp_multiple_streams_native_close_and_reconnect(self):
        self.pair = Pair(modes=['psp', 'psp'])
        self.assertTrue(self.pair.connect())
        listeners, peers, clients = [], [], []
        def exact(s, n):
            result = b''
            while len(result) < n:
                part = s.recv(n-len(result))
                self.assertTrue(part, 'unexpected TCP EOF')
                result += part
            return result
        try:
            for port in (27312, 27313):
                server = socket.socket();listeners.append(server)
                server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                server.bind(('127.0.0.1', port));server.listen();server.settimeout(3)
            # A coordination stream stays alive while gameplay sockets come
            # and go. Reusing a native port must not reuse an old stream ID.
            for i in range(6):
                client = socket.create_connection(('127.0.0.2', 27312 if i == 0 else 27313), timeout=3)
                clients.append(client)
                peer, address = listeners[0 if i == 0 else 1].accept();peers.append(peer);peer.settimeout(3)
                self.assertEqual(address[0], '127.0.0.2')
                payload = bytes([i])*262144
                client.sendall(payload);self.assertEqual(exact(peer, len(payload)), payload)
                peer.sendall(payload[::-1]);self.assertEqual(exact(client, len(payload)), payload[::-1])
                if i:
                    # Last bytes before EOF must arrive before native close.
                    peer.sendall(b'final');peer.close()
                    self.assertEqual(exact(client, 5), b'final')
                    self.assertEqual(client.recv(1), b'');client.close()
                peers[0].sendall(b'live');self.assertEqual(exact(clients[0], 4), b'live')
        finally:
            for s in clients+peers+listeners:s.close()

    def test_psp_cannot_negotiate_as_remote_play(self):
        self.pair = Pair(modes=['psp', None])
        self.assertFalse(self.pair.connect())
        self.assertFalse(self.pair.ready)

    def test_unresponsive_stun_does_not_remove_direct_local_candidates(self):
        # An actual bound but silent UDP endpoint, independent of public DNS or
        # internet availability. Local play must survive address-discovery loss.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as blackhole:
            blackhole.bind(('127.0.0.1', 0))
            self.pair = Pair(ice=[f'stun:127.0.0.1:{blackhole.getsockname()[1]}'])
            self.assertTrue(self.pair.connect(timeout=25))
            batches = [c['candidate'] for v in self.pair.output if v['event'] == 'candidates' for c in v['candidates']]
            self.assertTrue(any(' typ host' in s for s in batches))
            self.assertTrue(all(' typ relay' not in s for s in batches))

    def test_existing_udp_and_rtsp_with_private_fresh_material(self):
        self.pair = Pair()
        self.assertTrue(self.pair.connect())
        fields = []
        for i in (0,1):
            path=self.pair.ready[i]
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            self.assertEqual(path.parent.stat().st_mode & 0o777, 0o700)
            self.assertEqual(path.stat().st_uid, os.getuid())
            fields.append(dict(line.split('=', 1) for line in path.read_text().splitlines()))
        for name in ('key', 'ping', 'connect'):
            self.assertEqual(fields[0][name], fields[1][name])
            self.assertNotIn(fields[0][name], json.dumps(self.pair.output))
        for port in (49011, 49012, 49013):
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
                server.bind((fields[0]['address'], port));server.settimeout(3)
                client.bind(('127.0.0.1', 0));client.settimeout(3)
                request = os.urandom(1392)
                client.sendto(request, (fields[1]['address'], port))
                data, sender = server.recvfrom(8192)
                self.assertEqual(sender[0], fields[0]['peer'])
                self.assertEqual(data, request)
                server.sendto(data, sender)
                self.assertEqual(client.recvfrom(8192)[0], request)
                # An unrelated local socket cannot replace the selected client.
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as other:
                    other.bind(('127.0.0.1', 0));other.sendto(b'wrong peer', (fields[1]['address'], port))
                server.settimeout(.1)
                with self.assertRaises(socket.timeout):server.recvfrom(8192)
        with socket.socket() as server:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind((fields[0]['address'], 49010));server.listen();server.settimeout(3)
            request = b'OPTIONS rtsp://127.0.0.2 RTSP/1.0\r\nCSeq: 1\r\n\r\n'
            response = b'RTSP/1.0 200 OK\r\nCSeq: 1\r\n\r\n'
            with socket.create_connection((fields[1]['address'], 49010), timeout=3) as client:
                client.sendall(request)
                connection, sender = server.accept()
                with connection:
                    self.assertEqual(sender[0], fields[0]['peer'])
                    self.assertEqual(connection.recv(8192), request)
                    connection.sendall(response)
                self.assertEqual(client.recv(8192), response)
                self.assertEqual(client.recv(8192), b'')
        old_key = fields[0]['key']
        self.pair.close()
        self.assertTrue(all(not p.exists() for p in self.pair.paths))
        self.pair = Pair();self.assertTrue(self.pair.connect())
        current = dict(line.split('=', 1) for line in self.pair.ready[0].read_text().splitlines())
        self.assertNotEqual(current['key'], old_key)

    def test_different_game_generation_identity_never_admitted(self):
        self.pair = Pair(different_identity=True)
        self.assertFalse(self.pair.connect())
        self.assertFalse(self.pair.ready)

    def test_concurrent_guests_have_independent_local_endpoints(self):
        self.pair = Pair();other = Pair()
        try:
            self.assertTrue(self.pair.connect());self.assertTrue(other.connect())
            configs = [[dict(line.split('=', 1) for line in pair.ready[i].read_text().splitlines())
                        for i in (0, 1)] for pair in (self.pair, other)]
            self.assertEqual(len({c['address'] for pair in configs for c in pair}), 4)
            client_ports = {}
            def exchange(index):
                host, guest = configs[index]
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
                    server.bind((host['address'], 49013));server.settimeout(2)
                    client.bind(('127.0.0.1', client_ports.get(index, 0)));client.settimeout(2)
                    client_ports[index] = client.getsockname()[1]
                    packet = os.urandom(32);client.sendto(packet, (guest['address'], 49013))
                    value, address = server.recvfrom(100);self.assertEqual(value, packet)
                    server.sendto(value, address);self.assertEqual(client.recvfrom(100)[0], packet)
            exchange(0);exchange(1)
            self.pair.close();self.pair = None
            exchange(1)
        finally:
            other.close()

    def test_wrong_certificate_fingerprint_never_admitted(self):
        self.pair = Pair(bad_fingerprint=True)
        self.assertFalse(self.pair.connect(timeout=5))
        self.assertFalse(self.pair.ready)

    def test_invalid_parent_input(self):
        for value in ('{}\n', 'x'*70000+'\n'):
            result = subprocess.run([BINARY], input=value, text=True, capture_output=True, timeout=5)
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn('ready', result.stdout)

    def test_candidate_batches_require_current_identity_and_bounded_records(self):
        config = {'host': True, 'identity': 'ab'*32, 'seconds': 30, 'ice': []}
        for packet in ({'identity': 'cd'*32, 'candidates': [{'candidate': 'candidate:1 1 UDP 1 127.0.0.1 9 typ host', 'mid': '0'}]},
                       {'identity': 'ab'*32, 'candidates': [{}]*33},
                       {'identity': 'ab'*32, 'candidates': [{'candidate': 'x'*513, 'mid': '0'}]}):
            result = subprocess.run([BINARY], input=json.dumps(config)+'\n'+json.dumps(packet)+'\n', text=True, capture_output=True, timeout=5)
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn('"event":"ready"', result.stdout)

    def test_parent_eof_closes_session(self):
        self.pair = Pair();self.assertTrue(self.pair.connect())
        self.pair.processes[0].stdin.close()
        self.pair.processes[0].wait(timeout=5)
        self.assertFalse(self.pair.ready[0].exists())

    def test_deliberate_departure_closes_both_without_error(self):
        self.pair = Pair();self.assertTrue(self.pair.connect())
        self.pair.send(1, {'close': True})
        for p in self.pair.processes:
            self.assertEqual(p.wait(timeout=5), 0)

    def test_duplicate_description_cannot_replace_connection(self):
        self.pair = Pair();self.assertTrue(self.pair.connect())
        description = next(v['description'] for v in self.pair.output if v['event'] == 'description')
        self.pair.send(1, {'identity': self.pair.identity[1], 'description': description})
        self.assertNotEqual(self.pair.processes[1].wait(timeout=5), 0)


if __name__ == '__main__':
    unittest.main()
