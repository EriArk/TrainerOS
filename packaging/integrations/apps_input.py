"""Bounded InputPlumber Home lease for ordinary applications.

Reuses the verified physical reader and device API. A separate watchdog owns
restoration, including when the application service or shell is killed.
"""
import fcntl
import json
import importlib.util
import os
from pathlib import Path
import select
import socket
import subprocess
import sys
import time
from overlay_support import RawPad, X11, identity
from game_input import Mapper, HeldOutput, XTestSink, mapped_guard
from keyboard_input import HomeChord, KeyboardNavigation

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


def installer_profile():
    return dict(version=1, mode='keyboard_mouse', leftStick='arrows', rightStick='mouse',
                deadzone=18, sensitivity=650, modifier='none', rings=[], shiftBindings={},
                bindings=dict(south='MouseLeft', east='MouseRight', west='Backspace', north='Tab',
                              start='Enter', select='Escape', leftShoulder='WheelUp', rightShoulder='WheelDown',
                              up='Up', down='Down', left='Left', right='Right'))


class Lease:
    def __init__(self, installer=None):
        self.installer = installer
        self.mapper = Mapper(installer_profile()) if installer else None
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
            self.guard = subprocess.Popen([sys.executable, __file__, str(child.fileno()), path, sources[0]] +
                ([str(installer[0]), installer[1]] if installer else []),
                pass_fds=(child.fileno(), self.lock), start_new_session=True,
                stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            child.close()
            if not select.select([self.channel], [], [], 2)[0] or self.channel.recv(1) != b'R':
                raise RuntimeError('Application input watchdog did not start.')
            self.device.mode(2 if self.mapper else 1)
        except Exception:
            os.close(self.lock);os.close(self.pad.fd);raise
        self.menu = False;self.armed = False;self.held = '';self.last = 0
        self.resume_pending = False
        self.chord = HomeChord();self.keyboard = False;self.navigation = KeyboardNavigation()

    def ping(self): self.channel.sendall(b'K\n' if self.mapper else b'K')

    def open(self):
        self.device.mode(2);self.menu = True;self.keyboard = False;self.armed = False;self.held = ''

    def resume(self):
        self.menu = False;self.keyboard = False;self.resume_pending = True

    def poll(self, emit):
        if self.guard.poll() is not None: raise RuntimeError('Application input watchdog stopped.')
        sample = self.pad.sample()
        event = self.chord.update(sample['home'],sample.get('keyboardX',False),sample['neutral'])
        if self.resume_pending:
            if sample['neutral']:
                self.device.mode(2 if self.mapper else 1);self.resume_pending = False
            if self.mapper:self.channel.sendall(b'{"held":[],"motion":[0,0]}\n')
            return
        if event == 'keyboard':
            if self.keyboard:
                self.resume();emit('keyboard-closed');return
            self.open();self.keyboard = True;self.navigation = KeyboardNavigation();emit('keyboard')
        elif event == 'home' and self.keyboard:
            self.open();emit('keyboard-closed');emit('controls')
        if not self.menu and (event == 'home' or (not self.mapper and self.device.mode()==2 and sample['neutral'] and not self.chord.held)):
            self.open();emit('controls')
        if self.mapper:
            packet=self.mapper.frame(sample,not self.menu and not sample['home'] and not self.chord.blocked,time.monotonic())
            self.channel.sendall(json.dumps(packet).encode()+b'\n')
        if self.keyboard:
            if not sample['home'] and not self.chord.blocked:
                action=self.navigation.action(sample)
                if action:emit('keyboard-action',action=action)
            return
        if sample['home'] or self.chord.blocked:return
        sample=dict(sample,home=event=='home')
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
    if len(sys.argv)==6:
        x11=X11();pid=int(sys.argv[4]);start=sys.argv[5]
        mapped_guard(channel,HeldOutput(XTestSink(x11)),lambda:x11.owns(x11.active(),pid,start),lambda:restore(device,pad))
    else:
        overlay.guard_loop(channel, lambda: restore(device, pad))
