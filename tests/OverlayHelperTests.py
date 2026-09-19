import importlib.util
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1] / 'packaging/integrations'
sys.path.insert(0, str(ROOT))
from overlay_support import RawPad, identity


class OverlayHelperTests(unittest.TestCase):
    def test_physical_neutral_includes_every_button_stick_and_trigger(self):
        pad = object.__new__(RawPad)
        pad.axes = [0,1,2,3,4,5,20,21]
        keys = set(); values = {}
        def read(number, size):
            if number == 0x18:
                bits = bytearray(size)
                for key in keys: bits[key // 8] |= 1 << (key % 8)
                return bits
            axis = number - 0x40
            return struct.pack('iiiiii', values.get(axis, 78 if axis >= 20 else 0),
                               0 if axis >= 20 else -1408, 1552 if axis >= 20 else 1408,0,0,0)
        pad.get = read
        self.assertTrue(pad.sample()['neutral'])
        for code in (304,305,306,307,310,311,314,315,316,317,318):
            keys.add(code); self.assertFalse(pad.sample()['neutral']); keys.clear()
        for axis in pad.axes:
            values[axis] = 1200; self.assertFalse(pad.sample()['neutral']); values.clear()
        keys.add(305); self.assertTrue(pad.sample()['confirm']); self.assertFalse(pad.sample()['back'])
        keys.clear(); keys.add(304); self.assertTrue(pad.sample()['back'])

    def guard(self, channel, path):
        code = '''import importlib.util, socket, sys
sys.path.insert(0,sys.argv[1])
s=importlib.util.spec_from_file_location('overlay',sys.argv[1]+'/adventure-overlay.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
from pathlib import Path
m.guard_loop(socket.socket(fileno=int(sys.argv[2])),lambda:Path(sys.argv[3]).write_text('restored'),.25)
'''
        return subprocess.Popen([sys.executable,'-c',code,str(ROOT),str(channel.fileno()),str(path)],pass_fds=(channel.fileno(),))

    def test_guard_restores_after_pipe_loss_and_heartbeat_stall(self):
        for lost in (True,False):
            with self.subTest(lost=lost), tempfile.TemporaryDirectory() as directory:
                parent, child = socket.socketpair(); path = Path(directory)/'restored'
                process = self.guard(child,path); child.close()
                try:
                    parent.settimeout(2); self.assertEqual(parent.recv(1),b'R')
                    for _ in range(4):
                        parent.sendall(b'K'); time.sleep(.05)
                        self.assertIsNone(process.poll()); self.assertFalse(path.exists())
                    if lost: parent.close()
                    self.assertEqual(process.wait(timeout=2),0)
                    self.assertEqual(path.read_text(),'restored')
                finally:
                    parent.close()
                    if process.poll() is None: process.kill(); process.wait()

    def test_process_identity_is_not_just_a_pid(self):
        self.assertTrue(identity(os.getpid()).isdigit())
        with self.assertRaises(OSError): identity(999999999)


if __name__ == '__main__': unittest.main()
