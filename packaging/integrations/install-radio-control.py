#!/usr/bin/python3 -I
"""Add bounded radio privileges; preserve other system/user configuration."""
import argparse
import os
from pathlib import Path
import pwd
import re
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--user', required=True)
a = p.parse_args()
if os.geteuid() != 0 or not re.fullmatch(r'[a-z_][a-z0-9_-]*', a.user) or pwd.getpwnam(a.user).pw_uid < 1000:
    p.error('Root and a regular local user are required')
root = Path('/var/opt/traineros/integrations')
root.mkdir(parents=True, exist_ok=True)
if root.stat().st_uid != 0 or root.stat().st_mode & 0o022:
    raise RuntimeError('Unsafe helper directory')
helper = root/'radio-control.py'
verbs = [kind+'-'+state for kind in ('wifi','bluetooth','airplane') for state in ('on','off')]
policy = a.user+' ALL=(root) NOPASSWD: '+', '.join(str(helper)+' '+verb for verb in verbs)+'\n'
policy += a.user+' ALL=(root) NOPASSWD: '+str(root/'network-control.py')+' ""\n'
policy += a.user+' ALL=(root) NOPASSWD: '+str(root/'nearby-control.py')+' ""\n'
with tempfile.NamedTemporaryFile() as check:
    check.write(policy.encode()); check.flush()
    subprocess.run(['/usr/sbin/visudo','-cf',check.name],check=True,stdout=subprocess.DEVNULL)
backup = Path('/var/lib/traineros/radio-backup')
backup.mkdir(parents=True, exist_ok=True, mode=0o700)
for destination, data, mode in [(helper, Path(__file__).with_name('radio-control.py').read_bytes(), 0o755),
                                (root/'network-control.py', Path(__file__).with_name('network-control.py').read_bytes(), 0o755),
                                (root/'nearby-bluetooth.py', Path(__file__).with_name('nearby-bluetooth.py').read_bytes(), 0o644),
                                (root/'nearby-control.py', Path(__file__).with_name('nearby-control.py').read_bytes(), 0o755),
                                (Path('/etc/sudoers.d/traineros-radio'), policy.encode(), 0o440)]:
    previous = backup/destination.name
    if destination.exists() and not previous.exists():
        shutil.copy2(destination, previous)
    fd, temporary = tempfile.mkstemp(dir=destination.parent)
    with os.fdopen(fd,'wb') as output:
        output.write(data.replace(b'\r\n',b'\n')); output.flush(); os.fchmod(output.fileno(),mode); os.fsync(output.fileno())
    os.replace(temporary,destination)
print('Radio controls installed; radio state unchanged')
