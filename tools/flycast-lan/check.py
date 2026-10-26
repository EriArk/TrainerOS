#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real namespace/UDP/IPC/ICE checks. The child is a protocol fixture, not a game."""
import base64
import json
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import os
import unittest

transport, endpoint = map(lambda p: str(Path(p).resolve()), sys.argv[1:3])
sys.argv = [sys.argv[0], transport]
Pair = runpy.run_path(str(Path(__file__).parents[1] / 'remote-play-stream/transport/check.py'))['Pair']

CHILD = r'''
import base64,json,socket,sys
port=int(sys.argv[1])
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',port));s.settimeout(.5)
print('ready',flush=True)
for line in sys.stdin:
 v=json.loads(line)
 if 'send' in v:
  data=base64.b64decode(v['send']);peer=int(v['target'])
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
    def test_two_three_four_cabinets_all_routes_and_disconnect(self):
        for count in (2,3,4):
            with self.subTest(players=count), tempfile.TemporaryDirectory() as temp:
                pairs=[];children=[]
                try:
                    for guest in range(2,count+1):
                        pair=Pair(modes=['cabinet','cabinet']);pairs.append(pair)
                        self.assertTrue(pair.connect())
                    for seat in range(1,count+1):
                        sockets=({str(n+2):str(pair.ready[0].parent/'cabinet.sock') for n,pair in enumerate(pairs)}
                                 if seat==1 else {'1':str(pairs[seat-2].ready[1].parent/'cabinet.sock')})
                        config=Path(temp)/str(seat)
                        config.write_text(json.dumps({'protocol':2,'seat':seat,'players':count,'sockets':sockets}));config.chmod(0o600)
                        p=subprocess.Popen(['/usr/bin/bwrap','--die-with-parent','--unshare-net','--bind','/','/','--',endpoint,str(config),'/usr/bin/python3','-u','-c',CHILD,str(37390+seat)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
                        children.append(p);self.assertEqual(p.stdout.readline().strip(),'ready')
                    def command(seat,value):
                        p=children[seat-1];p.stdin.write(json.dumps(value)+'\n');p.stdin.flush();return p.stdout.readline().strip()
                    for sender in range(1,count+1):
                        for target in range(1,count+1):
                            if sender==target:continue
                            for size in (1,10,256,4096,0x4004):
                                data=bytes((n+sender)%256 for n in range(size));encoded=base64.b64encode(data).decode()
                                self.assertEqual(command(sender,{'send':encoded,'target':37390+target}),'sent')
                                self.assertEqual(json.loads(command(target,{'receive':True})),{'data':encoded,'port':37390+sender})
                            for invalid in ({'send':'YWJj','foreign':True},{'send':base64.b64encode(b'x'*0x4005).decode()}):
                                self.assertEqual(command(sender,dict(invalid,target=37390+target)),'sent')
                                self.assertEqual(command(target,{'receive':True}),'timeout')
                    # Native SyncReply contains the next cabinet's loopback port:
                    # route this unmodified to a guest, then follow that address.
                    next_seat=count
                    data=bytes.fromhex('01000100')+(37390+next_seat).to_bytes(2,'big')+b'\x7f\x00\x00\x01'
                    encoded=base64.b64encode(data).decode();command(1,{'send':encoded,'target':37392})
                    self.assertEqual(json.loads(command(2,{'receive':True}))['data'],encoded)
                    for pair in pairs:pair.close()
                    for seat,p in enumerate(children,1):
                        self.assertEqual(command(seat,{'alive':True}),'alive');self.assertIsNone(p.poll())
                        self.assertEqual(command(seat,{'receive':True}),'timeout')
                finally:
                    for p in children:
                        p.stdin.close()
                        try:p.wait(timeout=5)
                        except subprocess.TimeoutExpired:p.kill();p.wait()
                        p.stdout.close();p.stderr.close()
                    for pair in pairs:
                        if any(p.poll() is None for p in pair.processes):pair.close()

    def test_refuses_host_network_before_launch(self):
        p=subprocess.run([endpoint, '/nonexistent/config', '/bin/true', 'unused'], timeout=5)
        self.assertEqual(p.returncode, 2)


if __name__ == '__main__':
    unittest.main()
