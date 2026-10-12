#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A game's native IP interface, isolated from the host and owned by one session.

Invoked only by network-session.py inside a fresh user/network namespace.
The emulator supplies its own networking; no game or save protocol is implemented.
"""
import array
import ctypes
import fcntl
import json
import os
from pathlib import Path
import native_log
import select
import signal
import socket
import struct
import subprocess
import sys

config = json.loads(Path(sys.argv[1]).read_text())
assert os.getuid()==0 and os.readlink('/proc/self/ns/net')!=sys.argv[3]
control = socket.socket(fileno=int(sys.argv[2]))
tap = os.open('/dev/net/tun', os.O_RDWR | os.O_NONBLOCK)
name = fcntl.ioctl(tap, 0x400454ca, struct.pack('16sH', b'trainer-party', 0x1002))[:16].split(b'\0')[0].decode()
address = '10.203.0.1' if config['channel']['host'] else '10.203.0.2'
for command in [
    ['ip', 'link', 'set', 'lo', 'up'],
    ['ip', 'link', 'set', name, 'address', config['mac']],
    ['ip', 'addr', 'add', address + '/30', 'dev', name],
    ['ip', 'link', 'set', name, 'up'],
]:
    subprocess.run(command, check=True)
control.sendmsg([b'ready'], [(socket.SOL_SOCKET, socket.SCM_RIGHTS, array.array('i', [tap]))])
os.close(tap)
# Network setup is complete. The game needs neither namespace administrator
# capabilities nor root-exec capability regain. Drop them before launching it,
# retaining ordinary same-owner /proc visibility for the shell's Home controls.
libc = ctypes.CDLL(None, use_errno=True)
def checked(result):
    if result != 0: raise OSError(ctypes.get_errno(), 'drop namespace capabilities')
checked(libc.prctl(28, 15, 0, 0, 0))  # LOCKED NOROOT + NO_SETUID_FIXUP
checked(libc.prctl(38, 1, 0, 0, 0))   # NO_NEW_PRIVS
class Header(ctypes.Structure):
    _fields_ = [('version', ctypes.c_uint32), ('pid', ctypes.c_int)]
class Capabilities(ctypes.Structure):
    _fields_ = [('effective', ctypes.c_uint32), ('permitted', ctypes.c_uint32), ('inheritable', ctypes.c_uint32)]
header = Header(0x20080522, 0)
caps = (Capabilities * 2)()
checked(libc.capset(ctypes.byref(header), ctypes.byref(caps)))
checked(libc.prctl(4, 1, 0, 0, 0))    # DUMPABLE: original owner may inspect PID
user = Path(config['user'])
runtime = Path(config['runtime'])
runtime_root = Path(config['runtimeRoot'])
native = None
try:
    assert control.recv(32) == b'start'
    env = dict(os.environ)
    env.update(XDG_CONFIG_HOME=str(user/'config'), XDG_DATA_HOME=str(user/'data'),
               XDG_CACHE_HOME=str(user/'cache'), APPDIR=str(runtime_root),
               APPIMAGE=str(runtime), SDL_VIDEODRIVER='x11',TRAINEROS_VITA_MANAGED='1')
    parent=os.getpid()
    def guard():
        signal.signal(signal.SIGTERM,signal.SIG_DFL)
        checked(libc.prctl(1,signal.SIGTERM,0,0,0))
        if os.getppid()!=parent:os.kill(os.getpid(),signal.SIGTERM)
    native = subprocess.Popen([str(runtime), '--frontend', 'sdl',
        '-r', config['title'], '-F', '-c', str(user/'config/Vita3K/config.yml'), '--keep-config'],
        cwd=user, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        start_new_session=True,preexec_fn=guard)
    native_log.capture(native.stdout,Path(sys.argv[1]).parent/'native.log')
    control.send(json.dumps({'native': native.pid}).encode())
    while native.poll() is None:
        readable, _, _ = select.select([control], [], [], .25)
        if readable:
            try: control.recv(32)
            except ConnectionResetError: pass
            break
    if native.poll() is not None:
        try: control.send(json.dumps({'exited':native.returncode}).encode())
        except (BrokenPipeError,ConnectionResetError): pass
finally:
    if native is not None:
        try:
            os.killpg(native.pid, signal.SIGTERM)
            native.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(native.pid, signal.SIGKILL)
            native.wait()
        except ProcessLookupError:
            pass
    control.close()
