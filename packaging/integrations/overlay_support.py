"""Read-only Flip controller state and ownership-checked X11 operations."""
import ctypes as c
import fcntl
import os
import signal
from pathlib import Path
import struct
import json


class RawPad:
    def __init__(self, path):
        self.fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC)
        name = bytes(self.get(0x06, 128)).split(b'\0')[0]
        self.name = name.decode('utf-8', errors='replace')
        self.shell_mapping = {}
        self.mapping_stamp = None
        profiles = {b'Retroid Pocket Gamepad': ({0,1,2,3,4,5,20,21}, {20,21}),
                    b'AYN Odin2 Gamepad': ({0,1,2,3,4,5}, {2,5})}
        if name not in profiles or struct.unpack('HHHH', self.get(0x02, 8))[:3] != (3, 0x2020, 0x3001):
            raise RuntimeError('Unverified physical controller')
        expected, self.triggers = profiles[name]
        bits = self.get(0x23, 8)
        self.axes = [i for i in range(64) if bits[i // 8] & (1 << (i % 8))]
        if set(self.axes) != expected:
            raise RuntimeError('Unverified controller axes')

    def get(self, number, size):
        data = bytearray(size)
        fcntl.ioctl(self.fd, 0x80000000 | (size << 16) | (ord('E') << 8) | number, data, True)
        return data

    def sample(self):
        mapping_path = Path(os.environ.get('XDG_RUNTIME_DIR', '/nonexistent'))/'traineros-shell-input.json'
        try:
            stamp = mapping_path.stat().st_mtime_ns
            if stamp != self.mapping_stamp:
                self.mapping_stamp = stamp
                self.shell_mapping = {}
                if mapping_path.stat().st_size <= 4096:
                    candidate = json.loads(mapping_path.read_text())
                    if isinstance(candidate, dict) and candidate.get('version') == 1 and candidate.get('device') == self.name:
                        self.shell_mapping = candidate
        except (OSError, ValueError):
            self.shell_mapping = {}
        bits = self.get(0x18, 96)
        keys = {i for i in range(768) if bits[i // 8] & (1 << (i % 8))}
        neutral = not keys
        positions = {}
        for axis in self.axes:
            value, low, high, _, _, _ = struct.unpack('iiiiii', self.get(0x40 + axis, 24))
            if high <= low:
                raise RuntimeError('Invalid axis range')
            position = (value - low) / (high - low)
            positions[axis] = position
            neutral &= position < .15 if axis in self.triggers else abs(position - .5) < .175
        buttons = [name for code, name in ((304,'south'),(305,'east'),(308,'west'),(307,'north'),
            (310,'leftShoulder'),(311,'rightShoulder'),(317,'leftStick'),(318,'rightStick'),
            (314,'select'),(315,'start'),(544,'up'),(545,'down'),(546,'left'),(547,'right')) if code in keys]
        if positions[min(self.triggers)] > .55: buttons.append('leftTrigger')
        if positions[max(self.triggers)] > .55: buttons.append('rightTrigger')
        right = (2,3) if 20 in self.triggers else (3,4)
        confirm, back = (304,305) if self.shell_mapping.get('southConfirms') is True else (305,304)
        lx = 1-positions[0] if self.shell_mapping.get('invertX') is True else positions[0]
        ly = 1-positions[1] if self.shell_mapping.get('invertY') is True else positions[1]
        return {'buttons': buttons, 'sticks': [positions[a]*2-1 for a in (0,1,*right)],
                'connected': True, 'neutral': bool(neutral), 'confirm': confirm in keys,
                'back': back in keys, 'home': 316 in keys,
                'up': 544 in keys or ly < .225,
                'down': 545 in keys or ly > .775,
                'left': 546 in keys or lx < .225,
                'right': 547 in keys or lx > .775,
                'start': 315 in keys, 'select': 314 in keys,
                'secondary': 307 in keys, 'recent': 308 in keys,
                'previousPage': 310 in keys, 'nextPage': 311 in keys,
                'previousFace': positions[min(self.triggers)] > .75,
                'nextFace': positions[max(self.triggers)] > .75}


def identity(pid):
    return (Path('/proc') / str(pid) / 'stat').read_text().rsplit(')', 1)[1].split()[19]


def supported_emulator_process(pid, proc=Path('/proc')):
    # Some cores rename the main thread (PPSSPP uses "Main"). Ownership is
    # checked by X11.owns separately; comm is not executable identity.
    try:
        executable = (proc / str(pid) / 'exe').readlink().name
        # ARM AppImage userland execution can retain the package's /proc/exe.
        # The managed flags and XRes process-tree ownership are still required.
        executable = {'rpcs3.AppImage': 'rpcs3', 'eden.AppImage': 'eden', 'xemu.AppImage': 'xemu'}.get(executable, executable)
        if executable == 'traineros-webgame':
            env = (proc / str(pid) / 'environ').read_bytes().split(b'\0')
            return b'TRAINEROS_CRATES_GAME=1' in env
        if executable in ('Cemu', 'cemu', 'duckstation-qt', 'rpcs3', 'eden', 'xemu'):
            # The surrounding X11 ownership check still requires this launch's
            # exact process tree. Plain desktop emulator windows are excluded.
            args = (proc / str(pid) / 'cmdline').read_bytes().split(b'\0')
            if executable == 'duckstation-qt':
                return b'-batch' in args and b'-fullscreen' in args
            if executable == 'rpcs3':
                return b'--no-gui' in args and b'--fullscreen' in args
            if executable == 'xemu':
                return b'-full-screen' in args and b'-dvd_path' in args
            return b'-f' in args and b'-g' in args
        if executable == 'dolphin-emu':
            # Only our batch launch suppresses Dolphin's second desktop prompt.
            args = (proc / str(pid) / 'cmdline').read_bytes().split(b'\0')
            return b'-b' in args and any(args[i:i+2] == [b'-C', b'Dolphin.Interface.ConfirmStop=False']
                                         for i in range(len(args) - 1))
        if executable in ('RMG', 'flycast', 'flycast-dojo'):
            # Only the owned native sessions have verified window-close behavior.
            # Ordinary desktop launches keep their existing handling.
            env = (proc / str(pid) / 'environ').read_bytes().split(b'\0')
            marker = b'TRAINEROS_RMG_SESSION=' if executable == 'RMG' else b'TRAINEROS_FLYCAST_SESSION='
            return any(value.startswith(marker) and len(value) > len(marker) for value in env)
        if executable == 'stream_guest':
            env = (proc / str(pid) / 'environ').read_bytes().split(b'\0')
            return b'TRAINEROS_REMOTE_PLAY_MANAGED=1' in env
        if executable == 'Vita3K':
            env = (proc / str(pid) / 'environ').read_bytes().split(b'\0')
            return b'TRAINEROS_VITA_MANAGED=1' in env
        if executable == 'azahar':
            env = (proc / str(pid) / 'environ').read_bytes().split(b'\0')
            return b'TRAINEROS_AZAHAR_MANAGED=1' in env or any(
                value.startswith(b'TRAINEROS_AZAHAR_SESSION=') and
                len(value) > len(b'TRAINEROS_AZAHAR_SESSION=') for value in env)
        # The packaged DS frontend handles WM_DELETE_WINDOW by agreeing a
        # complete-frame return with its peer; SIGTERM deliberately cancels.
        if executable in ('retroarch', 'PPSSPPSDL', 'armsx2-qt', 'melonDS', 'traineros-ds'):
            return True
        # Native Crates and ordinary PC games have arbitrary executable names. The caller also
        # requires this launch's exact XRes process tree and WM_DELETE_WINDOW.
        env = (proc / str(pid) / 'environ').read_bytes().split(b'\0')
        return b'TRAINEROS_PC_MANAGED=1' in env or b'TRAINEROS_CRATES_GAME=1' in env
    except OSError:
        return False


class X11:
    def __init__(self):
        self.graceful_signals = set()
        self.graceful_closes = set()
        self.x = x = c.CDLL('libX11.so.6')
        self.r = r = c.CDLL('libXRes.so.1')
        x.XOpenDisplay.argtypes = [c.c_char_p]; x.XOpenDisplay.restype = c.c_void_p
        self.display = d = x.XOpenDisplay(None)
        if not d:
            raise RuntimeError('X11 unavailable')
        handler = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_void_p)
        self.error_handler = handler(lambda *_: 0)
        x.XSetErrorHandler.argtypes = [handler]; x.XSetErrorHandler(self.error_handler)
        x.XDefaultRootWindow.argtypes = [c.c_void_p]; x.XDefaultRootWindow.restype = c.c_ulong
        self.root = x.XDefaultRootWindow(d)
        x.XInternAtom.argtypes = [c.c_void_p, c.c_char_p, c.c_int]; x.XInternAtom.restype = c.c_ulong
        x.XGetWindowProperty.argtypes = [c.c_void_p,c.c_ulong,c.c_ulong,c.c_long,c.c_long,c.c_int,c.c_ulong,c.POINTER(c.c_ulong),c.POINTER(c.c_int),c.POINTER(c.c_ulong),c.POINTER(c.c_ulong),c.POINTER(c.c_void_p)]
        x.XFree.argtypes = [c.c_void_p]; x.XFlush.argtypes = [c.c_void_p]
        x.XUnmapWindow.argtypes = [c.c_void_p, c.c_ulong]
        x.XQueryTree.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(c.c_ulong),
                                c.POINTER(c.c_ulong), c.POINTER(c.POINTER(c.c_ulong)), c.POINTER(c.c_uint)]
        class Spec(c.Structure):
            _fields_ = [('client', c.c_ulong), ('mask', c.c_uint)]
        class Value(c.Structure):
            _fields_ = [('spec', Spec), ('length', c.c_long), ('value', c.c_void_p)]
        self.Spec, self.Value = Spec, Value
        r.XResQueryVersion.argtypes = [c.c_void_p, c.POINTER(c.c_int), c.POINTER(c.c_int)]
        major, minor = c.c_int(), c.c_int()
        if not r.XResQueryVersion(d, c.byref(major), c.byref(minor)) or (major.value, minor.value) < (1, 2):
            raise RuntimeError('XRes ownership unavailable')
        r.XResQueryClientIds.argtypes = [c.c_void_p,c.c_long,c.POINTER(Spec),c.POINTER(c.c_long),c.POINTER(c.POINTER(Value))]
        r.XResGetClientPid.argtypes = [c.POINTER(Value)]; r.XResGetClientPid.restype = c.c_int
        r.XResClientIdsDestroy.argtypes = [c.c_long, c.POINTER(Value)]

    def atom(self, name):
        return self.x.XInternAtom(self.display, name.encode(), False)

    def prop(self, window, name):
        actual, fmt, count, left, data = c.c_ulong(), c.c_int(), c.c_ulong(), c.c_ulong(), c.c_void_p()
        status = self.x.XGetWindowProperty(self.display, window, self.atom(name), 0, 4096, False, 0,
            c.byref(actual), c.byref(fmt), c.byref(count), c.byref(left), c.byref(data))
        try:
            if status or not data or left.value:
                return []
            if fmt.value == 32:
                return list(c.cast(data, c.POINTER(c.c_ulong))[:count.value])
            return c.string_at(data, count.value) if fmt.value == 8 else []
        finally:
            if data: self.x.XFree(data)

    def pid(self, window):
        spec, count, values = self.Spec(window, 2), c.c_long(), c.POINTER(self.Value)()
        if self.r.XResQueryClientIds(self.display, 1, c.byref(spec), c.byref(count), c.byref(values)) != 0:
            return 0
        try:
            return next((p for i in range(count.value) if (p := self.r.XResGetClientPid(c.byref(values[i]))) > 0), 0)
        finally:
            self.r.XResClientIdsDestroy(count, values)

    def active(self):
        return next(iter(self.prop(self.root, '_NET_ACTIVE_WINDOW')), 0)

    def owns(self, window, root_pid, start):
        try:
            if identity(root_pid) != start: return False
            pid = self.pid(window)
            for _ in range(32):
                if pid == root_pid: return True
                if pid <= 1: return False
                pid = int((Path('/proc') / str(pid) / 'stat').read_text().rsplit(')', 1)[1].split()[1])
        except (OSError, ValueError, IndexError):
            pass
        return False

    def hide_prompt(self, shell_pid, shell_start, all_windows=False):
        try:
            if identity(shell_pid) != shell_start: return
        except OSError:
            return
        root, parent, children, count = c.c_ulong(), c.c_ulong(), c.POINTER(c.c_ulong)(), c.c_uint()
        windows = set(self.prop(self.root, '_NET_CLIENT_LIST'))
        if self.x.XQueryTree(self.display, self.root, c.byref(root), c.byref(parent), c.byref(children), c.byref(count)):
            try: windows.update(children[:count.value])
            finally:
                if children: self.x.XFree(children)
        for window in windows:
            if self.pid(window) == shell_pid and (all_windows or self.prop(window, '_NET_WM_NAME') == 'TrainerOS — Return from Adventure'.encode()):
                self.x.XUnmapWindow(self.display, window)
        self.x.XFlush(self.display)

    def close(self, window, root_pid, start):
        if not self.owns(window, root_pid, start) or self.atom('WM_DELETE_WINDOW') not in self.prop(window, 'WM_PROTOCOLS'):
            return False
        pid = self.pid(window)
        try:
            token = (pid, identity(pid))
            dolphin = (Path('/proc') / str(pid) / 'exe').readlink().name == 'dolphin-emu'
            if dolphin and (token in self.graceful_closes or not supported_emulator_process(pid)):
                return False
            if (Path('/proc') / str(pid) / 'exe').readlink().name == 'armsx2-qt':
                # ARMSX2's first SIGTERM requests its normal Qt shutdown with
                # confirmation disabled. WM_DELETE asks a second desktop modal.
                # Never repeat: its second signal is a forced exit. Keep the
                # upstream memory-card-busy guard and existing emulator config.
                token = (pid, identity(pid))
                if token in self.graceful_signals:
                    return False
                fd = os.pidfd_open(pid)
                try:
                    if identity(pid) != token[1] or not self.owns(window, root_pid, start):
                        return False
                    signal.pidfd_send_signal(fd, signal.SIGTERM)
                    self.graceful_signals.add(token)
                    return True
                finally:
                    os.close(fd)
        except OSError:
            return False
        class Data(c.Union):
            _fields_ = [('b', c.c_char * 20), ('s', c.c_short * 10), ('l', c.c_long * 5)]
        class Message(c.Structure):
            _fields_ = [('type',c.c_int),('serial',c.c_ulong),('send_event',c.c_int),('display',c.c_void_p),('window',c.c_ulong),('message_type',c.c_ulong),('format',c.c_int),('data',Data)]
        class Event(c.Union):
            _fields_ = [('client', Message), ('pad', c.c_long * 24)]
        event = Event(); event.client = Message(33,0,True,self.display,window,self.atom('WM_PROTOCOLS'),32,Data())
        event.client.data.l[0] = self.atom('WM_DELETE_WINDOW')
        self.x.XSendEvent.argtypes = [c.c_void_p,c.c_ulong,c.c_int,c.c_long,c.POINTER(Event)]
        sent = self.x.XSendEvent(self.display, window, False, 0, c.byref(event))
        self.x.XFlush(self.display)
        if sent and dolphin:
            # A second stop request can force a Wii shutdown. Never escalate.
            self.graceful_closes.add(token)
        return bool(sent)
