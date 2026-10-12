# SPDX-License-Identifier: GPL-3.0-or-later
"""Create a party-local interface, then drop setup privileges before execution."""
import array
import ctypes
import fcntl
import os
import signal
import socket
import struct
import subprocess

import ethernet

libc = ctypes.CDLL(None, use_errno=True)


def checked(result):
    if result != 0:
        raise OSError(ctypes.get_errno(), 'native network setup')


def open_interface(slot, control_fd, outside_namespace):
    if os.readlink('/proc/self/ns/net') == outside_namespace:
        raise ValueError('native network must be isolated')
    control = socket.socket(fileno=control_fd)
    tap = os.open('/dev/net/tun', os.O_RDWR | os.O_NONBLOCK)
    try:
        name = fcntl.ioctl(tap, 0x400454ca, struct.pack('16sH', b'trainer-party', 0x1002))[:16].split(b'\0')[0].decode()
        mac = ':'.join(f'{value:02x}' for value in ethernet.mac(slot))
        for command in [
            ['ip', 'link', 'set', 'lo', 'up'],
            ['ip', 'link', 'set', name, 'address', mac],
            ['ip', 'addr', 'add', ethernet.address(slot)+'/29', 'dev', name],
            ['ip', 'link', 'set', name, 'up'],
        ]:
            subprocess.run(command, check=True, timeout=5)
        control.sendmsg([b'ready'], [(socket.SOL_SOCKET, socket.SCM_RIGHTS, array.array('i', [tap]))])
    finally:
        os.close(tap)
    checked(libc.prctl(28, 15, 0, 0, 0))  # locked NOROOT / NO_SETUID_FIXUP
    checked(libc.prctl(38, 1, 0, 0, 0))   # NO_NEW_PRIVS

    class Header(ctypes.Structure):
        _fields_ = [('version', ctypes.c_uint32), ('pid', ctypes.c_int)]

    class Capabilities(ctypes.Structure):
        _fields_ = [('effective', ctypes.c_uint32), ('permitted', ctypes.c_uint32), ('inheritable', ctypes.c_uint32)]
    header = Header(0x20080522, 0)
    caps = (Capabilities*2)()
    checked(libc.capset(ctypes.byref(header), ctypes.byref(caps)))
    checked(libc.prctl(4, 1, 0, 0, 0))  # ordinary owner may inspect Home's PID
    return control


def death_guard(parent, death_signal=signal.SIGTERM):
    def guard():
        signal.signal(signal.SIGTERM, signal.SIG_DFL)
        checked(libc.prctl(1, death_signal, 0, 0, 0))
        if os.getppid() != parent:
            os.kill(os.getpid(), death_signal)
    return guard


def stop_owned(child):
    if child is None or child.poll() is not None:
        return
    try:
        os.killpg(child.pid, signal.SIGTERM)
        child.wait(timeout=3)
    except subprocess.TimeoutExpired:
        os.killpg(child.pid, signal.SIGKILL)
        child.wait()
    except ProcessLookupError:
        pass
