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
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1] / 'packaging/integrations'
sys.path.insert(0, str(ROOT))
from overlay_support import RawPad, X11, identity, supported_emulator_process


class OverlayHelperTests(unittest.TestCase):
    def test_core_thread_name_does_not_hide_the_owned_emulator(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);process=root/'42';process.mkdir()
            (process/'comm').write_text('Main')
            (process/'exe').symlink_to('/app/bin/retroarch')
            self.assertTrue(supported_emulator_process(42,root))
            (process/'exe').unlink();(process/'exe').symlink_to('/tmp/mount/usr/bin/armsx2-qt')
            self.assertTrue(supported_emulator_process(42,root))
            (process/'exe').unlink();(process/'exe').symlink_to('/app/bin/PPSSPPSDL')
            self.assertTrue(supported_emulator_process(42,root))
            (process/'exe').unlink();(process/'exe').symlink_to('/usr/bin/unrelated')
            (process/'comm').write_text('retroarch')
            self.assertFalse(supported_emulator_process(42,root))
            self.assertFalse(supported_emulator_process(43,root))
    def test_armsx2_graceful_signal_is_owned_and_never_repeated(self):
        x=object.__new__(X11);x.graceful_signals=set()
        x.owns=lambda *args:True;x.pid=lambda window:42
        x.atom=lambda name:7;x.prop=lambda *args:[7]
        with patch('overlay_support.Path.readlink',return_value=Path('/usr/bin/armsx2-qt')), \
             patch('overlay_support.identity',return_value='123'), \
             patch('overlay_support.os.pidfd_open',return_value=9), \
             patch('overlay_support.os.close'), \
             patch('overlay_support.signal.pidfd_send_signal') as send:
            self.assertTrue(x.close(1,40,'120'));self.assertFalse(x.close(1,40,'120'))
            self.assertEqual(send.call_count,1)
            x.owns=lambda *args:False
            self.assertFalse(x.close(2,40,'120'));self.assertEqual(send.call_count,1)
    def test_physical_neutral_includes_every_button_stick_and_trigger(self):
        pad = object.__new__(RawPad)
        pad.axes = [0,1,2,3,4,5,20,21]
        pad.triggers = {20,21}
        keys = set(); values = {}
        def read(number, size):
            if number == 0x18:
                bits = bytearray(size)
                for key in keys: bits[key // 8] |= 1 << (key % 8)
                return bits
            axis = number - 0x40
            return struct.pack('iiiiii', values.get(axis, 78 if axis >= 20 else 0),
                               0 if axis in pad.triggers else -1408, 1552 if axis in pad.triggers else 1408,0,0,0)
        pad.get = read
        self.assertTrue(pad.sample()['neutral'])
        for code in (304,305,306,307,310,311,314,315,316,317,318):
            keys.add(code); self.assertFalse(pad.sample()['neutral']); keys.clear()
        for axis in pad.axes:
            values[axis] = 1200; self.assertFalse(pad.sample()['neutral']); values.clear()
        keys.add(305); self.assertTrue(pad.sample()['confirm']); self.assertFalse(pad.sample()['back'])
        keys.clear(); keys.add(304); self.assertTrue(pad.sample()['back'])
        keys.clear()
        # Odin uses 2/5 for triggers, unlike the Flip's 20/21.
        pad.axes = [0, 1, 2, 3, 4, 5]; pad.triggers = {2, 5}
        self.assertTrue(pad.sample()['neutral'])
        for axis in pad.axes:
            values[axis] = 1200
            self.assertFalse(pad.sample()['neutral']); values.clear()

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
