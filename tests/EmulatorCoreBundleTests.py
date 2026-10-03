"""Real-file update/rollback and interrupted-update coverage; no emulator needed."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('core_bundle', Path(__file__).resolve().parents[1] / 'tools/emulator-core-bundle.py')
bundle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundle)


class CoreBundleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'; self.source.mkdir()
        self.target = self.root / 'target'; self.target.mkdir()
        self.package = self.root / 'bundle'
        self.state = self.root / 'journal'
        self.core(self.source / 'alpha_libretro.so', b'new-a')
        self.core(self.source / 'beta_libretro.so', b'new-b')
        self.core(self.target / 'alpha_libretro.so', b'old-a')
        self.old = (self.target / 'alpha_libretro.so').read_bytes()
        (self.target / 'personal.sav').write_bytes(b'untouched-save')
        bundle.capture(self.source, self.package, 'test-1')

    @staticmethod
    def core(path, payload):
        header = bytearray(64); header[:6] = b'\x7fELF\x02\x01'
        struct.pack_into('<H', header, 18, 183)
        path.write_bytes(header + payload)

    def apply(self):
        with patch.object(bundle, 'idle'):
            return bundle.apply(self.package, self.target, self.state)

    def rollback(self):
        with patch.object(bundle, 'idle'):
            return bundle.rollback(self.state, self.target)

    def test_update_verify_rollback_preserves_unmanaged_files(self):
        self.assertEqual(self.apply()['changed'], 2)
        self.assertEqual(bundle.verify(self.package, self.target)['different'], [])
        self.rollback()
        self.assertEqual((self.target / 'alpha_libretro.so').read_bytes(), self.old)
        self.assertFalse((self.target / 'beta_libretro.so').exists())
        self.assertEqual((self.target / 'personal.sav').read_bytes(), b'untouched-save')
        self.rollback()  # Recovery can be retried safely.

    def test_corrupt_artifact_changes_nothing(self):
        (self.package / 'beta_libretro.so').write_bytes(b'broken')
        with self.assertRaises(ValueError): self.apply()
        self.assertEqual((self.target / 'alpha_libretro.so').read_bytes(), self.old)
        self.assertFalse((self.state / 'transaction.json').exists())

    def test_interrupted_apply_has_durable_recovery(self):
        replace = bundle.replace_file
        def crash(source, target, expected):
            if target.name == 'beta_libretro.so': raise OSError('simulated interruption')
            replace(source, target, expected)
        with patch.object(bundle, 'replace_file', side_effect=crash):
            with self.assertRaises(OSError): self.apply()
        self.assertEqual(json.loads((self.state / 'transaction.json').read_text())['status'], 'applying')
        with self.assertRaises(RuntimeError): self.apply()
        self.rollback()
        self.assertEqual((self.target / 'alpha_libretro.so').read_bytes(), self.old)

    def test_rollback_preserves_later_user_change(self):
        self.apply()
        self.core(self.target / 'beta_libretro.so', b'user-update')
        with self.assertRaises(ValueError): self.rollback()
        self.assertEqual((self.target / 'alpha_libretro.so').read_bytes(), (self.source / 'alpha_libretro.so').read_bytes())

    def test_running_emulator_refuses_update(self):
        with patch.object(bundle, 'idle', side_effect=RuntimeError('Close RetroArch')):
            with self.assertRaises(RuntimeError): bundle.apply(self.package, self.target, self.state)
        self.assertEqual((self.target / 'alpha_libretro.so').read_bytes(), self.old)

    def test_path_traversal_and_duplicate_names_rejected(self):
        manifest = self.package / 'manifest.json'
        original = json.loads(manifest.read_text())
        altered = json.loads(manifest.read_text()); altered['cores'][0]['name'] = '../alpha_libretro.so'
        manifest.write_text(json.dumps(altered))
        with self.assertRaises(ValueError): self.apply()
        original['cores'].append(original['cores'][0]); manifest.write_text(json.dumps(original))
        with self.assertRaises(ValueError): self.apply()

    def test_target_symlink_rejected(self):
        (self.target / 'beta_libretro.so').symlink_to(self.source / 'beta_libretro.so')
        with self.assertRaises(ValueError): self.apply()
        self.assertEqual((self.target / 'alpha_libretro.so').read_bytes(), self.old)

    def test_backup_symlink_rejected(self):
        self.state.mkdir()
        (self.state / 'before').symlink_to(self.source, target_is_directory=True)
        with self.assertRaises(ValueError): self.apply()
        self.assertEqual((self.target / 'alpha_libretro.so').read_bytes(), self.old)


if __name__ == '__main__': unittest.main()
