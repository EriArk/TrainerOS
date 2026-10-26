#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real UDP/Ethernet, ICE/DTLS, assigned MACs and revocation; no game claim."""
import base64
import json
from pathlib import Path
import runpy
import select
import subprocess
import sys
import tempfile
import unittest

transport, endpoint = map(lambda p: str(Path(p).resolve()), sys.argv[1:3])
sys.argv = [sys.argv[0], transport]
Pair = runpy.run_path(str(Path(__file__).parents[3] / 'tools/remote-play-stream/transport/check.py'))['Pair']
CHILD = r'''
import socket,sys,base64,json,threading,queue
port,seat=map(int,sys.argv[1:]);q=queue.Queue();s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',0));s.connect(('127.0.0.1',port))
mac=bytes.fromhex('0250f2abcd')+bytes([seat]);s.send(b'\xff'*6+mac+b'\x88\xb5'+b'learn')
def receive():
 while True:
  data=s.recv(2048)
  if data[14:]!=b'learn':q.put(data)
threading.Thread(target=receive,daemon=True).start()
print('ready',flush=True)
for line in sys.stdin:
 v=json.loads(line)
 if 'send' in v:s.send(base64.b64decode(v['send']));print('sent',flush=True)
 elif 'receive' in v:
  try:print(base64.b64encode(q.get(timeout=.3)).decode(),flush=True)
  except queue.Empty:print('timeout',flush=True)
 elif 'alive' in v:print('alive',flush=True)
 else:break
'''


class Groups(unittest.TestCase):
    def test_two_three_four_and_repeat_group(self):
        for count in (2, 3, 4, 4):
            with self.subTest(players=count), tempfile.TemporaryDirectory(prefix='xbox-group-') as temp:
                pairs, children = [], []
                try:
                    for guest in range(2, count + 1):
                        pair = Pair(modes=['bba', 'bba'])
                        pairs.append(pair)
                        self.assertTrue(pair.connect())
                    for seat in range(1, count + 1):
                        sockets = ({str(n + 2): str(pair.ready[0].parent / 'bba.sock') for n, pair in enumerate(pairs)}
                                   if seat == 1 else {'1': str(pairs[seat - 2].ready[1].parent / 'bba.sock')})
                        path = Path(temp) / str(seat)
                        path.mkdir(mode=0o700)
                        config = path / 'connection.json'
                        config.write_text(json.dumps({'protocol': 2, 'mode': 'xbox', 'room': 'abcdef123456',
                                                      'seat': seat, 'players': count, 'sockets': sockets}))
                        config.chmod(0o600)
                        child = subprocess.Popen([endpoint, str(config), sys.executable, '-u', '-c', CHILD,
                                                  'TRAINEROS_XBOX_PORT', str(seat)], stdin=subprocess.PIPE,
                                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                        children.append(child)
                        self.assertTrue(select.select([child.stdout], [], [], 8)[0])
                        self.assertEqual(child.stdout.readline().strip(), 'ready')

                    def command(seat, value):
                        p = children[seat - 1]
                        p.stdin.write(json.dumps(value) + '\n')
                        p.stdin.flush()
                        self.assertTrue(select.select([p.stdout], [], [], 4)[0])
                        return p.stdout.readline().strip()

                    for sender in range(1, count + 1):
                        for size in (14, 60, 1518):
                            data = b'\xff' * 6 + bytes.fromhex('0250f2abcd') + bytes([sender]) + b'\x08\x06' + b'X' * (size - 14)
                            encoded = base64.b64encode(data).decode()
                            self.assertEqual(command(sender, {'send': encoded}), 'sent')
                            for target in range(1, count + 1):
                                self.assertEqual(command(target, {'receive': True}), 'timeout' if target == sender else encoded)
                    forged = b'\xff' * 6 + bytes.fromhex('0250f2abcd01') + b'\x08\x06'
                    self.assertEqual(command(count, {'send': base64.b64encode(forged).decode()}), 'sent')
                    for target in range(1, count):
                        self.assertEqual(command(target, {'receive': True}), 'timeout')
                    for pair in pairs:
                        pair.close()
                    for seat in range(1, count + 1):
                        self.assertEqual(command(seat, {'alive': True}), 'alive')
                        self.assertEqual(command(seat, {'receive': True}), 'timeout')
                finally:
                    for p in children:
                        p.stdin.close()
                        try:
                            p.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            p.kill()
                            p.wait()
                        p.stdout.close()
                        p.stderr.close()
                    for pair in pairs:
                        if any(p.poll() is None for p in pair.processes):
                            pair.close()


if __name__ == '__main__':
    unittest.main()
