"""Shared Home chord and bounded, focus-owned X11 keyboard output."""
import ctypes as c
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from game_input import XTestSink


class HomeChord:
    def __init__(self):
        self.held = self.used = self.blocked = False

    def update(self, home, x, neutral=False):
        if self.blocked:
            if neutral: self.blocked = False
            return ''
        if home:
            self.held = True
            if x and not self.used:
                self.used = True
                return 'keyboard'
        elif self.held:
            event = '' if self.used else 'home'
            self.held = self.used = False
            self.blocked = not neutral
            return event
        return ''


class KeyboardNavigation:
    def __init__(self):
        self.armed = False
        self.held = ''
        self.last = 0

    def action(self, sample):
        if not self.armed:
            self.armed = sample.get('neutral', False)
            return ''
        key = next((k for k in ('confirm','back','up','down','left','right','select',
                               'secondary','recent','previousFace','nextFace') if sample.get(k)), '')
        result = ''
        if key and (key != self.held or (key in ('up','down','left','right') and time.monotonic()-self.last > .15)):
            result = key
            self.last = time.monotonic()
        self.held = key
        return result


class KeyboardOutput:
    def __init__(self, x11, owns_focus):
        self.x11, self.owns_focus = x11, owns_focus
        self.sink = XTestSink(x11)
        meta=x11.x.XKeysymToKeycode(x11.display,x11.x.XStringToKeysym(b'Super_L'))
        if meta:self.sink.codes['Meta']=meta

    def tap(self, key, modifiers=()):
        if key not in self.sink.codes or any(m not in self.sink.codes for m in modifiers) or not self.owns_focus(): return
        pressed = []
        try:
            for item in (*modifiers, key):
                pressed.append(item);self.sink.button(item, True)
        finally:
            for item in reversed(pressed): self.sink.button(item, False)

    def text(self, text):
        # Reserve an unused keycode only for this synchronous request, then restore
        # its entire mapping even on failure. Never change the active layout.
        if not isinstance(text, str) or not text or len(text) > 16 or not self.owns_focus(): return
        x, display = self.x11.x, self.x11.display
        x.XDisplayKeycodes.argtypes = [c.c_void_p,c.POINTER(c.c_int),c.POINTER(c.c_int)]
        x.XGetKeyboardMapping.argtypes = [c.c_void_p,c.c_ubyte,c.c_int,c.POINTER(c.c_int)]
        x.XGetKeyboardMapping.restype = c.POINTER(c.c_ulong)
        x.XChangeKeyboardMapping.argtypes = [c.c_void_p,c.c_int,c.c_int,c.POINTER(c.c_ulong),c.c_int]
        x.XSync.argtypes = [c.c_void_p,c.c_int]
        low, high, width = c.c_int(), c.c_int(), c.c_int()
        x.XDisplayKeycodes(display,c.byref(low),c.byref(high))
        mapping = x.XGetKeyboardMapping(display,low.value,high.value-low.value+1,c.byref(width))
        if not mapping: return
        try:
            code = next((k for k in range(high.value,low.value-1,-1)
                         if not any(mapping[(k-low.value)*width.value+i] for i in range(width.value))), None)
            if code is None: return
            saved = (c.c_ulong * width.value)(*[mapping[(code-low.value)*width.value+i] for i in range(width.value)])
            try:
                for char in text:
                    if not self.owns_focus(): break
                    if char in '\r\n': self.tap('Enter'); continue
                    if char == '\t': self.tap('Tab'); continue
                    if not char.isprintable(): continue
                    sym = ord(char) if ord(char) < 256 else 0x01000000 | ord(char)
                    values = (c.c_ulong * width.value)(*([sym]*width.value))
                    x.XChangeKeyboardMapping(display,code,width.value,values,1); x.XSync(display,False)
                    self.sink.xt.XTestFakeKeyEvent(display,code,True,0)
                    self.sink.xt.XTestFakeKeyEvent(display,code,False,0)
                    x.XSync(display,False)
                    time.sleep(.012) # Allow clients to consume the temporary Unicode mapping.
            finally:
                x.XChangeKeyboardMapping(display,code,width.value,saved,1); x.XSync(display,False)
        finally: x.XFree(mapping)

    def apply(self, packet):
        if not self.owns_focus(): return
        action = packet.get('action')
        commands = {'enter':'Enter','backspace':'Backspace','left':'Left','right':'Right',
                    'paste':'Ctrl+V','copy':'Ctrl+C','cut':'Ctrl+X','select-all':'Ctrl+A',
                    'undo':'Ctrl+Z','redo':'Ctrl+Y'}
        if action in commands:
            parts=commands[action].split('+');self.tap(parts[-1],parts[:-1]);return
        if action == 'period-space': self.tap('Backspace');self.text('. ');return
        if action:return
        if 'key' not in packet: self.text(packet.get('text',''));return
        key, mods = packet.get('key'), packet.get('modifiers',0)
        if not isinstance(key,int) or not isinstance(mods,int) or mods & ~0x7e000000: return
        # Qt::KeyboardModifier bits; keypad is a location hint.
        modifiers=[name for bit,name in ((0x04000000,'Ctrl'),(0x08000000,'Alt'),(0x02000000,'Shift'),(0x10000000,'Meta')) if mods&bit]
        text = packet.get('text','')
        if text and not mods & (0x04000000|0x08000000|0x10000000): self.text(text);return
        names={0x01000000:'Escape',0x01000001:'Tab',0x01000003:'Backspace',0x01000004:'Enter',
               0x01000005:'Enter',0x01000006:'Insert',0x01000007:'Delete',0x01000010:'Home',
               0x01000011:'End',0x01000012:'Left',0x01000013:'Up',0x01000014:'Right',
               0x01000015:'Down',0x01000016:'PageUp',0x01000017:'PageDown',32:'Space'}
        name=names.get(key,chr(key) if 48<=key<=90 else 'F'+str(key-0x01000030+1) if 0x01000030<=key<=0x0100003b else '')
        if name: self.tap(name,modifiers)


class KeyboardWriter:
    """A separate output process finishes key-up/restoration if its caller dies."""
    def __init__(self, root):
        self.root=root
        self.process=subprocess.Popen([sys.executable,str(Path(__file__).resolve()),str(root[0]),str(root[1])],
            stdin=subprocess.PIPE,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True)
        os.set_blocking(self.process.stdin.fileno(),False)

    def apply(self, packet):
        data=json.dumps(packet,ensure_ascii=True).encode()+b'\n'
        if len(data)>4096 or self.process.poll() is not None:raise RuntimeError('Keyboard output is unavailable.')
        try:
            if os.write(self.process.stdin.fileno(),data)!=len(data):raise RuntimeError('Keyboard output is busy.')
        except OSError as error:raise RuntimeError('Keyboard output is unavailable.') from error

    def close(self):
        self.process.stdin.close() # EOF completes the current bounded key-up before exiting.


def run_writer(pid,start):
    from overlay_support import X11
    x11=X11();output=KeyboardOutput(x11,lambda:x11.owns(x11.active(),pid,start))
    buffer=b''
    while True:
        chunk=os.read(sys.stdin.fileno(),4096)
        if not chunk:return
        buffer+=chunk
        if len(buffer)>8192:return
        while b'\n' in buffer:
            line,buffer=buffer.split(b'\n',1)
            packet=json.loads(line)
            if not isinstance(packet,dict):return
            output.apply(packet)


if __name__=='__main__':
    run_writer(int(sys.argv[1]),sys.argv[2])
