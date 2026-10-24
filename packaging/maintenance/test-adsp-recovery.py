#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
from pathlib import Path
import tempfile
import unittest
import unittest.mock

spec = importlib.util.spec_from_file_location('restore_adsp', Path(__file__).with_name('restore-adsp.py'))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class AdspRecoveryTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        self.procs = root / 'remoteproc'
        self.device = self.procs / 'remoteproc0'
        self.device.mkdir(parents=True)
        self.firmware = root / 'firmware'
        (self.firmware / 'qcom/test').mkdir(parents=True)
        (self.firmware / 'qcom/test/adsp.mbn').write_bytes(b'fixture')
        self.write('name', 'adsp')
        self.write('state', 'offline')
        self.write('firmware', 'qcom/test/adsp.mbn')
        self.patch = unittest.mock.patch.object(Path, 'resolve', return_value=Path('/drivers/qcom_q6v5_pas'))
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def write(self, name, value):
        (self.device / name).write_text(value)

    def run_recovery(self):
        module.restore(self.procs, self.firmware)

    def test_firmware_available_after_root_switch_starts_offline_adsp(self):
        self.run_recovery()
        self.assertEqual((self.device / 'state').read_text(), 'start\n')

    def test_running_crashed_and_other_processors_are_untouched(self):
        for state in ['running', 'crashed', 'suspended']:
            self.write('state', state)
            self.run_recovery()
            self.assertEqual((self.device / 'state').read_text(), state)
        self.write('state', 'offline')
        self.write('name', 'cdsp')
        self.run_recovery()
        self.assertEqual((self.device / 'state').read_text(), 'offline')

    def test_missing_or_unsafe_firmware_does_not_start(self):
        for firmware in ['qcom/missing.mbn', '/tmp/foreign', 'qcom/../foreign']:
            self.write('firmware', firmware)
            with self.assertRaises(RuntimeError):
                self.run_recovery()
            self.assertEqual((self.device / 'state').read_text(), 'offline')


if __name__ == '__main__':
    unittest.main()
