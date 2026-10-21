"""Check SDK readiness at discovery in an ordinary shell, without smoke flags."""
from pathlib import Path
import argparse
import os
import subprocess
import sys
import tempfile
import time

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'sdk/python'))
from traineros import Client, HostError

parser = argparse.ArgumentParser()
parser.add_argument('executable', type=Path)
parser.add_argument('--production', action='store_true')
parser.add_argument('--network-python')
parser.add_argument('--network-helper')
args = parser.parse_args()
exe = str(args.executable.resolve())
env = dict(os.environ, QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
expected = {'context', 'lifecycle', 'ui.start.widget'}
if args.network_python and args.network_helper:
    env['TRAINEROS_RECORD_PYTHON'] = str(Path(args.network_python).resolve())
    env['TRAINEROS_RECORD_HELPER'] = str(Path(args.network_helper).resolve())
    expected |= {'lobby.activity', 'network.records'}
help_result = subprocess.run([exe, '--help'], env=env, capture_output=True, text=True, timeout=15)
assert help_result.returncode == 0
assert '--ephemeral' in help_result.stdout
if args.production:
    assert 'platform-smoke-test' not in help_result.stdout
with tempfile.TemporaryDirectory(prefix='trainer-sdk-startup-') as directory:
    endpoint = Path(directory) / 'endpoint.json'
    env['TRAINEROS_SDK_ENDPOINT'] = str(endpoint)
    with (Path(directory) / 'host.log').open('w') as log:
        process = subprocess.Popen([exe, '--ephemeral', '--windowed'], env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 30
            while not endpoint.exists() and time.monotonic() < deadline and process.poll() is None:
                time.sleep(0.1)
            assert endpoint.exists(), 'Normal host did not publish its SDK endpoint'
            # No extra startup sleep or retry: discovery must mean a responsive
            # API, not a socket waiting for a still-blocked GUI thread.
            with Client('org.test.startup', endpoint) as host:
                assert set(host.capabilities) == expected
                assert host.subscribe()['profile'] == ''
                try:
                    host.widget('sample', 'Sample')
                    raise AssertionError('A personal widget without a profile was accepted')
                except HostError as error:
                    assert error.code == 'no_profile'
                if args.network_python:
                    try:
                        host.call('activity.register', title='No profile', protocol='v1')
                        raise AssertionError('Personal activity without a profile was accepted')
                    except HostError as error:
                        assert error.code == 'no_profile'
            print('Normal host: discovery readiness, handshake, context and no-profile gate passed')
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
