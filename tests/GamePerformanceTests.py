"""No host access: exercise host rounding, partial failures and recovery journal."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import unittest.mock
import io
import json
import subprocess
import sys

spec = importlib.util.spec_from_file_location('power', Path(__file__).parents[1] / 'packaging/integrations/game-performance.py')
power = importlib.util.module_from_spec(spec)
spec.loader.exec_module(power)


class Host:
    name = 'org.armada.Power'
    profile_key = 'Profile'
    def __init__(self):
        self.gov = {'Eco': 'conservative', 'Balanced': 'schedutil', 'Performance': 'performance'}
        self.s = dict(Profile='Balanced', AvailableProfiles=list(self.gov),
                      AvailableCpuScalingGovernors=['conservative', 'schedutil', 'performance'],
                      GpuPerformanceLevel='auto', AvailableGpuPerformanceLevels=['auto', 'manual'],
                      ManualGpuClock=800, ManualGpuClockMin=200, ManualGpuClockMax=1000)
        self.fail = ''

    def read(self):
        return dict(self.s, CpuScalingGovernor=self.gov[self.s['Profile']])

    def write(self, key, value):
        if self.fail == key:
            self.fail = ''
            raise RuntimeError('host refused write')
        if key == 'CpuScalingGovernor':
            self.gov[self.s['Profile']] = value
        else:
            self.s[key] = round(value / 100) * 100 if key == 'ManualGpuClock' else value


class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.host = Host()
        self.journal = Path(self.temp.name) / 'restore.json'
        self.lease = power.Lease(self.host, self.journal)

    def tearDown(self):
        self.temp.cleanup()

    def test_full_lease_restores_target_governor_and_original_mode(self):
        before = self.host.read()
        governors = self.host.gov.copy()
        self.lease.apply(dict(power='Performance', governor='schedutil', gpu=455))
        self.assertEqual(self.host.read()['ManualGpuClock'], 500)
        self.lease.release()
        self.assertEqual(before, self.host.read())
        self.assertEqual(governors, self.host.gov)
        self.assertFalse(self.journal.exists())

    def test_invalid_override_does_not_partly_change_power(self):
        before = self.host.read()
        with self.assertRaises(ValueError):
            self.lease.apply(dict(power='Eco', gpu=9999))
        self.assertEqual(before, self.host.read())

    def test_profile_with_sticky_global_governor_restores_baseline(self):
        original = self.host.write
        def sticky(key, value):
            governor = self.host.read()['CpuScalingGovernor']
            original(key, value)
            if key == 'Profile':
                self.host.gov[value] = 'conservative' if value == 'Eco' else governor
                self.host.s['GpuPerformanceLevel'] = 'auto'
        with unittest.mock.patch.object(self.host, 'write', side_effect=sticky):
            self.host.s['GpuPerformanceLevel'] = 'manual'
            before = self.host.read()
            self.lease.update({'power': 'Eco'})
            self.lease.update({'power': 'Balanced', 'gpu': 400, 'governor': 'schedutil'})
            self.lease.update({})
            self.assertEqual(before, self.host.read())
            self.lease.update({'power': 'Eco'})
            power.Lease(self.host, self.journal).recover()
            self.assertEqual(before, self.host.read())

    def test_failed_governor_restore_is_recovered_after_profile_switch(self):
        before = self.host.read()
        self.lease.apply({'power': 'Eco'})
        # Some hosts keep the new governor when selecting Balanced again.
        self.host.gov['Balanced'] = 'conservative'
        self.host.fail = 'CpuScalingGovernor'
        with self.assertRaises(RuntimeError):
            self.lease.release()
        self.assertTrue(self.journal.exists())
        power.Lease(self.host, self.journal).recover()
        self.assertEqual(before, self.host.read())

    def test_partial_failure_restores_changes(self):
        before = self.host.read()
        self.host.fail = 'GpuPerformanceLevel'
        with self.assertRaises(RuntimeError):
            self.lease.apply(dict(power='Performance', governor='schedutil', gpu=400))
        self.lease.release()
        self.assertEqual(before, self.host.read())

    def test_recovery_after_helper_loss(self):
        before = self.host.read()
        self.lease.apply(dict(power='Performance', gpu=400))
        power.Lease(self.host, self.journal).recover()
        self.assertEqual(before, self.host.read())

    def test_external_profile_change_never_modifies_its_governor(self):
        self.lease.apply(dict(power='Performance', governor='schedutil'))
        self.host.write('Profile', 'Eco')
        self.lease.release()
        self.assertEqual(self.host.read()['Profile'], 'Eco')
        self.assertEqual(self.host.gov['Eco'], 'conservative')

    def test_restore_failure_keeps_journal_for_retry(self):
        self.lease.apply(dict(power='Performance'))
        self.host.fail = 'Profile'
        with self.assertRaises(RuntimeError):
            self.lease.release()
        self.assertTrue(self.journal.exists())
        power.Lease(self.host, self.journal).recover()
        self.assertEqual(self.host.read()['Profile'], 'Balanced')

    def test_external_clock_change_keeps_its_manual_mode(self):
        self.lease.apply(dict(power='Performance', gpu=400))
        self.host.write('ManualGpuClock', 700)
        self.lease.release()
        self.assertEqual(self.host.read()['ManualGpuClock'], 700)
        self.assertEqual(self.host.read()['GpuPerformanceLevel'], 'manual')
        self.assertEqual(self.host.read()['Profile'], 'Performance')

    def test_interrupted_rollback_is_idempotent(self):
        self.lease.apply(dict(power='Performance', gpu=400))
        original = self.host.write
        def failed_after_write(key, value):
            original(key, value)
            if key == 'Profile' and value == 'Balanced':
                raise RuntimeError('connection lost after host applied the write')
        with unittest.mock.patch.object(self.host, 'write', side_effect=failed_after_write):
            with self.assertRaises(RuntimeError):
                self.lease.release()
        power.Lease(self.host, self.journal).recover()
        self.assertEqual(self.host.read()['Profile'], 'Balanced')
        self.assertEqual(self.host.read()['ManualGpuClock'], 800)

    def test_profile_cap_does_not_lose_original_manual_clock(self):
        self.host.s.update(Profile='Performance', ManualGpuClock=900, GpuPerformanceLevel='manual')
        original = self.host.read
        def capped():
            state = original()
            if state['Profile'] == 'Eco':
                state['ManualGpuClock'] = min(600, state['ManualGpuClock'])
                state['ManualGpuClockMax'] = 600
            return state
        with unittest.mock.patch.object(self.host, 'read', side_effect=capped):
            before = self.host.read()
            self.lease.apply(dict(power='Eco', gpu=400))
            self.lease.release()
            self.assertEqual(before, self.host.read())

    def test_unsupported_host_exposes_no_made_up_clocks(self):
        host = Host()
        host.name, host.profile_key = 'net.hadess.PowerProfiles', 'ActiveProfile'
        host.read = lambda: dict(ActiveProfile='balanced', Profiles=[{'Profile': 'balanced'}])
        self.assertEqual(power.capabilities(host)['gpuModes'], [])
        self.assertEqual(power.plan(host, dict(power='balanced')), [('ActiveProfile', 'balanced')])

    def test_repeated_live_updates_and_reset_retain_original_baseline(self):
        before, governors = self.host.read(), self.host.gov.copy()
        self.lease.update(dict(power='Performance', governor='schedutil', gpu=400))
        self.lease.update(dict(power='Eco', governor='performance', gpu=600))
        self.assertEqual(self.host.read()['Profile'], 'Eco')
        self.assertEqual(self.host.gov['Performance'], governors['Performance'])
        self.lease.update({})
        self.assertEqual(before, self.host.read())
        self.assertEqual(governors, self.host.gov)
        self.lease.update(dict(gpu=500))  # default-started sessions can acquire overrides later
        self.lease.release()
        self.assertEqual(before, self.host.read())

    def test_invalid_live_update_keeps_active_settings(self):
        self.lease.update(dict(power='Performance', gpu=500))
        before = self.host.read()
        with self.assertRaises(ValueError):
            self.lease.update(dict(power='Eco', gpu=9999))
        self.assertEqual(before, self.host.read())

    def test_refused_live_write_restores_previous_profile_then_exit_baseline(self):
        before = self.host.read()
        self.lease.update(dict(power='Performance'))
        self.host.fail = 'GpuPerformanceLevel'
        with self.assertRaisesRegex(RuntimeError, 'previous settings restored'):
            self.lease.update(dict(power='Eco', gpu=500))
        self.assertEqual(self.host.read()['Profile'], 'Performance')
        self.assertEqual(self.host.read()['GpuPerformanceLevel'], 'auto')
        self.lease.release()
        self.assertEqual(before, self.host.read())

    def test_compatibility_only_update_does_not_cycle_host(self):
        self.lease.update(dict(power='Performance', gpu=500))
        with unittest.mock.patch.object(self.host, 'write') as write:
            self.lease.update(dict(power='Performance', gpu=500, renderer='opengl'))
            write.assert_not_called()

    def test_external_changes_become_baseline_for_explicit_live_update(self):
        self.lease.update(dict(power='Performance', gpu=500))
        self.host.write('ManualGpuClock', 700)
        external = self.host.read()
        self.lease.update(dict(power='Eco', gpu=400))
        self.lease.release()
        self.assertEqual(external, self.host.read())

    def test_initially_equal_override_still_notices_external_change(self):
        self.lease.update(dict(power='Balanced'))
        self.host.write('Profile', 'Eco')
        self.lease.update(dict(power='Balanced'))
        self.assertEqual(self.host.read()['Profile'], 'Balanced')
        self.lease.release()
        self.assertEqual(self.host.read()['Profile'], 'Eco')

    def test_failed_rollback_cannot_be_skipped_by_reset_retry(self):
        self.lease.update(dict(power='Performance'))
        self.host.fail = 'Profile'
        with self.assertRaises(RuntimeError):
            self.lease.update({})
        self.assertTrue(self.journal.exists())
        self.lease.update({})
        self.assertEqual(self.host.read()['Profile'], 'Balanced')
        self.assertFalse(self.journal.exists())

    def test_pipe_rejects_bad_request_and_continues_until_eof(self):
        replies = []
        power.serve(self.lease, io.BytesIO(b'[]\n{"power":"Eco"}\n'), replies.append)
        self.assertIn('error', replies[0])
        self.assertTrue(replies[1]['applied'])
        self.assertEqual(self.host.read()['Profile'], 'Eco')
        with self.assertRaises(ValueError):
            power.serve(self.lease, io.BytesIO(b'x' * 8193), replies.append)

    def test_real_inherited_pipe_updates_then_eof_restores_without_game_restart(self):
        script = """
import runpy, sys
from pathlib import Path
ns = runpy.run_path(sys.argv[1], run_name='fixture')
power = ns['power']; host = ns['Host'](); lease = power.Lease(host, Path(sys.argv[2]))
try:
    power.serve(lease, sys.stdin.buffer, power.emit)
finally:
    lease.release()
    power.emit(dict(restored=host.read()))
"""
        result = subprocess.run([sys.executable, '-u', '-c', script, str(Path(__file__).resolve()), str(self.journal)],
                                input=b'{"power":"Performance","gpu":400}\n{"power":"Eco"}\n',
                                capture_output=True, timeout=10, check=True)
        replies = [json.loads(line) for line in result.stdout.splitlines()]
        self.assertEqual(len(replies), 3)
        self.assertTrue(replies[0]['applied'])
        self.assertEqual(replies[1]['capabilities']['current'], 'Eco')
        self.assertEqual(replies[2]['restored'], self.host.read())
        self.assertFalse(self.journal.exists())


if __name__ == '__main__':
    unittest.main()
