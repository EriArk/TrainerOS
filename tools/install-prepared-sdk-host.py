#!/usr/bin/env python3
"""Bounded, owner-authorized native host delivery; preserve the old executable and DB.

STAGE contains traineros and manifest.json with sha256 and expected_old_sha256.
Uses the existing session supervisor. Does not change boot policy or accounts.
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import signal
import socket
import sqlite3
import subprocess
import sys
import time

stage = Path(sys.argv[1]).resolve()
manifest = json.loads((stage/'manifest.json').read_text())
target = Path.home()/'.local/bin/traineros'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert digest(stage/'traineros') == manifest['sha256']
assert digest(target) == manifest['expected_old_sha256'], 'Installed host changed since preparation'
ids = subprocess.check_output(['pgrep', '-x', 'traineros'], text=True).split()
assert len(ids) == 1
old_pid = int(ids[0])
assert digest(Path('/proc')/str(old_pid)/'exe') == manifest['expected_old_sha256']
for proc in Path('/proc').glob('[0-9]*'):
    try:
        env = (proc/'environ').read_bytes().split(b'\0')
        name = (proc/'exe').readlink().name.lower()
    except OSError:
        continue
    assert not any(x in env for x in [b'TRAINEROS_PC_MANAGED=1', b'TRAINEROS_CRATES_GAME=1']), 'Managed game is active'
    assert not any(x in name for x in ['retroarch', 'ppsspp', 'dolphin-emu', 'armsx2', 'vita3k', 'flycast']), 'Emulator is active'

backup = stage/'before'
backup.mkdir(mode=0o700)
shutil.copy2(target, backup/'traineros')
settings = Path.home()/'.config/TrainerOS/TrainerOS.conf'
if settings.exists():
    shutil.copy2(settings, backup/'settings.conf')
db = Path.home()/'.local/share/TrainerOS/TrainerOS/traineros.sqlite3'
with sqlite3.connect(str(db)) as source, sqlite3.connect(str(backup/'traineros.sqlite3')) as saved:
    source.backup(saved)
    assert saved.execute('pragma quick_check').fetchone()[0] == 'ok'

candidate = target.with_name('traineros-crates26-new')
shutil.copy2(stage/'traineros', candidate)
candidate.chmod(0o755)
check = subprocess.run(['ldd', str(candidate)], capture_output=True, text=True)
assert check.returncode == 0 and 'not found' not in check.stdout, check.stdout
with candidate.open('rb') as stream:
    os.fsync(stream.fileno())
os.replace(candidate, target)
os.kill(old_pid, signal.SIGTERM)

try:
    for _ in range(45):
        time.sleep(1)
        ids = subprocess.run(['pgrep', '-x', 'traineros'], capture_output=True, text=True).stdout.split()
        if len(ids) == 1 and int(ids[0]) != old_pid:
            new_pid = int(ids[0])
            assert digest(Path('/proc')/str(new_pid)/'exe') == manifest['sha256']
            time.sleep(15)
            assert Path('/proc', str(new_pid)).exists()
            break
    else:
        raise RuntimeError('Session supervisor did not restart the host')
    endpoint = json.loads((Path.home()/'.local/share/traineros/platform-v1/endpoint.json').read_text())
    with socket.create_connection((endpoint['host'], endpoint['port']), timeout=5) as peer:
        packet = {'id':'verify', 'method':'hello', 'params':{'protocol':1, 'token':endpoint['token'], 'app':'org.traineros.delivery-check'}}
        peer.sendall((json.dumps(packet)+'\n').encode())
        capabilities = json.loads(peer.makefile().readline())['result']['capabilities']
    assert all(capabilities.get(k) == 1 for k in ['network.group', 'play.scorecards', 'account.identity'])
    report = {'sha256':manifest['sha256'], 'previous_sha256':manifest['expected_old_sha256'],
              'supervised_restart':True, 'capabilities':capabilities, 'backup_retained':True}
    (stage/'installation.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report))
except Exception:
    rollback = target.with_name('traineros-crates26-rollback')
    shutil.copy2(backup/'traineros', rollback)
    os.replace(rollback, target)
    ids = subprocess.run(['pgrep', '-x', 'traineros'], capture_output=True, text=True).stdout.split()
    for pid in ids:
        if int(pid) != old_pid:
            os.kill(int(pid), signal.SIGTERM)
    raise
