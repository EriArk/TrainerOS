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
import base64
from unittest.mock import patch, Mock

ROOT = Path(__file__).resolve().parents[1] / 'packaging/integrations'
sys.path.insert(0, str(ROOT))
from overlay_support import RawPad, X11, identity, supported_emulator_process
spec = importlib.util.spec_from_file_location('overlay', ROOT / 'adventure-overlay.py')
overlay = importlib.util.module_from_spec(spec); spec.loader.exec_module(overlay)
PNG = base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLttAAAAABJRU5ErkJggg==')


class OverlayHelperTests(unittest.TestCase):
    def test_png_waits_for_complete_output_and_keeps_old_frame_until_ready(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); frame = root / 'frame.png'; frame.write_bytes(b'previous')
            control = root / 'control'
            control.write_text('#!' + sys.executable + '\n' +
                'import sys,time\nfrom pathlib import Path\np=Path(sys.argv[2])\n' +
                'assert sys.argv[1]=="screenshot" and sys.argv[3]=="4"\n' +
                'p.write_bytes(' + repr(PNG[:-12]) + ')\ntime.sleep(.08)\n' +
                'assert (p.parent.parent/"frame.png").read_bytes()==b"previous"\n' +
                'with p.open("ab") as f:f.write(' + repr(PNG[-12:]) + ')\n')
            control.chmod(0o700)
            overlay.capture_png(root, str(control))
            self.assertEqual(frame.read_bytes(), PNG)
            self.assertFalse(list(root.glob('png-*')))

    def test_partial_oversized_and_symlink_png_are_not_ready(self):
        with tempfile.TemporaryDirectory() as directory:
            target=Path(directory)/'screen.png'
            self.assertFalse(overlay.complete_png(target))
            target.write_bytes(PNG[:-12]);self.assertFalse(overlay.complete_png(target))
            target.write_bytes(PNG);self.assertTrue(overlay.complete_png(target))
            bad=bytearray(PNG);bad[16:20]=struct.pack('>I',4097);target.write_bytes(bad)
            with self.assertRaises(RuntimeError):overlay.complete_png(target)
            target.unlink();target.symlink_to(Path(directory)/'elsewhere')
            with self.assertRaises(RuntimeError):overlay.complete_png(target)

    def test_timed_out_png_cannot_reuse_old_frame_or_retry_capture(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);frame=root/'frame.png';frame.write_bytes(PNG)
            paths=[]
            def command(args,**kwargs):
                paths.append(args[2]);Path(args[2]).write_bytes(PNG[:-12])
            with patch.object(overlay.subprocess,'run',side_effect=command), \
                 patch.object(overlay.time,'monotonic',side_effect=[0,0,2,3,3,5]), \
                 patch.object(overlay.time,'sleep'):
                for _ in range(2):
                    with self.assertRaises(RuntimeError):overlay.capture_png(root,'control')
            self.assertNotEqual(paths[0],paths[1]);self.assertEqual(frame.read_bytes(),PNG)
            self.assertFalse(list(root.glob('png-*')))
        with patch.object(overlay.shutil,'which',return_value='/usr/bin/gamescopectl'), \
             patch.object(overlay,'capture_png',side_effect=RuntimeError('timeout')), \
             patch.object(overlay,'capture_avif') as fallback:
            with self.assertRaises(RuntimeError):overlay.capture(Path('/unused'))
            fallback.assert_not_called()

    def test_old_compositor_without_control_retains_avif_route(self):
        with patch.object(overlay.shutil,'which',return_value=None), \
             patch.object(overlay,'capture_avif') as fallback:
            overlay.capture(Path('/unused'));fallback.assert_called_once_with(Path('/unused'))

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
    def test_ds_and_only_batch_dolphin_with_single_confirmation(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);process=root/'42';process.mkdir()
            (process/'exe').symlink_to('/tmp/mount/usr/bin/melonDS')
            self.assertTrue(supported_emulator_process(42,root))
            (process/'exe').unlink();(process/'exe').symlink_to('/app/bin/dolphin-emu')
            for args, expected in ((b'dolphin-emu\0-b\0-e\0game.iso\0',False),
                (b'dolphin-emu\0-C\0Dolphin.Interface.ConfirmStop=False\0',False),
                (b'dolphin-emu\0-b\0-C\0Dolphin.Interface.ConfirmStop=False\0-e\0game.iso\0',True)):
                (process/'cmdline').write_bytes(args)
                self.assertEqual(supported_emulator_process(42,root),expected)

    def test_dolphin_close_never_escalates_a_pending_shutdown(self):
        x=object.__new__(X11);x.graceful_closes=set();x.x=Mock();x.display=None
        x.x.XSendEvent.return_value=1
        x.owns=lambda *args:True;x.pid=lambda window:42
        x.atom=lambda name:7;x.prop=lambda *args:[7]
        with patch('overlay_support.Path.readlink',return_value=Path('/app/bin/dolphin-emu')), \
             patch('overlay_support.identity',return_value='123'), \
             patch('overlay_support.supported_emulator_process',return_value=True):
            self.assertTrue(x.close(1,40,'120'));self.assertFalse(x.close(1,40,'120'))
            self.assertEqual(x.x.XSendEvent.call_count,1)
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
        for code, action in ((307,'secondary'),(308,'recent'),(314,'select'),(315,'start'),
                             (310,'previousPage'),(311,'nextPage'),(546,'left'),(547,'right')):
            keys.add(code); self.assertTrue(pad.sample()[action]); self.assertFalse(pad.sample()['neutral']); keys.clear()
        keys.add(544); self.assertTrue(pad.sample()['up']); self.assertFalse(pad.sample()['down']); keys.clear()
        keys.add(545); self.assertTrue(pad.sample()['down']); keys.clear()
        values[1]=-1200; self.assertTrue(pad.sample()['up'])
        values[1]=1200; self.assertTrue(pad.sample()['down']); values.clear()
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

    def test_watchdog_hides_browsing_shell_only_while_same_game_is_alive(self):
        for game_alive in (True,False):
            device=Mock();device.mode.return_value=0
            pad=Mock();view=Mock()
            with patch.object(overlay,'identity',return_value='owned' if game_alive else 'reused'):
                overlay.restore_input(device,pad,view,31,'shell-start',42,'owned')
            view.hide_prompt.assert_called_once_with(31,'shell-start',all_windows=game_alive)


if __name__ == '__main__': unittest.main()
