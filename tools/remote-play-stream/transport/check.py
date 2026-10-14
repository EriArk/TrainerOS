#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real ICE/DTLS/socket boundary checks; no codec, game or uinput needed."""
import json
import os
from pathlib import Path
import queue
import re
import socket
import subprocess
import sys
import threading
import time
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())


class Pair:
    def __init__(self, different_identity=False, bad_fingerprint=False):
        self.events = queue.Queue()
        self.ready, self.paths, self.output = {}, [], []
        self.processes = [subprocess.Popen([BINARY], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                          for _ in range(2)]
        self.identity = ['ab'*32, ('cd' if different_identity else 'ab')*32]
        self.bad_fingerprint = bad_fingerprint
        for i, p in enumerate(self.processes):
            threading.Thread(target=self.read, args=(i, p), daemon=True).start()
            self.send(i, {'host': i == 0, 'identity': self.identity[i], 'seconds': 30, 'ice': []})

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

    def test_existing_udp_and_rtsp_with_private_fresh_material(self):
        self.pair = Pair()
        self.assertTrue(self.pair.connect())
        fields = []
        for path in self.pair.ready.values():
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            self.assertEqual(path.parent.stat().st_mode & 0o777, 0o700)
            self.assertEqual(path.stat().st_uid, os.getuid())
            fields.append(dict(line.split('=', 1) for line in path.read_text().splitlines()))
        for name in ('key', 'ping', 'connect'):
            self.assertEqual(fields[0][name], fields[1][name])
            self.assertNotIn(fields[0][name], json.dumps(self.pair.output))
        for port in (49011, 49012, 49013):
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
                server.bind(('127.0.0.1', port));server.settimeout(3)
                client.bind(('127.0.0.1', 0));client.settimeout(3)
                request = os.urandom(1392)
                client.sendto(request, ('127.0.0.2', port))
                data, sender = server.recvfrom(8192)
                self.assertEqual(sender[0], '127.0.0.2')
                self.assertEqual(data, request)
                server.sendto(data, sender)
                self.assertEqual(client.recvfrom(8192)[0], request)
                # An unrelated local socket cannot replace the selected client.
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as other:
                    other.bind(('127.0.0.1', 0));other.sendto(b'wrong peer', ('127.0.0.2', port))
                server.settimeout(.1)
                with self.assertRaises(socket.timeout):server.recvfrom(8192)
        with socket.socket() as server:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind(('127.0.0.1', 49010));server.listen();server.settimeout(3)
            request = b'OPTIONS rtsp://127.0.0.2 RTSP/1.0\r\nCSeq: 1\r\n\r\n'
            response = b'RTSP/1.0 200 OK\r\nCSeq: 1\r\n\r\n'
            with socket.create_connection(('127.0.0.2', 49010), timeout=3) as client:
                client.sendall(request)
                connection, sender = server.accept()
                with connection:
                    self.assertEqual(sender[0], '127.0.0.2')
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

    def test_wrong_certificate_fingerprint_never_admitted(self):
        self.pair = Pair(bad_fingerprint=True)
        self.assertFalse(self.pair.connect(timeout=5))
        self.assertFalse(self.pair.ready)

    def test_invalid_parent_input(self):
        for value in ('{}\n', 'x'*70000+'\n'):
            result = subprocess.run([BINARY], input=value, text=True, capture_output=True, timeout=5)
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn('ready', result.stdout)

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
