"""Trainer/game keyboard-mouse profiles. The watchdog, not the UI, owns held keys."""
import ctypes as c
import json
import math
import select
import string
import time

BUTTONS = ('south', 'east', 'west', 'north', 'leftShoulder', 'rightShoulder',
           'leftTrigger', 'rightTrigger', 'leftStick', 'rightStick', 'select', 'start',
           'up', 'down', 'left', 'right')
KEYS = set(string.ascii_uppercase + string.digits) | {
    'Space', 'Enter', 'Escape', 'Tab', 'Backspace', 'Shift', 'Ctrl', 'Alt',
    'Up', 'Down', 'Left', 'Right'} | {f'F{i}' for i in range(1, 13)}
TARGETS = KEYS | {'None', 'MouseLeft', 'MouseRight', 'MouseMiddle', 'WheelUp',
                 'WheelDown', 'Ctrl+S', 'Ctrl+L', 'Alt+Enter'}


def validate(p):
    if not isinstance(p, dict) or p.get('version') != 1 or p.get('mode') not in ('native', 'keyboard_mouse'):
        raise ValueError('Unsupported game input profile')
    if p.get('leftStick') not in ('wasd', 'arrows', 'off') or p.get('rightStick') not in ('mouse', 'scroll', 'off'):
        raise ValueError('Unsupported stick mode')
    for key, low, high in (('deadzone', 5, 40), ('sensitivity', 100, 1600)):
        value = p.get(key)
        if type(value) not in (int, float) or not math.isfinite(value) or not low <= value <= high:
            raise ValueError('Invalid stick calibration')
    if p.get('modifier') not in ('none', 'leftShoulder', 'rightShoulder'):
        raise ValueError('Invalid modifier')
    rings = p.get('rings')
    if not isinstance(rings, list) or len(rings) > 4:
        raise ValueError('Invalid rings')
    for ring in rings:
        if not isinstance(ring, list) or not 2 <= len(ring) <= 8 or any(not isinstance(x, str) or x not in TARGETS for x in ring):
            raise ValueError('Invalid ring slot')
    for key in ('bindings', 'shiftBindings'):
        bindings = p.get(key)
        if not isinstance(bindings, dict) or any(k not in BUTTONS or not isinstance(v, str) or v not in TARGETS | {f'Ring {i}' for i in range(1, 5)} for k, v in bindings.items()):
            raise ValueError('Invalid button mapping')
    return p


def keys(target):
    return set() if target == 'None' or target.startswith('Ring ') else set(target.split('+'))


class Mapper:
    def __init__(self, profile):
        self.p = validate(profile)
        self.armed = False
        self.ring = None
        self.selected = -1
        self.last = None
        self.scroll_at = 0
        self.touch_target = None
        self.blocked_ring_button = None

    def select_slot(self, index):
        if self.ring is None or type(index) is not int:
            return
        button, ring = self.ring
        if not 0 <= index < len(self.p['rings'][ring]):
            return
        self.touch_target = self.p['rings'][ring][index]
        self.blocked_ring_button = button
        self.ring = None

    def frame(self, sample, active, now):
        result = {'held': [], 'motion': [0, 0], 'radial': {}}
        dt = min(.05, max(0, now - self.last)) if self.last is not None else 0
        self.last = now
        if not active or not sample.get('connected') or sample.get('home'):
            self.armed = False; self.ring = None; self.selected = -1
            self.touch_target = None
            return result
        if not self.armed:
            self.armed = bool(sample.get('neutral'))
            return result
        pressed = set(sample.get('buttons', [])) & set(BUTTONS)
        if self.blocked_ring_button not in pressed:
            self.blocked_ring_button = None
        pressed.discard(self.blocked_ring_button)
        if self.touch_target is not None:
            result['held'] = sorted(keys(self.touch_target))
            self.touch_target = None
            return result
        modifier = self.p['modifier']
        bindings = self.p['shiftBindings'] if modifier in pressed else self.p['bindings']
        pressed.discard(modifier)
        axes = sample.get('sticks', [0, 0, 0, 0])
        dz = self.p['deadzone'] / 100
        def axis(v):
            return 0 if abs(v) <= dz else math.copysign((min(1, abs(v))-dz)/(1-dz), v)
        lx, ly, rx, ry = map(axis, axes)
        held = set()
        if self.ring is None:
            for button in BUTTONS:
                target = bindings.get(button, 'None')
                if button in pressed and target.startswith('Ring '):
                    index = int(target[-1])-1
                    if index < len(self.p['rings']):
                        self.ring = (button, index); self.selected = -1
                        break
        if self.ring is not None:
            button, index = self.ring
            slots = self.p['rings'][index]
            if math.hypot(lx, ly) > .3:
                angle = (math.atan2(lx, -ly)+2*math.pi) % (2*math.pi)
                self.selected = int(angle/(2*math.pi)*len(slots)+.5) % len(slots)
            else:
                self.selected = -1
            if button in pressed:
                result['radial'] = {'ring': index, 'slots': slots, 'selected': self.selected}
                return result
            if self.selected >= 0:
                held.update(keys(slots[self.selected]))
            self.ring = None; self.selected = -1
            # Suppress the release frame's normal stick/button actions.
            result['held'] = sorted(held)
            return result
        for button in pressed:
            held.update(keys(bindings.get(button, 'None')))
        if self.p['leftStick'] != 'off':
            up, down, left, right = ('W', 'S', 'A', 'D') if self.p['leftStick'] == 'wasd' else ('Up', 'Down', 'Left', 'Right')
            if lx < -.3: held.add(left)
            if lx > .3: held.add(right)
            if ly < -.3: held.add(up)
            if ly > .3: held.add(down)
        if self.p['rightStick'] == 'mouse':
            result['motion'] = [rx*self.p['sensitivity']*dt, ry*self.p['sensitivity']*dt]
        elif self.p['rightStick'] == 'scroll' and abs(ry) > .3 and now >= self.scroll_at:
            held.add('WheelDown' if ry > 0 else 'WheelUp'); self.scroll_at = now+.12
        result['held'] = sorted(held)
        return result


class HeldOutput:
    """Set semantics preserve a key shared by several physical bindings."""
    def __init__(self, sink):
        self.sink = sink
        self.held = set()

    def apply(self, packet):
        wanted = set(packet.get('held', []))
        if not wanted <= KEYS | {'MouseLeft', 'MouseRight', 'MouseMiddle', 'WheelUp', 'WheelDown'}:
            raise ValueError('Invalid output')
        # Modifiers down first, up last, so chords are deterministic.
        modifiers = {'Shift', 'Ctrl', 'Alt'}
        for key in sorted(self.held-wanted, key=lambda k: k in modifiers):
            self.sink.button(key, False); self.held.remove(key)
        for key in sorted(wanted-self.held, key=lambda k: k not in modifiers):
            self.held.add(key)  # Retain it for cleanup even if the backend fails.
            self.sink.button(key, True)
        motion = packet.get('motion', [0, 0])
        if len(motion) != 2 or any(type(v) not in (int, float) or not math.isfinite(v) or abs(v)>80 for v in motion):
            raise ValueError('Invalid pointer motion')
        self.sink.motion(*motion)

    def release(self):
        for key in sorted(self.held, key=lambda k: k in {'Shift', 'Ctrl', 'Alt'}):
            try: self.sink.button(key, False)
            except Exception: pass
        self.held.clear()


class XTestSink:
    """Existing X11/Xwayland game route; never focuses a window to inject input."""
    def __init__(self, x11):
        self.x11 = x11
        self.fraction = [0., 0.]
        self.xt = c.CDLL('libXtst.so.6')
        self.xt.XTestFakeKeyEvent.argtypes = [c.c_void_p, c.c_uint, c.c_int, c.c_ulong]
        self.xt.XTestFakeButtonEvent.argtypes = [c.c_void_p, c.c_uint, c.c_int, c.c_ulong]
        self.xt.XTestFakeRelativeMotionEvent.argtypes = [c.c_void_p, c.c_int, c.c_int, c.c_ulong]
        x11.x.XStringToKeysym.argtypes = [c.c_char_p]; x11.x.XStringToKeysym.restype = c.c_ulong
        x11.x.XKeysymToKeycode.argtypes = [c.c_void_p, c.c_ulong]; x11.x.XKeysymToKeycode.restype = c.c_uint
        self.codes = {}
        names = {'Enter':'Return', 'Escape':'Escape', 'Backspace':'BackSpace', 'Space':'space',
                 'Ctrl':'Control_L', 'Shift':'Shift_L', 'Alt':'Alt_L'}
        for key in KEYS:
            name = names.get(key, key.lower() if len(key)==1 and key.isalpha() else key)
            code = x11.x.XKeysymToKeycode(x11.display, x11.x.XStringToKeysym(name.encode()))
            if not code: raise RuntimeError('Keyboard layout lacks a required key')
            self.codes[key] = code

    def button(self, key, down):
        buttons = {'MouseLeft':1, 'MouseMiddle':2, 'MouseRight':3, 'WheelUp':4, 'WheelDown':5}
        if key in buttons:
            ok = self.xt.XTestFakeButtonEvent(self.x11.display, buttons[key], down, 0)
        else:
            ok = self.xt.XTestFakeKeyEvent(self.x11.display, self.codes[key], down, 0)
        self.x11.x.XFlush(self.x11.display)
        if not ok: raise RuntimeError('Input injection failed')

    def motion(self, x, y):
        self.fraction = [self.fraction[0]+x, self.fraction[1]+y]
        dx, dy = map(int, self.fraction)
        self.fraction[0] -= dx; self.fraction[1] -= dy
        if dx or dy:
            if not self.xt.XTestFakeRelativeMotionEvent(self.x11.display, dx, dy, 0):
                raise RuntimeError('Pointer injection failed')
            self.x11.x.XFlush(self.x11.display)


def mapped_guard(channel, output, owns_focus, restore, timeout=.35):
    """EOF, missing frames, malformed output and exceptions all release held input."""
    channel.sendall(b'R')
    buffer = b''
    try:
        while select.select([channel], [], [], timeout)[0]:
            chunk = channel.recv(16384)
            if not chunk: break
            buffer += chunk
            if len(buffer)>32768: raise ValueError('Oversized input frame')
            while b'\n' in buffer:
                line, buffer = buffer.split(b'\n', 1)
                if line == b'K': continue
                packet = json.loads(line)
                if owns_focus(): output.apply(packet)
                else: output.release()
    finally:
        output.release()
        restore()
