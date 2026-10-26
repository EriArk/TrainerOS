#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real namespace/UDP/IPC/ICE checks. The child is a protocol fixture, not a game."""
import base64
import json
from pathlib import Path
import runpy
import subprocess
import sys
import unittest

transport, endpoint = map(lambda p: str(Path(p).resolve()), sys.argv[1:3])
sys.argv = [sys.argv[0], transport]
Pair = runpy.run_path(str(Path(__file__).parents[1] / 'remote-play-stream/transport/check.py'))['Pair']

CHILD = r'''
import base64,json,socket,sys
port=int(sys.argv[1]);peer=37392 if port==37391 else 37391
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',port));s.settimeout(.5)
print('ready',flush=True)
for line in sys.stdin:
 v=json.loads(line)
 if 'send' in v:
  data=base64.b64decode(v['send'])
  if v.get('foreign'):
   other=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);other.sendto(data,('127.0.0.1',peer));other.close()
  else:s.sendto(data,('127.0.0.1',peer))
  print('sent',flush=True)
 elif 'receive' in v:
  try:
   data,source=s.recvfrom(65536);print(json.dumps({'data':base64.b64encode(data).decode(),'port':source[1]}),flush=True)
  except socket.timeout:print('timeout',flush=True)
 elif 'alive' in v:print('alive',flush=True)
 else:break
'''


class EndpointTests(unittest.TestCase):
    def test_native_ports_payloads_and_disconnect_keeps_game(self):
        pair = Pair(modes=['cabinet', 'cabinet'])
        children = []
        try:
            self.assertTrue(pair.connect())
            for i in (0, 1):
                p = subprocess.Popen(['/usr/bin/bwrap', '--die-with-parent', '--unshare-net',
                    '--bind', '/', '/', '--', endpoint, str(pair.ready[i]), '/usr/bin/python3',
                    '-u', '-c', CHILD, str(37391+i)], stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                children.append(p)
                self.assertEqual(p.stdout.readline().strip(), 'ready')

            def command(i, value):
                p=children[i];p.stdin.write(json.dumps(value)+'\n');p.stdin.flush()
                return p.stdout.readline().strip()

            for sender in (0, 1):
                for size in (1, 10, 256, 4096, 0x4004):
                    data=bytes((n+sender)%256 for n in range(size))
                    encoded=base64.b64encode(data).decode()
                    self.assertEqual(command(sender, {'send':encoded}), 'sent')
                    received=json.loads(command(1-sender, {'receive':True}))
                    self.assertEqual(received, {'data':encoded, 'port':37391+sender})
                for invalid in ({'send':'YWJj', 'foreign':True},
                                {'send':base64.b64encode(b'x'*0x4005).decode()}):
                    self.assertEqual(command(sender, invalid), 'sent')
                    self.assertEqual(command(1-sender, {'receive':True}), 'timeout')
            # Native Naomi's sync reply advertises the host port. It must be
            # reachable in the guest namespace without rewriting the payload.
            advertised=bytes.fromhex('01000100')+(37391).to_bytes(2,'big')+b'\x7f\x00\x00\x01'
            encoded=base64.b64encode(advertised).decode()
            self.assertEqual(command(0, {'send':encoded}), 'sent')
            self.assertEqual(json.loads(command(1, {'receive':True}))['data'], encoded)
            pair.close()
            for i in (0, 1):
                self.assertEqual(command(i, {'alive':True}), 'alive')
                self.assertEqual(command(i, {'receive':True}), 'timeout')
                self.assertIsNone(children[i].poll())
        finally:
            for p in children:
                p.stdin.close()
                try:p.wait(timeout=5)
                except subprocess.TimeoutExpired:p.kill();p.wait()
                p.stdout.close();p.stderr.close()
            if any(p.poll() is None for p in pair.processes):pair.close()

    def test_refuses_host_network_before_launch(self):
        p=subprocess.run([endpoint, '/nonexistent/config', '/bin/true', 'unused'], timeout=5)
        self.assertEqual(p.returncode, 2)


if __name__ == '__main__':
    unittest.main()
