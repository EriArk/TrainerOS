"""Bounded InputPlumber Home lease for ordinary applications.

Reuses the verified physical reader and device API. A separate watchdog owns
restoration, including when the application service or shell is killed.
"""
import fcntl
import importlib.util
import os
from pathlib import Path
import select
import socket
import subprocess
import sys
import time
from overlay_support import RawPad

spec = importlib.util.spec_from_file_location('trainer_overlay', Path(__file__).with_name('adventure-overlay.py'))
overlay = importlib.util.module_from_spec(spec)
spec.loader.exec_module(overlay)
Device = overlay.Device


def restore(device, pad):
    while True:
        try:
            if device.mode() not in (1, 2): return
            if pad.sample()['neutral']:
                device.mode(0); return
        except Exception: break
        time.sleep(.02)
    try: device.mode(0)
    except Exception: pass


class Lease:
    def __init__(self):
        path = '/org/shadowblip/InputPlumber/CompositeDevice0'
        self.device = Device(path)
        if self.device.mode() != 0: raise RuntimeError('Another activity is using the controller.')
        sources = [str(p) for p in self.device.get('SourceDevicePaths')
                   if (Path('/sys/class/input')/Path(str(p)).name/'device/name').read_text().strip()
                   in ('Retroid Pocket Gamepad', 'AYN Odin2 Gamepad')]
        if len(sources) != 1: raise RuntimeError('Application Home controls are not configured for this controller.')
        self.pad = RawPad(sources[0])
        self.lock = os.open(Path(os.environ['XDG_RUNTIME_DIR'])/'traineros-input-lease.lock',
                            os.O_CREAT | os.O_RDWR | os.O_CLOEXEC, 0o600)
        try:
            fcntl.flock(self.lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.channel, child = socket.socketpair()
            self.guard = subprocess.Popen([sys.executable, __file__, str(child.fileno()), path, sources[0]],
                pass_fds=(child.fileno(), self.lock), start_new_session=True,
                stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            child.close()
            if not select.select([self.channel], [], [], 2)[0] or self.channel.recv(1) != b'R':
                raise RuntimeError('Application input watchdog did not start.')
            self.device.mode(1)
        except Exception:
            os.close(self.lock);os.close(self.pad.fd);raise
        self.menu = False;self.armed = False;self.held = '';self.last = 0
        self.resume_pending = False

    def ping(self): self.channel.sendall(b'K')

    def open(self):
        self.device.mode(2);self.menu = True;self.armed = False;self.held = ''

    def resume(self):
        self.menu = False;self.resume_pending = True

    def poll(self, emit):
        if self.guard.poll() is not None: raise RuntimeError('Application input watchdog stopped.')
        sample = self.pad.sample()
        if self.resume_pending:
            if sample['neutral']:
                self.device.mode(1);self.resume_pending = False
            return
        if not self.menu and self.device.mode() == 2:
            self.open();emit('controls')
        if not self.menu: return
        if not self.armed:
            if sample['neutral']: self.armed = True
            return
        key = next((k for k in ('confirm','back','home','left','right','up','down') if sample[k]), '')
        if key and (key != self.held or (key in ('left','right','up','down') and time.monotonic()-self.last > .35)):
            self.last = time.monotonic();emit('action', action=key)
        self.held = key

    def close(self):
        self.channel.close()
        try: self.guard.wait(timeout=1)
        except subprocess.TimeoutExpired: pass # The independent guard keeps its lock until physical release.
        os.close(self.lock);os.close(self.pad.fd)


if __name__ == '__main__':
    channel = socket.socket(fileno=int(sys.argv[1]))
    device, pad = Device(sys.argv[2]), RawPad(sys.argv[3])
    overlay.guard_loop(channel, lambda: restore(device, pad))
