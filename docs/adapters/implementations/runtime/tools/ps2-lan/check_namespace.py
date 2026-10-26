#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Linux host check: real namespace UDP isolation and game lifetime on loss."""
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile


def packet(peer=1):
    value=bytearray(44);value[:6]=b'ECLX\x01\x01'
    struct.pack_into('>I',value,8,peer);struct.pack_into('>H',value,6,44);struct.pack_into('>H',value,22,1)
    return bytes(value)


def game(role, root):
    udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);udp.settimeout(8)
    udp.bind(('0.0.0.0',19072 if role=='host' else 0))
    if role=='guest':udp.sendto(packet(2),('127.0.0.1',19073))
    (root/'ready').touch()
    data,peer=udp.recvfrom(2048);assert data==packet(2 if role=='host' else 1)
    udp.sendto(packet(1 if role=='host' else 2),peer)
    # Parent closes the channel, then tells the still-owned game to finish.
    import time
    for _ in range(100):
        if (root/'exit').exists():return
        time.sleep(.05)
    raise AssertionError('Game did not receive its own exit')


def check(endpoint, role):
    with tempfile.TemporaryDirectory(prefix='ps2-check-') as directory:
        root=Path(directory);server=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
        server.bind(str(root/'ps2.sock'));server.listen(1);server.settimeout(8)
        host=role=='host'
        config=root/'session.conf'
        config.write_text(json.dumps({'protocol':2,'mode':'ps2','room':'123456abcdef','seat':1 if host else 2,'players':2,'sockets':{'2' if host else '1':str(root/'ps2.sock')}}))
        config.chmod(0o600)
        # The exact same UDP port is occupied outside: namespace bind must work.
        outside=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);outside.bind(('0.0.0.0',19072 if host else 19073))
        command=[str(endpoint),str(config),sys.executable,str(Path(__file__).resolve()),'game',role,str(root)]
        rejected=subprocess.run(command,capture_output=True,timeout=8)
        assert rejected.returncode==2,'Endpoint accepted ordinary host interfaces'
        process=subprocess.Popen(['/usr/bin/bwrap','--die-with-parent','--unshare-net','--bind','/','/','--proc','/proc','--']+command)
        try:
            channel,_=server.accept();channel.settimeout(8)
            import time
            for _ in range(100):
                if (root/'ready').exists():break
                assert process.poll() is None
                time.sleep(.05)
            assert (root/'ready').exists()
            def receive():
                result=b''
                while len(result)<46:
                    part=channel.recv(46-len(result));assert part,'Channel closed';result+=part
                assert result==struct.pack('>H',44)+packet(1 if host else 2)
            if not host:receive()
            channel.sendall(struct.pack('>H',44)+packet(2 if host else 1));receive()
            channel.close();time.sleep(.3)
            assert process.poll() is None,'Transport loss killed the game'
            (root/'exit').touch();assert process.wait(timeout=8)==0
        finally:
            if process.poll() is None:process.terminate();process.wait(timeout=5)
            outside.close();server.close()


if __name__=='__main__':
    if sys.argv[1]=='game':game(sys.argv[2],Path(sys.argv[3]))
    else:
        for role in ('host','guest'):check(Path(sys.argv[1]).resolve(),role)
        print('PS2_NAMESPACE_BOUNDARY_PASSED')
