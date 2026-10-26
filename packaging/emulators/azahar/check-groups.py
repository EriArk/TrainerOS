#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real secure-channel/group lifecycle checks with a UDP fixture, not emulation."""
import argparse
import asyncio
import base64
import json
import os
from pathlib import Path
import signal
import socket
import sys
import tempfile
import time


def native_fixture():
    config = json.loads(Path(os.environ['TRAINEROS_AZAHAR_SESSION']).read_text())
    root = Path(config['user'])
    (root/'pid').write_text(str(os.getpid()))
    running = True
    def stop(*_):
        nonlocal running
        running = False
    signal.signal(signal.SIGTERM,stop)
    signal.signal(signal.SIGINT,stop)
    room = None
    if config['host']:
        room=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        room.bind(('127.0.0.1',config['port']));room.setblocking(False)
        print('[TrainerOS3DS] room-ready',flush=True)
    client=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    client.bind(('127.0.0.1',0));client.setblocking(False)
    seat=int(config['nickname'][6:]);peers={};received=set();announced=False
    try:
        while running:
            client.sendto(bytes([seat]),('127.0.0.1',config['port']))
            if room:
                while True:
                    try:payload,address=room.recvfrom(4096)
                    except BlockingIOError:break
                    if len(payload)!=1 or not 1<=payload[0]<=config['players']:raise ValueError('fixture seat')
                    if payload[0] in peers and peers[payload[0]]!=address:raise ValueError('UDP address collision')
                    peers[payload[0]]=address
                    for peer in peers.values():room.sendto(payload,peer)
            while True:
                try:payload,address=client.recvfrom(4096)
                except BlockingIOError:break
                received.add(payload[0])
                if not announced:
                    announced=True;print('[TrainerOS3DS] running',flush=True)
            tmp=root/'received.tmp';tmp.write_text(json.dumps(sorted(received)));tmp.replace(root/'received.json')
            time.sleep(.02)
    finally:
        client.close()
        if room:room.close()


async def exercise(args,players):
    with tempfile.TemporaryDirectory(prefix='traineros-room-check-') as temporary:
        root=Path(temporary);children=[];pids=[]
        async def start(seat,endpoint=None):
            folder=root/str(seat);folder.mkdir();user=folder/'user';user.mkdir()
            game=folder/'fixture';game.write_text('transport fixture only')
            channel=dict(endpoint or {},host=seat==1,mode=args.mode,
                         identity='a'*64,players=players,seat=seat)
            if args.mode=='nearby':channel['address']='127.0.0.1'
            elif args.relay_only:channel['relayOnly']=True
            config={'channel':channel,'content':str(game),'runtime':args.runtime or str(Path(__file__).resolve()),
                    'python':args.python,'helper':args.helper,'user':str(user),'sysdata':str(user)}
            file=folder/'network.json';file.write_text(json.dumps(config));file.chmod(0o600)
            process=await asyncio.create_subprocess_exec(args.python,str(Path(__file__).with_name('room-session.py')),str(file),
                stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
            children.append(process);return process
        async def event(process):
            line=await asyncio.wait_for(process.stdout.readline(),30)
            assert line.startswith(b'TRAINEROS_3DS '),(line,await process.stderr.read() if not line else '')
            result=json.loads(base64.b64decode(line.split()[1]));assert result['event']!='error',result
            return result
        try:
            host=await start(1);endpoints={}
            while len(endpoints)<players-1:
                value=await event(host);assert value['event']=='ready',value
                seat=value['seat'];assert seat not in endpoints and 2<=seat<=players
                endpoints[seat]=value['endpoint']
            assert len({e['token' if args.mode=='nearby' else 'code'] for e in endpoints.values()})==players-1
            for seat in range(2,players+1):await start(seat,endpoints[seat])
            for child in children:assert (await event(child))['event']=='running'
            deadline=time.monotonic()+10
            while time.monotonic()<deadline:
                if all((root/str(seat)/'user/received.json').exists() and
                       json.loads((root/str(seat)/'user/received.json').read_text())==list(range(1,players+1))
                       for seat in range(1,players+1)):break
                await asyncio.sleep(.1)
            else:raise AssertionError('Not every native UDP client received every player')
            pids=[int((root/str(seat)/'user/pid').read_text()) for seat in range(1,players+1)]
            # A departed guest closes the complete group, with no orphan helpers or native children.
            if args.drop_channel:
                parent=children[-1].pid
                descendants=Path('/proc',str(parent),'task',str(parent),'children').read_text().split()
                helpers=[int(pid) for pid in descendants if args.helper.encode() in Path('/proc',pid,'cmdline').read_bytes().split(b'\0')]
                assert len(helpers)==1,helpers
                os.kill(helpers[0],signal.SIGKILL)
            else:
                children[-1].terminate()
            for child in children:await asyncio.wait_for(child.wait(),35 if args.drop_channel else 15)
            assert all(not Path('/proc',str(pid)).exists() for pid in pids),'native child leaked'
            print(f'PASS {args.mode} {players} players: independent admissions, all-player UDP, {"channel crash" if args.drop_channel else "guest exit"} and cleanup',flush=True)
        finally:
            for child in children:
                if child.returncode is None:child.terminate()
            for child in children:
                try:await asyncio.wait_for(child.wait(),10)
                except asyncio.TimeoutError:child.kill();await child.wait()


if __name__=='__main__':
    if 'TRAINEROS_AZAHAR_SESSION' in os.environ:
        native_fixture()
    else:
        parser=argparse.ArgumentParser(description=__doc__)
        parser.add_argument('--python',required=True);parser.add_argument('--helper',required=True)
        parser.add_argument('--mode',choices=['nearby','online'],default='nearby')
        parser.add_argument('--relay-only',action='store_true')
        parser.add_argument('--drop-channel',action='store_true')
        parser.add_argument('--counts',nargs='+',type=int,choices=[2,3,4],default=[2,3,4,4])
        parser.add_argument('--runtime',help='Optional native-room-probe linked to pinned Azahar libraries')
        args=parser.parse_args()
        for players in args.counts:asyncio.run(exercise(args,players))
