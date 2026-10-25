"""Bounded acceptance of this Compose project only; no user/game requests."""
import json
from pathlib import Path
import subprocess
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parent


def compose(*args):
    return subprocess.check_output(['docker', 'compose', *args], cwd=ROOT, text=True, stdin=subprocess.DEVNULL)


def get(path):
    try:
        with urllib.request.urlopen('http://127.0.0.1:19120' + path, timeout=6) as response:
            return response.status, json.load(response)
    except urllib.error.HTTPError as exc:
        return exc.code, json.load(exc)


def wait_ready():
    for _ in range(30):
        try:
            if get('/health/ready')[0] == 200:
                return
        except (OSError, ValueError):
            pass
        time.sleep(1)
    raise RuntimeError('readiness_timeout')


wait_ready()
assert get('/v1/capabilities')[1]['capabilities'] == {}
assert get('/v1/me/purchases')[0] == 404
try:
    compose('stop', 'db')
    assert get('/health/live')[0] == 200
    assert get('/health/ready')[0] == 503
finally:
    compose('up', '-d', '--wait', 'db')
wait_ready()
compose('restart', 'api', 'worker')
wait_ready()
api = compose('ps', '-q', 'api').strip()
metadata = json.loads(subprocess.check_output(['docker', 'inspect', api], text=True))[0]
assert metadata['HostConfig']['PortBindings']['19120/tcp'] == [{'HostIp': '127.0.0.1', 'HostPort': '19120'}]
assert metadata['Config']['User'] == '10001:10001'
assert metadata['HostConfig']['ReadonlyRootfs']
print(json.dumps({'ready': get('/health/ready')[1], 'database_outage_503': True,
    'recovered_after_database_restart': True, 'api_worker_restarted': True,
    'loopback_only': True, 'non_root_readonly': True, 'image': metadata['Image']}))
