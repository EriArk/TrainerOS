"""Bounded installation repairs must preserve user data and reject collisions."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Repairs(unittest.TestCase):
    def run_repair(self, script, *args, ok=True):
        result = subprocess.run([sys.executable, str(ROOT / 'packaging/emulators' / script),
                                 *map(str, args)], capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, ok, result.stdout + result.stderr)

    def test_firmware_copy_and_idempotence(self):
        with tempfile.TemporaryDirectory() as folder:
            base = Path(folder)
            source = base / 'source.bin'; source.write_bytes(b'F' * 262144)
            config = base / 'melonDS.toml'
            original = 'FirmwarePath = ' + json.dumps(str(source)) + '\nOther = 7\n'
            config.write_text(original)
            args = ('--config', config, '--directory', base / 'private')
            self.run_repair('melonds/make-firmware-writable.py', *args)
            self.run_repair('melonds/make-firmware-writable.py', *args)
            self.assertEqual((base/'private/firmware.bin').read_bytes(), source.read_bytes())
            self.assertEqual(config.with_name(config.name+'.before-launch80-firmware').read_text(), original)
            self.assertIn('Other = 7', config.read_text())

    def test_differing_working_firmware_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as folder:
            base = Path(folder); (base/'private').mkdir()
            source = base/'source.bin'; source.write_bytes(b'F'*262144)
            target = base/'private/firmware.bin'; target.write_bytes(b'U'*262144)
            config = base/'melonDS.toml'; config.write_text('FirmwarePath = '+json.dumps(str(source))+'\n')
            original = config.read_bytes()
            self.run_repair('melonds/make-firmware-writable.py', '--config', config,
                            '--directory', target.parent, ok=False)
            self.assertEqual(config.read_bytes(), original)
            self.assertEqual(target.read_bytes(), b'U'*262144)

    def test_disc_registration_preserves_profile_and_refuses_changed_identity(self):
        with tempfile.TemporaryDirectory() as folder:
            base = Path(folder)
            for region in 'UEJ': (base/f'bios_CD_{region}.bin').write_bytes(region.encode()*131072)
            profile = base/'profile.json'; profile.write_text(json.dumps({'version':1,'user':{'keep':True}}))
            args = ('--profile',profile,'--system-directory',base)
            self.run_repair('retroarch/register-disc-firmware.py',*args)
            first = profile.read_bytes()
            self.run_repair('retroarch/register-disc-firmware.py',*args)
            self.assertEqual(profile.read_bytes(), first)
            self.assertEqual(json.loads(first)['user'], {'keep':True})
            (base/'bios_CD_U.bin').write_bytes(b'Z'*131072)
            self.run_repair('retroarch/register-disc-firmware.py',*args,ok=False)
            self.assertEqual(profile.read_bytes(),first)

    def test_dolphin_only_replaces_stale_device_and_preserves_bindings(self):
        with tempfile.TemporaryDirectory() as folder:
            base=Path(folder); config=base/'GCPadNew.ini'
            original=b'[GCPad1]\nDevice = SDL/0/Steam Virtual Gamepad\nButtons/A = `Button S`\n'
            config.write_bytes(original)
            args=(base,'--device','SDL/0/Verified Pad')
            self.run_repair('dolphin/rebind-stale-input.py',*args)
            first=config.read_bytes()
            self.run_repair('dolphin/rebind-stale-input.py',*args)
            self.assertEqual(config.read_bytes(),first)
            self.assertIn(b'Buttons/A = `Button S`',first)
            self.assertEqual(config.with_suffix('.ini.before-launch80').read_bytes(),original)


if __name__ == '__main__':
    unittest.main()
