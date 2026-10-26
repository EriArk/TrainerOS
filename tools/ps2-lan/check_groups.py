#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real 2/3/4-console IPC/namespace/ICE paths; child protocol fixtures, not games."""
import base64,json,os,runpy,select,subprocess,sys,tempfile,time,unittest
from pathlib import Path
transport,endpoint=map(lambda p:str(Path(p).resolve()),sys.argv[1:3])
sys.argv=[sys.argv[0],transport]
Pair=runpy.run_path(str(Path(__file__).parents[1]/'remote-play-stream/transport/check.py'))['Pair']
CHILD=r"""
import base64,json,socket,struct,sys,threading,queue
mode,seat,path=sys.argv[1:];seat=int(seat);received=queue.Queue();peers={}
s=socket.socket(socket.AF_UNIX if mode=='bba' else socket.AF_INET,socket.SOCK_STREAM if mode=='bba' else socket.SOCK_DGRAM)
if mode=='bba':s.connect(path)
else:s.bind(('127.0.0.1',19072 if seat==1 else 0))
def eclx(payload=b'',who=seat):
 b=bytearray(44);b[:6]=b'ECLX\x01'+bytes([3 if payload else 1]);struct.pack_into('>H',b,6,44);struct.pack_into('>I',b,8,who);struct.pack_into('>H',b,22,1);struct.pack_into('>H',b,24,len(payload));return bytes(b)+payload
def exact(n):
 b=b''
 while len(b)<n:
  x=s.recv(n-len(b))
  if not x:return None
  b+=x
 return b
def read():
 while True:
  if mode=='bba':
   n=exact(2)
   if not n:return
   b=exact(struct.unpack('<H',n)[0])
   if b is None:return
   received.put(b)
  else:
   b,addr=s.recvfrom(2048)
   if seat==1:
    who=struct.unpack_from('>I',b,8)[0];peers[who]=addr
    if b[5]==1:continue
    for target,peer in peers.copy().items():
     if target!=who:s.sendto(b,peer)
   if b[5]==3:received.put(b[44:])
threading.Thread(target=read,daemon=True).start()
if mode=='ps2' and seat!=1:s.sendto(eclx(),('127.0.0.1',19073))
print('ready',flush=True)
for line in sys.stdin:
 v=json.loads(line)
 if 'send' in v:
  b=base64.b64decode(v['send'])
  if mode=='bba':
   wire=struct.pack('<H',len(b))+b;s.sendall(wire[:1]);s.sendall(wire[1:])
  elif seat==1:
   for peer in peers.copy().values():s.sendto(eclx(b),peer)
  else:s.sendto(eclx(b),('127.0.0.1',19073))
  print('sent',flush=True)
 elif 'receive' in v:
  try:print(base64.b64encode(received.get(timeout=.7)).decode(),flush=True)
  except queue.Empty:print('timeout',flush=True)
 elif 'alive' in v:print('alive',flush=True)
 elif 'peers' in v:print(len(peers),flush=True)
 else:break
"""
class Groups(unittest.TestCase):
 def test_native_groups_all_sources_and_revocation(self):
  for mode in ('ps2','bba'):
   for count in (2,3,4):
    with self.subTest(mode=mode,players=count),tempfile.TemporaryDirectory(prefix='lan-groups-') as temporary:
     pairs=[];children=[]
     try:
      for guest in range(2,count+1):
       pair=Pair(modes=[mode,mode]);pairs.append(pair);self.assertTrue(pair.connect())
      for seat in range(1,count+1):
       sockets=({str(n+2):str(pair.ready[0].parent/(mode+'.sock')) for n,pair in enumerate(pairs)} if seat==1 else {'1':str(pairs[seat-2].ready[1].parent/(mode+'.sock'))})
       directory=Path(temporary)/str(seat);directory.mkdir(mode=0o700);config=directory/'connection.json'
       config.write_text(json.dumps({'protocol':2,'mode':mode,'room':'abcdef123456','seat':seat,'players':count,'sockets':sockets}));config.chmod(0o600)
       command=[endpoint,str(config),sys.executable,'-u','-c',CHILD,mode,str(seat),str(directory/'bba.sock')]
       if mode=='ps2':command=['/usr/bin/bwrap','--die-with-parent','--unshare-net','--bind','/','/','--']+command
       p=subprocess.Popen(command,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True);children.append(p)
       self.assertTrue(select.select([p.stdout],[],[],8)[0],'Child startup timeout');self.assertEqual(p.stdout.readline().strip(),'ready')
      def command(seat,value):
       p=children[seat-1];p.stdin.write(json.dumps(value)+'\n');p.stdin.flush();self.assertTrue(select.select([p.stdout],[],[],4)[0]);return p.stdout.readline().strip()
      if mode=='ps2':
       for _ in range(50):
        if command(1,{'peers':True})==str(count-1):break
        time.sleep(.05)
       self.assertEqual(command(1,{'peers':True}),str(count-1))
      for sender in range(1,count+1):
       for size in ((1,60,1000) if mode=='ps2' else (14,60,1518)):
        data=os.urandom(size)
        if mode=='bba':data=b'\xff'*6+bytes.fromhex('0009bfabcd')+bytes([sender])+b'\x08\x06'+data[14:]
        encoded=base64.b64encode(data).decode();self.assertEqual(command(sender,{'send':encoded}),'sent')
        for target in range(1,count+1):
         if target!=sender:self.assertEqual(command(target,{'receive':True}),encoded)
        self.assertEqual(command(sender,{'receive':True}),'timeout','Echoed source packet')
      if mode=='bba':
       # An admitted guest cannot claim another console's source MAC.
       forged=b'\xff'*6+bytes.fromhex('0009bfabcd01')+b'\x08\x06'
       self.assertEqual(command(count,{'send':base64.b64encode(forged).decode()}),'sent')
       for target in range(1,count):self.assertEqual(command(target,{'receive':True}),'timeout')
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
if __name__=='__main__':unittest.main()
