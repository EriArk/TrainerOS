#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real temporary-file preparation/launch checks; no emulator or handheld IO.

Disc/runtime admission and the Linux transport-file boundary are fixture inputs.
The INI copies, atomic publication, receipt and launch arguments are real.
"""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import launcher
from check_config import MATERIAL


class LauncherTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve()
        self.ordinary=self.root/'ARMSX2'
        (self.ordinary/'inis').mkdir(parents=True)
        (self.ordinary/'gamesettings').mkdir()
        (self.ordinary/'memcards').mkdir()
        (self.ordinary/'memcards/Own.ps2').write_bytes(b'personal card sentinel')
        (self.ordinary/'inis/PCSX2.ini').write_bytes(
            b'[GameList]\nPaths=/unavailable/share\n[Pad1]\nCross=SDL-0/A\n'
            b'[MemoryCards]\nSlot1_Filename=Own.ps2\n')
        (self.ordinary/'inis/UI.ini').write_bytes(b'[UI]\nTheme=personal\n')
        (self.ordinary/'gamesettings/SLUS-21240_02F4B541.ini').write_bytes(b'[Graphics]\nRenderer=14\n')
        self.baseline={p.relative_to(self.ordinary):p.read_bytes() for p in self.ordinary.rglob('*') if p.is_file()}
        self.session=self.root/'session';self.session.mkdir(mode=0o700)
        self.path=self.session/'request.json'
        self.transport=self.root/'session.conf';self.transport.write_bytes(MATERIAL)
        self.identity={'serial':'SLUS-21240','crc':'02F4B541','runtime':'runtime-a','settings':'locked'}
        self.data={'content':str(self.root/'owned.iso'),'expected':self.identity,
                   'connection':str(self.transport),'host':True}
        self.path.write_text(json.dumps(self.data))
        self.setup={'runtime':str(self.root/'armsx2.AppImage'),'runtimeSha256':'runtime-a',
                    'configRoot':str(self.ordinary),'settings':'locked'}
        for name,kwargs in (
                ('identify',{'return_value':self.identity}),
                ('manifest',{'side_effect':lambda:dict(self.setup)}),
                ('connection_material',{'side_effect':lambda data:self.transport.read_bytes()})):
            mock=patch.object(launcher,name,**kwargs);mock.start();self.addCleanup(mock.stop)

    def tearDown(self):
        actual={p.relative_to(self.ordinary):p.read_bytes() for p in self.ordinary.rglob('*') if p.is_file()}
        self.assertEqual(actual,self.baseline,'Personal settings/cards must remain untouched')

    def assert_not_launched(self):
        with patch.object(launcher.os,'execvpe') as execute:
            with self.assertRaises((ValueError,OSError,KeyError)):
                launcher.run(self.path)
            execute.assert_not_called()

    def test_complete_preparation_and_isolated_launch(self):
        self.assertEqual(launcher.prepare_request(self.path),{'ready':True})
        ready=self.session/'prepared'
        ini=(ready/'inis/PCSX2.ini').read_bytes()
        self.assertNotIn(b'/unavailable/share',ini)
        self.assertIn(b'Cross=SDL-0/A',ini)
        self.assertIn(b'Slot1_Filename=Own.ps2',ini)
        self.assertIn(b'Renderer=14',(ready/'game.ini').read_bytes())
        with patch.object(launcher.os,'execvpe') as execute:
            launcher.run(self.path)
        program,args,env=execute.call_args.args
        self.assertEqual(program,'/usr/bin/bwrap')
        self.assertIn('--unshare-net',args)
        self.assertIn(str(ready/'inis'),args)
        self.assertEqual(args[args.index('-gamecfg')+1],str(ready/'game.ini'))
        self.assertEqual(args[-2:],['--',self.data['content']])
        self.assertNotIn('abcdef123456',' '.join(args))
        self.assertEqual(env['XDG_CONFIG_HOME'],str(self.ordinary.parent))

    def test_copy_failure_leaves_no_ready_state_and_retry_succeeds(self):
        copy=launcher.shutil.copyfile
        count=0
        def fail_second(source,target):
            nonlocal count
            count+=1
            if count==2:raise OSError('simulated full disk')
            return copy(source,target)
        with patch.object(launcher.shutil,'copyfile',side_effect=fail_second):
            with self.assertRaises(OSError):launcher.prepare_request(self.path)
        self.assertEqual(list(self.session.iterdir()),[self.path])
        self.assert_not_launched()
        self.assertEqual(launcher.prepare_request(self.path),{'ready':True})

    def test_invalid_base_ini_leaves_no_ready_state(self):
        with patch.object(launcher,'launch_settings',side_effect=ValueError('invalid INI')):
            with self.assertRaises(ValueError):launcher.prepare_request(self.path)
        self.assertEqual(list(self.session.iterdir()),[self.path])
        self.assert_not_launched()

    def test_connection_replaced_during_preparation(self):
        original=launcher.launch_settings
        def replace_connection(value):
            self.transport.write_bytes(MATERIAL.replace(b'abcdef123456',b'123456abcdef'))
            return original(value)
        with patch.object(launcher,'launch_settings',side_effect=replace_connection):
            with self.assertRaises(ValueError):launcher.prepare_request(self.path)
        self.assertFalse((self.session/'prepared').exists())
        self.assertEqual(list(self.session.iterdir()),[self.path])

    def test_duplicate_prepare_does_not_overwrite_active_settings(self):
        launcher.prepare_request(self.path)
        ready=self.session/'prepared'
        original={p.relative_to(ready):p.read_bytes() for p in ready.rglob('*') if p.is_file()}
        with self.assertRaises(ValueError):launcher.prepare_request(self.path)
        self.assertEqual(original,{p.relative_to(ready):p.read_bytes() for p in ready.rglob('*') if p.is_file()})

    def test_connection_removed_before_launch(self):
        launcher.prepare_request(self.path);self.transport.unlink()
        self.assert_not_launched()

    def test_connection_replaced_before_launch(self):
        launcher.prepare_request(self.path)
        self.transport.write_bytes(MATERIAL.replace(b'abcdef123456',b'123456abcdef'))
        self.assert_not_launched()

    def test_request_replaced_before_launch(self):
        launcher.prepare_request(self.path)
        self.path.write_text(json.dumps(dict(self.data,content=str(self.root/'other.iso'))))
        self.assert_not_launched()

    def test_runtime_replaced_before_launch(self):
        launcher.prepare_request(self.path);self.setup['runtimeSha256']='runtime-b'
        self.assert_not_launched()

    def test_personal_configuration_root_changed_before_launch(self):
        launcher.prepare_request(self.path)
        self.setup['configRoot']=str(self.root/'another-profile/ARMSX2')
        self.assert_not_launched()

    def test_partial_or_changed_settings_never_launch(self):
        launcher.prepare_request(self.path)
        ready=self.session/'prepared'
        for relative in ('game.ini','inis/PCSX2.ini','inis/UI.ini','ready.json'):
            with self.subTest(relative=relative):
                path=ready/relative;original=path.read_bytes()
                path.unlink();self.assert_not_launched()
                path.write_bytes(b'not json' if relative=='ready.json' else original+b' ')
                self.assert_not_launched()
                path.write_bytes(original)
        extra=ready/'inis/unexpected.ini';extra.write_bytes(b'[Extra]\n')
        self.assert_not_launched()

    def test_unpublished_staging_directory_cannot_launch(self):
        abandoned=self.session/'.prepare-abandoned';abandoned.mkdir()
        (abandoned/'game.ini').write_bytes(b'[DEV9/Eth]\n')
        # A killed preparation is owned by the outer session's ordinary cleanup.
        self.assert_not_launched()
        self.assertEqual(launcher.prepare_request(self.path),{'ready':True})
        self.assertTrue(abandoned.exists())


if __name__=='__main__':unittest.main()
