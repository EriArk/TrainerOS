#!/usr/bin/python3
"""Opt-in Flip/Gamescope exit transport. Never signals or kills the game."""
import argparse
import concurrent.futures
import fcntl
import json
import os
from pathlib import Path
import select
import signal
import socket
import subprocess
import sys
import time
from overlay_support import RawPad, X11, identity

SERVICE = 'org.shadowblip.InputPlumber'
INTERFACE = 'org.shadowblip.Input.CompositeDevice'


class Device:
    def __init__(self, path):
        import dbus
        self.dbus = dbus
        self.bus = dbus.SystemBus()
        self.owner = self.bus.get_name_owner(SERVICE)
        self.properties = dbus.Interface(self.bus.get_object(self.owner, path), 'org.freedesktop.DBus.Properties')

    def get(self, name):
        return self.properties.Get(INTERFACE, name, timeout=1)

    def mode(self, value=None):
        if value is not None:
            self.properties.Set(INTERFACE, 'InterceptMode', self.dbus.UInt32(value), timeout=1)
        return int(self.get('InterceptMode'))


def guard_loop(channel, restore, timeout=2.0):
    """Separate process retains the lock and restores input on EOF or stalled UI."""
    channel.sendall(b'R')
    try:
        while select.select([channel], [], [], timeout)[0]:
            packet = channel.recv(4096)
            if not packet: break
    finally:
        restore()


def restore_input(device, pad, x11, shell, start):
    x11.hide_prompt(shell, start)
    neutral_since = None
    while True:
        try:
            mode = device.mode()
            if mode not in (1, 2): break
            if mode == 1: device.mode(0); break
            neutral = pad.sample()['neutral']
            neutral_since = (neutral_since or time.monotonic()) if neutral else None
            if neutral_since and time.monotonic() - neutral_since >= .08:
                device.mode(0); break
        except OSError:  # Disconnected physical source cannot leak held input.
            try: device.mode(0)
            except Exception: pass
            break
        except Exception:  # Service owner disappeared: do not change a new owner.
            break
        time.sleep(.02)


def watchdog(args):
    # Independent D-Bus connection and raw FD; no dependency on the parent loop.
    device, pad, x11 = Device(args.device), RawPad(args.source), X11()
    channel = socket.socket(fileno=args.watchdog)
    guard_loop(channel, lambda: restore_input(device, pad, x11, args.shell, args.shell_start))


def emit(kind, **values):
    print(json.dumps({'event': kind, **values}, separators=(',', ':')), flush=True)


def capture(directory):
    candidates = []
    for p in Path('/proc').glob('[0-9]*'):
        try:
            if p.stat().st_uid == os.getuid() and (p/'comm').read_text().strip() == 'gamescope-wl': candidates.append(int(p.name))
        except OSError: pass
    if len(candidates) != 1: raise RuntimeError('Compositor unavailable')
    old = {p: p.stat().st_mtime_ns for p in Path('/tmp').glob('gamescope_*.avif') if not p.is_symlink()}
    os.kill(candidates[0], signal.SIGUSR2)
    deadline = time.monotonic() + 1.3
    while time.monotonic() < deadline:
        for p in Path('/tmp').glob('gamescope_*.avif'):
            if p.is_symlink() or p.stat().st_uid != os.getuid(): continue
            if p.stat().st_mtime_ns == old.get(p): continue
            time.sleep(.12)
            if p.stat().st_size > 32 * 1024 * 1024: raise RuntimeError('Capture too large')
            source = directory/'capture.avif'; source.write_bytes(p.read_bytes())
            result = directory/'frame.png'
            subprocess.run(['/usr/bin/ffmpeg','-v','error','-y','-threads','1','-c:v','libdav1d','-i',str(source),
                            '-frames:v','1','-threads','1',str(result)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=4)
            return
        time.sleep(.025)
    raise RuntimeError('Capture unavailable')


def run(args):
    directory = Path(args.directory)
    if not directory.is_absolute() or directory.stat().st_uid != os.getuid() or directory.stat().st_mode & 0o077:
        raise RuntimeError('Private capture directory required')
    device = Device(args.device)
    if str(device.get('Name')) != 'Retroid Pocket Flip 2' or device.mode() != 0:
        raise RuntimeError('Device already intercepted or unsupported')
    sources = list(device.get('SourceDevicePaths'))
    if len(sources) != 1: raise RuntimeError('Unverified composite sources')
    args.source = str(sources[0])
    pad, x11 = RawPad(args.source), X11()
    game_start, args.shell_start = identity(args.game), identity(args.shell)
    lock = os.open(Path(os.environ['XDG_RUNTIME_DIR'])/'traineros-input-lease.lock', os.O_CREAT | os.O_RDWR | os.O_CLOEXEC, 0o600)
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    parent, child = socket.socketpair()
    guard = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--watchdog',str(child.fileno()),
        '--device',args.device,'--source',args.source,'--shell',str(args.shell),'--shell-start',args.shell_start],
        pass_fds=(child.fileno(),lock),start_new_session=True,stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL)
    child.close()
    if not select.select([parent],[],[],2)[0] or parent.recv(1) != b'R':
        raise RuntimeError('Input watchdog unavailable')
    pool = concurrent.futures.ThreadPoolExecutor(max_workers=1)
    future = None; state = 'game'; target = 0; token = 0; epoch = '0'; buffer = b''; neutral_since = None
    try:
        device.mode(1); emit('ready')
        while guard.poll() is None:
            if identity(args.game) != game_start: break
            if select.select([sys.stdin.buffer],[],[],.02)[0]:
                data = os.read(sys.stdin.fileno(),4096)
                if not data: break
                buffer += data
                if len(buffer) > 16384: raise RuntimeError('Invalid transport')
                while b'\n' in buffer:
                    line,buffer = buffer.split(b'\n',1); command = json.loads(line)
                    op = command.get('command')
                    if op == 'ping': parent.sendall(b'K')
                    elif op == 'context': epoch = str(command['epoch'])
                    elif op == 'capture' and state == 'requested':
                        token = int(command['token']); state = 'overlay'
                        future = pool.submit(capture, directory)
                    elif op == 'cancel' and state in ('requested','overlay'):
                        state = 'release'; neutral_since = None
                    elif op == 'close' and state == 'overlay' and int(command['token']) == token:
                        if x11.close(target,args.game,game_start): state = 'closing'
                        else: emit('close-failed',token=token); state = 'release'
                    elif op == 'stop': return
            sample = pad.sample()
            mode = device.mode()
            if mode not in (1,2): raise RuntimeError('Input lease changed')
            if state == 'game' and mode == 2:
                target = x11.active()
                # Enable adapters one at a time. Other emulators may open their
                # own close confirmation, which this overlay must not obscure.
                supported = False
                if target and x11.owns(target,args.game,game_start):
                    try:
                        supported = (Path('/proc')/str(x11.pid(target))/'comm').read_text().strip().lower() == 'retroarch'
                    except OSError: pass
                if supported and x11.atom('WM_DELETE_WINDOW') in x11.prop(target,'WM_PROTOCOLS'):
                    state = 'requested'; emit('request')
                else: state = 'release'; neutral_since = None
            if future is not None and future.done():
                error = future.exception()
                if state == 'overlay': emit('captured',token=token,ok=error is None)
                future = None
            if state in ('requested','overlay','closing'):
                emit('input',epoch=epoch,**sample)
            if state == 'release':
                neutral_since = (neutral_since or time.monotonic()) if sample['neutral'] else None
                # Do not start another capture while an old worker can still write.
                if neutral_since and time.monotonic()-neutral_since >= .08 and future is None:
                    device.mode(1); state = 'game'; emit('released')
    finally:
        parent.close()  # Watchdog alone restores mode zero, even after SIGKILL.
        if guard.poll() is not None:
            # A killed watchdog cannot restore anything. The surviving helper
            # retains the same lock, service owner and physical release gate.
            restore_input(device, pad, x11, args.shell, args.shell_start)
        pool.shutdown(wait=False, cancel_futures=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--device',default='/org/shadowblip/InputPlumber/CompositeDevice0')
    parser.add_argument('--game',type=int); parser.add_argument('--shell',type=int,required=True)
    parser.add_argument('--directory'); parser.add_argument('--watchdog',type=int)
    parser.add_argument('--source'); parser.add_argument('--shell-start')
    args = parser.parse_args()
    try:
        if args.watchdog is not None: watchdog(args)
        else: run(args)
    except Exception:
        if args.watchdog is None:
            try: emit('failed')
            except BrokenPipeError: pass
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
