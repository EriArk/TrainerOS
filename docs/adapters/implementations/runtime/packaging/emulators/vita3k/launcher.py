#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Ordinary installed-game launch and preparation for an accepted Vita party."""
import argparse
import array
import ctypes
import fcntl
import json
import os
from pathlib import Path
import native_log
import signal
import socket
import struct
import subprocess
import sys

import game


def own_mac():
    # Match upstream Linux sceNetGetMacAddress: first non-loopback IPv4
    # interface. Preserve this identity inside the private network as well.
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
        memory=array.array('B',b'\0'*1024)
        address,size=memory.buffer_info()
        info=fcntl.ioctl(sock.fileno(),0x8912,struct.pack('iP',size,address))
        used=struct.unpack_from('i',info)[0]
        for offset in range(0,used,40):
            name=memory.tobytes()[offset:offset+16].split(b'\0')[0]
            request=struct.pack('256s',name)
            flags=struct.unpack_from('H',fcntl.ioctl(sock.fileno(),0x8913,request),16)[0]
            if flags&8:continue
            mac=fcntl.ioctl(sock.fileno(),0x8927,request)[18:24]
            if len(mac)==6 and any(mac) and not mac[0]&1:return ':'.join(f'{part:02x}' for part in mac)
    raise ValueError('own console identity unavailable')


def private_json(path,value):
    with Path(path).open('x') as stream:
        os.chmod(path,0o600)
        json.dump(value,stream,separators=(',',':'))
        stream.flush()
        os.fsync(stream.fileno())


def prepare(root,path):
    path=Path(path)
    if not path.is_absolute() or path.is_symlink() or not path.is_file() or path.stat().st_size>8192:
        raise ValueError('invitation file')
    request=json.loads(path.read_text())
    if set(request)!={'link','expected','channel'}:raise ValueError('invitation fields')
    current=game.identity(root,request['link'])
    expected=request['expected']
    if not isinstance(expected,dict) or any(expected.get(key)!=value for key,value in current.items()):
        raise ValueError('game or runtime changed')
    user=path.resolve().parent/'user'
    title=game.prepare_settings(root,request['link'],user,network=True)
    config={'channel':request['channel'],'python':str(root/'transport/venv/bin/python'),
        'helper':str(root/'transport/secure-channel.py'),'runtime':str(root/'runtime/AppRun'),
        'runtimeRoot':str(root/'runtime'),'user':str(user),'title':title,'mac':own_mac()}
    private_json(path.parent/'network.json',config)
    return {'ready':True}


def solo(root,link,folder):
    game.bundle(root)
    folder=Path(folder)
    if not folder.is_absolute() or folder.is_symlink() or not folder.is_dir():
        raise ValueError('owned solo directory')
    user=folder/'user'
    title=game.prepare_settings(root,link,user)
    env=dict(os.environ)
    env.update(XDG_CONFIG_HOME=str(user/'config'),XDG_DATA_HOME=str(user/'data'),
        XDG_CACHE_HOME=str(user/'cache'),APPDIR=str(root/'runtime'),APPIMAGE=str(root/'runtime/AppRun'),
        SDL_VIDEODRIVER='x11',PYTHONDONTWRITEBYTECODE='1',TRAINEROS_VITA_MANAGED='1')
    native=None
    def stop(signum,frame):
        if native is not None and native.poll() is None:
            try:os.killpg(native.pid,signal.SIGTERM)
            except ProcessLookupError:pass
    signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
    parent=os.getpid()
    def guard():
        signal.signal(signal.SIGTERM,signal.SIG_DFL)
        libc=ctypes.CDLL(None,use_errno=True)
        if libc.prctl(1,signal.SIGTERM,0,0,0)!=0:raise OSError('native lifetime guard')
        if os.getppid()!=parent:os.kill(os.getpid(),signal.SIGTERM)
    try:
        native=subprocess.Popen([str(root/'runtime/AppRun'),'--frontend','sdl','-r',title,'-F',
            '-c',str(user/'config/Vita3K/config.yml'),'--keep-config'],cwd=user,env=env,
            stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True,preexec_fn=guard)
        native_log.capture(native.stdout,folder/'native.log')
        return native.wait()
    finally:
        if native is not None and native.poll() is None:
            stop(None,None)
            try:native.wait(timeout=5)
            except subprocess.TimeoutExpired:os.killpg(native.pid,signal.SIGKILL);native.wait()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode',choices=['identity','prepare','solo'])
    parser.add_argument('target',type=Path)
    parser.add_argument('--session-root',type=Path)
    args=parser.parse_args()
    root=Path(__file__).resolve().parent.parent
    if args.mode=='solo':
        if args.session_root is None:raise ValueError('owned launch directory required')
        return solo(root,args.target,args.session_root)
    result=game.identity(root,args.target) if args.mode=='identity' else prepare(root,args.target)
    print(json.dumps(result,separators=(',',':')),flush=True)
    return 0


if __name__=='__main__':
    try:sys.exit(main())
    except Exception:
        print(json.dumps({'error':'This Vita game or its play setup is unavailable. Check the installation and try again.'}),flush=True)
        sys.exit(1)
