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
import json
from unittest.mock import patch, Mock

ROOT = Path(__file__).resolve().parents[1] / 'packaging/integrations'
sys.path.insert(0, str(ROOT))
from overlay_support import RawPad, X11, identity, supported_emulator_process
spec = importlib.util.spec_from_file_location('overlay', ROOT / 'adventure-overlay.py')
overlay = importlib.util.module_from_spec(spec); spec.loader.exec_module(overlay)
spec = importlib.util.spec_from_file_location('invitation_overlay', ROOT / 'invitation-overlay.py')
invitation_overlay = importlib.util.module_from_spec(spec); spec.loader.exec_module(invitation_overlay)
PNG = base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLttAAAAABJRU5ErkJggg==')


class OverlayHelperTests(unittest.TestCase):
    def test_native_crates_close_requires_managed_game_marker(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);process=root/'42';process.mkdir()
            (process/'exe').symlink_to('/packages/objects/game/bin/arbitrary-native-game')
            for env,expected in ((b'',False),(b'TRAINEROS_CRATES_GAME=0\0',False),
                                 (b'TRAINEROS_CRATES_GAME=10\0',False),
                                 (b'TRAINEROS_CRATES_GAME=1\0',True)):
                (process/'environ').write_bytes(env)
                self.assertEqual(supported_emulator_process(42,root),expected)

    def test_ordinary_pc_window_requires_managed_launch_marker(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); process = root / '42'; process.mkdir()
            (process / 'exe').symlink_to('/games/ordinary-game')
            for env, expected in ((b'', False), (b'TRAINEROS_PC_MANAGED=0\0', False),
                                  (b'TRAINEROS_PC_MANAGED=1\0', True)):
                (process / 'environ').write_bytes(env)
                self.assertEqual(supported_emulator_process(42, root), expected)

    def test_standalone_launch_flags_do_not_admit_plain_desktop_windows(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);process=root/'42';process.mkdir()
            for name, args in (('Cemu',b'Cemu\0-f\0-g\0game.wua\0'),
                               ('duckstation-qt',b'duckstation-qt\0-batch\0-fullscreen\0--\0game.chd\0'),
                               ('rpcs3',b'rpcs3\0--no-gui\0--fullscreen\0game.self\0'),
                               ('rpcs3.AppImage',b'AppRun.wrapped\0--no-gui\0--fullscreen\0game.self\0'),
                               ('eden.AppImage',b'eden\0-f\0-g\0game.nro\0'),
                               ('eden',b'eden\0-f\0-g\0game.nro\0'),
                               ('xemu',b'xemu\0-full-screen\0-dvd_path\0game.iso\0'),
                               ('xemu.AppImage',b'AppRun\0-full-screen\0-dvd_path\0game.iso\0')):
                target=process/'exe'
                if target.is_symlink():target.unlink()
                target.symlink_to('/app/bin/'+name)
                (process/'cmdline').write_bytes(args)
                self.assertTrue(supported_emulator_process(42,root))
                (process/'cmdline').write_bytes(name.encode()+b'\0')
                self.assertFalse(supported_emulator_process(42,root))

    def test_pad_read_grant_survives_late_inputplumber_acl_reset(self):
        spec = importlib.util.spec_from_file_location('pad_grant', ROOT / 'grant-input-read.py')
        hook = importlib.util.module_from_spec(spec); spec.loader.exec_module(hook)
        from types import SimpleNamespace
        from unittest.mock import MagicMock
        clock = [0.0]; reset = [False]; stamp = [1]; mode = [0]
        item = MagicMock(); item.name = 'event6'
        item.__truediv__.side_effect = lambda key: Mock(read_text=Mock(return_value='Retroid Pocket Gamepad' if key=='device/name' else '13:70'))
        sysfs = MagicMock(); sysfs.glob.return_value = [item]
        nodes = Path('/dev/input')
        def paths(value): return sysfs if value == '/sys/class/input' else nodes
        source = MagicMock(); source.__enter__.return_value.fileno.return_value = 77
        def stat(_): return SimpleNamespace(st_mode=0o020000|mode[0],st_uid=0,st_dev=2,st_ino=3,st_rdev=os.makedev(13,70),st_ctime_ns=stamp[0])
        def sleep(dt):
            clock[0] += dt
            if clock[0] >= .4 and not reset[0]:
                reset[0]=True;stamp[0]+=1;mode[0]=0
        def acl(*args,**kwargs): stamp[0]+=1;mode[0]=0o040
        def ioctl(fd, code, data, *args):
            value=b'Retroid Pocket Gamepad';data[:len(value)]=value
        with patch.object(hook,'Path',side_effect=paths), patch.object(hook.pwd,'getpwnam',return_value=SimpleNamespace(pw_uid=1000)), \
             patch.object(hook.os,'geteuid',return_value=0), patch.object(hook.os,'open',return_value=77), \
             patch.object(hook.os,'fdopen',return_value=source), patch.object(hook.os,'fstat',side_effect=stat), \
             patch.object(hook.fcntl,'ioctl',side_effect=ioctl), patch.object(hook.subprocess,'run',side_effect=acl) as grant, \
             patch.object(hook.time,'monotonic',side_effect=lambda:clock[0]), patch.object(hook.time,'sleep',side_effect=sleep):
            self.assertTrue(hook.grant('test'))
            self.assertEqual(grant.call_count,2)
            self.assertGreaterEqual(clock[0],2.4)
            self.assertIn('u:1000:r--',grant.call_args.args[0])
            self.assertEqual(grant.call_args.kwargs['pass_fds'],(77,))

    def test_badge_replays_outside_taps_but_consumes_inside_press_and_release(self):
        view = Mock(); view.display = 99; view.root = 42
        pointer = invitation_overlay.Pointer(view)
        events = []
        view.x.XPending.side_effect = lambda *_: len(events)
        def take(_, output):
            kind, x, y = events.pop(0)
            output._obj.type = kind
            output._obj.button.x_root = x; output._obj.button.y_root = y
            output._obj.button.time = 123
        view.x.XNextEvent.side_effect = take
        region = {"invitation": (1760,920,144,144)}
        events.extend([(4,800,600),(5,800,600)])
        self.assertFalse(pointer.poll(region, True))
        view.x.XAllowEvents.assert_called_with(99,2,123)  # Original press replayed.
        events.append((4,1820,1000))
        self.assertEqual(pointer.poll(region, True), [("invitation","press",1820,1000)])
        view.x.XAllowEvents.assert_called_with(99,0,123)
        events.append((5,1820,1000));self.assertEqual(pointer.poll(region, True), [("invitation","release",1820,1000)])
        events.extend([(4,1820,1000),(5,1820,1000)])
        self.assertFalse(pointer.poll(region, False))  # Another application's focus.
        view.x.XAllowEvents.assert_called_with(99,2,123)
        events.extend([(4,1820,1000),(5,800,600)])
        # The UI decides whether this is a drag or a cancelled invitation tap.
        self.assertEqual(pointer.poll(region, True), [("invitation","press",1820,1000),("invitation","release",800,600)])

    def test_sparse_player_regions_keep_drag_target_and_replay_gaps(self):
        view = Mock(); view.display = 99; view.root = 42
        pointer = invitation_overlay.Pointer(view); events = []
        view.x.XPending.side_effect = lambda *_: len(events)
        def take(_, output):
            kind, x, y = events.pop(0)
            output._obj.type = kind
            output._obj.button.x_root = x; output._obj.button.y_root = y
            output._obj.button.time = 123
        view.x.XNextEvent.side_effect = take
        regions = {'p1': (10,10,48,48), 'p2': (70,10,48,48), 'invite': (700,400,68,68)}
        events.extend([(4,62,30),(5,62,30)])
        self.assertEqual(pointer.poll(regions, True), [])
        view.x.XAllowEvents.assert_called_with(99,2,123)
        events.extend([(4,80,30),(6,500,300),(5,500,300)])
        self.assertEqual(pointer.poll(regions, True), [('p2','press',80,30),('p2','move',500,300),('p2','release',500,300)])
        view.x.XAllowEvents.assert_called_with(99,0,123)
        events.append((4,80,30));pointer.poll(regions, True)
        events.append((5,80,30));self.assertEqual(pointer.poll({}, True), [])
        events.append((4,80,30));pointer.poll(regions, True)
        events.append((5,80,30));self.assertEqual(pointer.poll(regions, False), [])
        events.append((4,720,420));self.assertEqual(pointer.poll(regions, True), [('invite','press',720,420)])

    def test_badge_watchdog_never_stops_reused_pid(self):
        with patch.object(invitation_overlay.socket,'socket') as pipe, \
             patch.object(invitation_overlay.select,'select',return_value=([],[],[])), \
             patch.object(invitation_overlay,'identity',return_value='new-process'), \
             patch.object(invitation_overlay.os,'kill') as stop:
            invitation_overlay.watchdog(7,42,'original-process');stop.assert_not_called()
        with patch.object(invitation_overlay.socket,'socket') as pipe, \
             patch.object(invitation_overlay.select,'select',return_value=([],[],[])), \
             patch.object(invitation_overlay,'identity',return_value='original-process'), \
             patch.object(invitation_overlay.os,'kill') as stop:
            invitation_overlay.watchdog(7,42,'original-process')
            stop.assert_called_once_with(42,invitation_overlay.signal.SIGKILL)

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

    def test_rmg_close_requires_the_native_session_bridge(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); process = root / '42'; process.mkdir()
            (process / 'exe').symlink_to('/bundle/bin/RMG')
            (process / 'environ').write_bytes(b'OTHER=value\0')
            self.assertFalse(supported_emulator_process(42, root))
            (process / 'environ').write_bytes(b'TRAINEROS_RMG_SESSION=\0')
            self.assertFalse(supported_emulator_process(42, root))
            (process / 'environ').write_bytes(b'TRAINEROS_RMG_SESSION=/private/session.json\0')
            self.assertTrue(supported_emulator_process(42, root))

    def test_flycast_close_requires_an_owned_session_marker(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); process = root / '42'; process.mkdir()
            for name in ('flycast', 'flycast-dojo'):
                executable = process / 'exe'; executable.symlink_to('/bundle/bin/' + name)
                for env, expected in ((b'OTHER=value\0', False),
                                      (b'TRAINEROS_FLYCAST_SESSION=\0', False),
                                      (b'TRAINEROS_FLYCAST_SESSION=private\0', True)):
                    (process / 'environ').write_bytes(env)
                    self.assertEqual(supported_emulator_process(42, root), expected)
                executable.unlink()

    def test_vita_close_requires_managed_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);process=root/'42';process.mkdir()
            (process/'exe').symlink_to('/bundle/usr/bin/Vita3K')
            for env,expected in ((b'OTHER=value\0',False),
                (b'TRAINEROS_VITA_MANAGED=0\0',False),
                (b'TRAINEROS_VITA_MANAGED=1extra\0',False),
                (b'TRAINEROS_VITA_MANAGED=1\0',True)):
                (process/'environ').write_bytes(env)
                self.assertEqual(supported_emulator_process(42,root),expected)

    def test_remote_play_close_requires_managed_guest(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);process=root/'42';process.mkdir()
            (process/'exe').symlink_to('/bundle/stream_guest')
            for env,expected in ((b'OTHER=value\0',False),
                (b'TRAINEROS_REMOTE_PLAY_MANAGED=0\0',False),
                (b'TRAINEROS_REMOTE_PLAY_MANAGED=1\0',True)):
                (process/'environ').write_bytes(env)
                self.assertEqual(supported_emulator_process(42,root),expected)

    def test_crates_webgame_close_requires_owned_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);process=root/'42';process.mkdir()
            (process/'exe').symlink_to('/bundle/bin/traineros-webgame')
            for env,expected in ((b'OTHER=value\0',False),
                (b'TRAINEROS_CRATES_GAME=0\0',False),
                (b'TRAINEROS_CRATES_GAME=1\0',True)):
                (process/'environ').write_bytes(env)
                self.assertEqual(supported_emulator_process(42,root),expected)

    def test_azahar_close_requires_managed_launch_or_private_room(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);process=root/'42';process.mkdir()
            (process/'exe').symlink_to('/bundle/bin/azahar')
            for env,expected in ((b'OTHER=value\0',False),
                (b'TRAINEROS_AZAHAR_MANAGED=0\0',False),
                (b'TRAINEROS_AZAHAR_MANAGED=1extra\0',False),
                (b'TRAINEROS_AZAHAR_MANAGED=1\0',True),
                (b'TRAINEROS_AZAHAR_SESSION=\0',False),
                (b'TRAINEROS_AZAHAR_SESSION=/private/session.json\0',True)):
                (process/'environ').write_bytes(env)
                self.assertEqual(supported_emulator_process(42,root),expected)

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
            (process/'exe').unlink();(process/'exe').symlink_to('/private/runtime/bin/traineros-ds')
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
        pad.name = 'Retroid Pocket Gamepad'
        pad.shell_mapping = {}
        pad.mapping_stamp = None
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
        # Shared shell hints/Confirm overrides reach raw helpers without changing
        # the physical button and stick coordinates used by game profiles.
        with tempfile.TemporaryDirectory() as directory, patch.dict(os.environ, {'XDG_RUNTIME_DIR':directory}):
            path=Path(directory)/'traineros-shell-input.json'
            path.write_text(json.dumps(dict(version=1, device=pad.name, southConfirms=True, invertX=True)))
            keys.add(304);values[0]=1200
            state=pad.sample()
            self.assertTrue(state['confirm']);self.assertFalse(state['back'])
            self.assertTrue(state['left']);self.assertGreater(state['sticks'][0],0)
            self.assertIn('south',state['buttons'])
            keys.clear();values.clear()
            for bad in ([], dict(version=1, device='unrelated', southConfirms=True)):
                path.write_text(json.dumps(bad));pad.mapping_stamp=None
                keys.add(305);self.assertTrue(pad.sample()['confirm']);keys.clear()
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
