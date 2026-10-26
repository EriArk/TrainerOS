# SPDX-License-Identifier: GPL-3.0-or-later
"""Recoverable ownership journal for the additive, managed session files."""
import hashlib
import json
import os
from pathlib import Path
import tempfile

EXACT = {'/etc/systemd/system/inputplumber.service.d/90-traineros-pad-read.conf', '/etc/polkit-1/rules.d/50-traineros-clock.rules', '/etc/systemd/user/traineros-session.service',
         '/etc/sudoers.d/traineros-session', '/etc/sudoers.d/traineros-power', '/etc/sddm.conf.d/90-traineros.conf',
         '/etc/traineros/wayland-sessions/traineros.desktop'}


class Ownership:
    def __init__(self, journal, user, filesystem=Path('/')):
        self.path, self.filesystem = Path(journal), Path(filesystem)
        if self.path.is_symlink(): raise ValueError('Redirected ownership journal')
        self.data = json.loads(self.path.read_text()) if self.path.exists() else {'user': user, 'files': {}}
        if self.data['user'] != user: raise ValueError('Session belongs to another local account')

    def target(self, name):
        if '..' in name.split('/') or not (name in EXACT or name.startswith('/var/opt/traineros/integrations/') or name.startswith('/var/opt/traineros/session/')):
            raise ValueError('Unexpected owned session path')
        path = self.filesystem / name.lstrip('/')
        if path.is_symlink() or any(p.is_symlink() for p in path.parents): raise ValueError('Redirected session file')
        return path

    def save(self):
        self.path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        fd, temporary = tempfile.mkstemp(dir=self.path.parent)
        try:
            with os.fdopen(fd, 'w') as stream:
                json.dump(self.data, stream); stream.flush(); os.fsync(stream.fileno())
            os.replace(temporary, self.path)
            if os.name != 'nt':
                directory = os.open(self.path.parent, os.O_RDONLY)
                try: os.fsync(directory)
                finally: os.close(directory)
        finally:
            if os.path.exists(temporary): os.unlink(temporary)

    def check(self, name):
        path = self.target(name)
        known = self.data['files'].get(name, [])
        if isinstance(known, str): known = [known]
        if path.exists() and (not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() not in known):
            raise ValueError('Unowned or changed session file: ' + name)
        return path

    def preparing(self, name, content):
        path = self.check(name)
        known = [hashlib.sha256(content).hexdigest()]
        if path.exists(): known.append(hashlib.sha256(path.read_bytes()).hexdigest())
        # Both sides of the atomic replace are ours after an interrupted upgrade.
        self.data['files'][name] = list(dict.fromkeys(known)); self.save()

    def uninstall(self):
        if not self.path.exists(): raise ValueError('No managed ownership; legacy removal refused')
        paths = [self.check(name) for name in self.data['files']]
        for path in paths: path.unlink(missing_ok=True)
        self.data['files'] = {}; self.save()
