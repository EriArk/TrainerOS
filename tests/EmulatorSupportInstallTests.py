import hashlib
import importlib.util
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

source = Path(__file__).resolve().parents[1] / 'packaging/integrations/emulator_support.py'
spec = importlib.util.spec_from_file_location('support', source)
support = importlib.util.module_from_spec(spec); spec.loader.exec_module(support)


class EmulatorSupportInstallTests(unittest.TestCase):
    def test_stock_upgrade_keeps_original_and_custom_bridge(self):
        with tempfile.TemporaryDirectory() as directory:
            account = SimpleNamespace(pw_dir=directory, pw_uid=os.getuid(), pw_gid=os.getgid())
            path = Path(directory) / '.local/libexec/traineros/controller-bridge.py'
            path.parent.mkdir(parents=True); path.write_bytes(b'old bridge')
            with patch.object(support, 'LEGACY_BRIDGE', hashlib.sha256(b'old bridge').hexdigest()):
                self.assertTrue(support.migrate_stock_bridge(account, b'new bridge'))
                self.assertFalse(support.migrate_stock_bridge(account, b'another bridge'))
                self.assertEqual(path.read_bytes(), b'new bridge')
                backup = path.with_name('controller-bridge.before-guarded-home.py')
                self.assertEqual(backup.read_bytes(), b'old bridge')
                path.write_bytes(b'personal bridge')
                self.assertFalse(support.migrate_stock_bridge(account, b'new bridge'))
                self.assertEqual(path.read_bytes(), b'personal bridge')
                path.unlink(); path.symlink_to(backup)
                self.assertFalse(support.migrate_stock_bridge(account, b'new bridge'))
                self.assertEqual(backup.read_bytes(), b'old bridge')

    def test_missing_prerequisite_does_not_start_installation(self):
        writes = []
        with patch.object(support, 'prerequisites', side_effect=RuntimeError('missing dependency')):
            with self.assertRaises(RuntimeError):
                support.install_support(None, lambda *args: writes.append(args))
        self.assertEqual(writes, [])


if __name__ == '__main__': unittest.main()
