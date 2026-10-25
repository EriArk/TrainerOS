#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import tempfile
import unittest
from session_config import connection, overlay, prepare, launch_settings
from launcher import stable

MATERIAL = b'mode=ps2\naddress=127.0.0.1\nhost=true\nport=19072\npeer=1\nroom=abcdef123456\nsocket=/tmp/traineros-test/ps2.sock\n'


class ConfigurationTests(unittest.TestCase):
    def test_preparation_rejects_runtime_replacement_after_admission(self):
        admitted={'id':'runtime.armsx2.lan.v1','content':'disc','runtime':'reviewed-a','settings':'locked'}
        self.assertNotEqual(stable(admitted),stable(dict(admitted,runtime='reviewed-b')))
        self.assertNotEqual(stable(admitted),stable({k:v for k,v in admitted.items() if k!='runtime'}))
        self.assertEqual(stable(admitted),stable(dict(admitted,label='Local title',connections=['online'])))

    def test_private_launch_skips_library_scanning_preserving_other_settings(self):
        original=b'[GameList]\nPaths = /mnt/share\nRecursivePaths=/mnt/nfs\nExcludedPaths=/skip\n[Pad1]\nPaths=keep\n[MemoryCards]\nSlot1_Filename=Own.ps2\n'
        result=launch_settings(original)
        self.assertNotIn(b'/mnt/',result)
        self.assertIn(b'ExcludedPaths=/skip',result)
        self.assertIn(b'[Pad1]\nPaths=keep',result)
        self.assertIn(b'Slot1_Filename=Own.ps2',result)

    def test_existing_game_settings_and_personal_cards_remain_unchanged(self):
        original = (b'[MemoryCards]\nSlot1_Filename = My\\x20Card.ps2\n'
                    b'[DEV9/Eth]\nEthEnable = false\nEthApi = Sockets\nLocalLinkRoomCode = old\n'
                    b'PS2IP = 192.0.2.2\n[Pad1]\nCross = SDL-0/A\n')
        with tempfile.TemporaryDirectory() as root:
            root = Path(root).resolve()
            source = root / 'original.ini'; source.write_bytes(original)
            output = root / 'session.ini'; prepare(source, output, MATERIAL)
            self.assertEqual(source.read_bytes(), original)
            result = output.read_bytes()
            self.assertIn(b'[MemoryCards]\nSlot1_Filename = My\\x20Card.ps2\n', result)
            self.assertIn(b'[Pad1]\nCross = SDL-0/A\n', result)
            self.assertIn(b'PS2IP = 192.0.2.2\n', result)
            self.assertNotIn(b' = old', result)
            # Pcsx2Config.cpp NetApiNames is the persisted enum, including space.
            self.assertEqual(result.count(b'EthApi = Local Link'), 1)
            self.assertIn(b'LocalLinkRoomCode = abcdef123456', result)
            with self.assertRaises(FileExistsError): prepare(source, output, MATERIAL)
            with self.assertRaises(FileExistsError): prepare(source, source, MATERIAL)
            self.assertEqual(source.read_bytes(), original)

    def test_guest_and_missing_game_override(self):
        guest = MATERIAL.replace(b'host=true', b'host=false').replace(b'peer=1', b'peer=2').replace(b'19072', b'19073')
        result = overlay(b'', guest)
        self.assertIn(b'LocalLinkHost = false', result)
        self.assertIn(b'LocalLinkPeerId = 2', result)
        self.assertIn(b'LocalLinkPort = 19073', result)

    def test_unterminated_lines_and_section_after_network(self):
        result = overlay(b'[DEV9/Eth]\nEthEnable=false\n[Graphics]\nRenderer=14', MATERIAL)
        self.assertLess(result.index(b'LocalLinkHost'), result.index(b'[Graphics]'))
        self.assertTrue(result.endswith(b'Renderer=14\n'))
        self.assertIn(b'Renderer=14\n\n[DEV9/Eth]', overlay(b'[Graphics]\nRenderer=14', MATERIAL))

    def test_invalid_material_and_ambiguous_ini_fail_closed(self):
        for bad in (MATERIAL + b'room=another\n', MATERIAL + b'extra=1\n',
                    MATERIAL.replace(b'127.0.0.1', b'8.8.8.8'), MATERIAL.replace(b'ps2', b'psp'),
                    MATERIAL.replace(b'peer=1', b'peer=2'), MATERIAL.replace(b'host=true', b'host=1'),
                    MATERIAL.replace(b'19072', b'70000'), MATERIAL.replace(b'19072', b'19073'),
                    MATERIAL.replace(b'abcdef123456', b'a\nEthApi=Sockets')):
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError): connection(bad)
        for bad in (b'[DEV9/Eth]\n[DEV9/Eth]\n', b'\0', b'a' * (1024*1024+1)):
            with self.assertRaises(ValueError): overlay(bad, MATERIAL)

    def test_failure_does_not_create_partial_configuration(self):
        with tempfile.TemporaryDirectory() as root:
            target = Path(root).resolve() / 'session.ini'
            with self.assertRaises(ValueError): prepare(None, target, b'bad')
            self.assertFalse(target.exists())


if __name__ == '__main__':
    unittest.main()
