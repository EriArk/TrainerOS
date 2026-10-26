#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import tomllib
import unittest

spec = importlib.util.spec_from_file_location('session', Path(__file__).with_name('session.py'))
session = importlib.util.module_from_spec(spec)
spec.loader.exec_module(session)


class SessionChecks(unittest.TestCase):
    def test_native_unicode_and_inline_settings_roundtrip(self):
        value = {'general': {'screenshot_dir': '/games/Игры/🎮'},
                 'input': {'bindings': {'port1': 'Pad "one"'}},
                 'net': {'ports': [{'host': 2121, 'guest': 21}]}}
        self.assertEqual(tomllib.loads(session.toml(value)), value)

    def test_preserves_keys_settings_and_hdd_changes_only_session_mac(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            original = bytearray(range(256))
            struct.pack_into('<I', original, 0x30, session.checksum(original[0x34:0x60]))
            files = {}
            for key in ('bootrom_path', 'flashrom_path', 'hdd_path', 'eeprom_path'):
                p = root / key
                p.write_bytes(original if key == 'eeprom_path' else b'owned-data')
                files[key] = str(p)
            value = {'sys': {'files': files}, 'display': {'renderer': 'VULKAN'},
                     'input': {'bindings': {'port1': 'own controller'}},
                     'net': {'enable': True, 'backend': 'nat', 'nat': {'forward_ports': [{'host': 2121, 'guest': 21, 'protocol': 'tcp'}]}}}
            config = root / 'original.toml'
            config.write_text(session.toml(value))
            before = {p.name: p.read_bytes() for p in root.iterdir()}
            for seat in (1, 2, 3, 4):
                stage = root / str(seat)
                stage.mkdir()
                session.prepare(config, stage, f'02:50:f2:ab:cd:{seat:02x}')
                output = (stage / 'eeprom.bin').read_bytes()
                for i in range(256):
                    if i not in range(0x30, 0x34) and i not in range(0x40, 0x46):
                        self.assertEqual(output[i], original[i])
                self.assertEqual(output[0x40:0x46], bytes.fromhex(f'0250f2abcd{seat:02x}'))
                self.assertEqual(struct.unpack_from('<I', output, 0x30)[0], session.checksum(output[0x34:0x60]))
                decoded = tomllib.loads((stage / 'xemu.toml').read_text())
                expected = json.loads(json.dumps(value))
                expected['net']['enable'] = False
                expected['sys']['files']['eeprom_path'] = str(stage / 'eeprom.bin')
                self.assertEqual(decoded, expected)
            self.assertEqual(before, {n: (root / n).read_bytes() for n in before})

    def test_rejects_corrupt_eeprom_and_reused_session(self):
        with self.assertRaises(ValueError):
            session.private_eeprom(bytes(256), bytes.fromhex('0250f2abcd01'))
        with self.assertRaises(ValueError):
            session.private_eeprom(bytes(255), bytes.fromhex('0250f2abcd01'))


if __name__ == '__main__':
    unittest.main()
