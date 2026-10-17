#!/usr/bin/python3
"""Gamescope passive badge; replay outside clicks without synthesizing game input."""
import argparse
import ctypes as c
import json
import os
import select
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path
from overlay_support import X11, identity


def emit(event, **fields):
    print(json.dumps({'event': event, **fields}), flush=True)


def inside(region, x, y):
    left, top, width, height = region
    return width > 0 and height > 0 and left <= x < left + width and top <= y < top + height


class Pointer:
    def __init__(self, view):
        self.view = view
        x, d = view.x, view.display
        class Button(c.Structure):
            _fields_ = [('type', c.c_int), ('serial', c.c_ulong), ('send_event', c.c_int),
                ('display', c.c_void_p), ('window', c.c_ulong), ('root', c.c_ulong),
                ('subwindow', c.c_ulong), ('time', c.c_ulong), ('x', c.c_int), ('y', c.c_int),
                ('x_root', c.c_int), ('y_root', c.c_int), ('state', c.c_uint), ('button', c.c_uint), ('same_screen', c.c_int)]
        class Event(c.Union):
            _fields_ = [('type', c.c_int), ('button', Button), ('pad', c.c_long * 24)]
        self.Event = Event
        x.XGrabButton.argtypes = [c.c_void_p,c.c_uint,c.c_uint,c.c_ulong,c.c_int,c.c_uint,c.c_int,c.c_int,c.c_ulong,c.c_ulong]
        x.XUngrabButton.argtypes = [c.c_void_p,c.c_uint,c.c_uint,c.c_ulong]
        x.XAllowEvents.argtypes = [c.c_void_p,c.c_int,c.c_ulong]
        x.XPending.argtypes = [c.c_void_p]
        x.XNextEvent.argtypes = [c.c_void_p,c.POINTER(Event)]
        x.XSync.argtypes = [c.c_void_p,c.c_int]
        self.grabbed = False
        self.pressed = False
        self.error = 0
        handler = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_void_p)
        def on_error(*_):
            self.error = 1
            return 0
        self.handler = handler(on_error)
        x.XSetErrorHandler(self.handler)

    def grab(self, enabled):
        if enabled == self.grabbed:
            return
        x, d = self.view.x, self.view.display
        self.error = 0
        if enabled:
            # Sync pointer only. The physical controller and keyboard stay with
            # the game. ReplayPointer below reprocesses the ORIGINAL outside tap.
            x.XGrabButton(d, 1, 1 << 15, self.view.root, False, (1 << 2) | (1 << 3) | (1 << 6), 0, 1, 0, 0)
        else:
            x.XUngrabButton(d, 1, 1 << 15, self.view.root)
        x.XSync(d, False)
        if self.error:
            raise RuntimeError('Badge input unavailable')
        self.grabbed = enabled

    def poll(self, regions, eligible):
        events = []
        x, d = self.view.x, self.view.display
        if not eligible:
            self.pressed = False
        while x.XPending(d):
            event = self.Event(); x.XNextEvent(d, c.byref(event))
            px, py = event.button.x_root, event.button.y_root
            if event.type == 4:
                self.pressed = next((key for key, rect in regions.items() if eligible and inside(rect, px, py)), False)
                x.XAllowEvents(d, 0 if self.pressed else 2, event.button.time)
                x.XFlush(d)
                if self.pressed: events.append((self.pressed, 'press', px, py))
            elif event.type in (5, 6) and self.pressed:
                if eligible and self.pressed in regions:
                    events.append((self.pressed, 'release' if event.type == 5 else 'move', px, py))
                else:
                    self.pressed = False
                if event.type == 5: self.pressed = False
        return events


def watchdog(fd, parent, start):
    # A dead GUI or a stalled pointer helper must release X's passive/active
    # grabs. Only this owned helper is stopped, never the game or the shell.
    with socket.socket(fileno=fd) as pipe:
        while select.select([pipe], [], [], 2)[0] and pipe.recv(4096):
            pass
    try:
        if identity(parent) == start:
            os.kill(parent, signal.SIGKILL)
    except (OSError, ProcessLookupError):
        pass


def run(args):
    v = X11(); start = identity(args.shell)
    if not v.owns(args.window, args.shell, start):
        raise RuntimeError('Unowned badge')
    x, d = v.x, v.display
    x.XChangeProperty.argtypes = [c.c_void_p,c.c_ulong,c.c_ulong,c.c_ulong,c.c_int,c.c_int,c.c_void_p,c.c_int]
    # Gamescope retains the last external-overlay commit after XUnmapWindow.
    # Explicit opacity also hides that retained frame during menu/exit capture.
    opacity = c.c_ulong(0)
    x.XChangeProperty(d,args.window,v.atom('_NET_WM_WINDOW_OPACITY'),v.atom('CARDINAL'),32,0,c.byref(opacity),1)
    one = c.c_ulong(1)
    x.XChangeProperty(d,args.window,v.atom('GAMESCOPE_EXTERNAL_OVERLAY'),v.atom('CARDINAL'),32,0,c.byref(one),1)
    x.XFlush(d)
    parent, child = socket.socketpair()
    guard = subprocess.Popen([sys.executable,str(Path(__file__).resolve()),'--watchdog',str(child.fileno()),
        '--parent',str(os.getpid()),'--start',identity(os.getpid())],pass_fds=(child.fileno(),),
        stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,start_new_session=True)
    child.close()
    pointer = Pointer(v); regions = {}; visible = False; game = 0; game_start = ''
    buffer = b''
    try:
        emit('ready')
        while guard.poll() is None and identity(args.shell) == start:
            # Hidden surfaces have no pointer work. Commands/heartbeats wake the
            # wait immediately; visible input retains its original 10 ms cadence.
            if select.select([sys.stdin.buffer],[],[],.01 if visible else .25)[0]:
                data = os.read(sys.stdin.fileno(),4096)
                if not data:
                    return
                buffer += data
                if len(buffer)>8192:
                    raise RuntimeError('Invalid transport')
                while b'\n' in buffer:
                    line,buffer = buffer.split(b'\n',1); command = json.loads(line)
                    if command.get('command') == 'ping':
                        parent.sendall(b'K')
                    elif command.get('command') == 'surface':
                        visible = command.get('visible') is True
                        opacity = c.c_ulong(0xffffffff if visible else 0)
                        x.XChangeProperty(d,args.window,v.atom('_NET_WM_WINDOW_OPACITY'),v.atom('CARDINAL'),32,0,c.byref(opacity),1)
                        x.XFlush(d)
                        rows = command.get('regions', [])
                        if not isinstance(rows, list) or len(rows) > 16: raise RuntimeError('Invalid regions')
                        regions = {}
                        for row in rows:
                            rect = tuple(int(row.get(k,0)) for k in ('x','y','width','height'))
                            key = row.get('id')
                            if not isinstance(key,str) or not key or key in regions or any(abs(n)>32768 for n in rect):
                                raise RuntimeError('Invalid region')
                            regions[key] = rect
                        next_game = int(command.get('game',0))
                        if game != next_game:
                            game = next_game
                            try: game_start = identity(game) if game else ''
                            except OSError: game_start = ''
            eligible = False
            if visible:
                focused = v.active()
                eligible = (v.owns(focused,args.shell,start) or
                    bool(game and game_start and v.owns(focused,game,game_start)))
            pointer.grab(eligible and bool(regions))
            for target, phase, px, py in pointer.poll(regions, eligible):
                emit('pointer', target=target, phase=phase, x=px, y=py)
    finally:
        pointer.grab(False)
        parent.close()


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--window',type=int);p.add_argument('--shell',type=int)
    p.add_argument('--watchdog',type=int);p.add_argument('--parent',type=int);p.add_argument('--start')
    a = p.parse_args()
    if a.watchdog is not None:
        watchdog(a.watchdog,a.parent,a.start)
    else:
        try: run(a)
        except Exception as error:
            emit('failed', reason=type(error).__name__ + ': ' + str(error)[:160])
            sys.exit(1)
