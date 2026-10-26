#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage a reviewed System Link package without replacing native user data."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import fcntl

SOURCE = '25e1ee2fd59a451f2ab57e825b4f8a5414220c8b'
RUNTIME = '0a2e4ff856644ae741a7476bc9c9dcd115b681f9faae26444df74ace2d275441'


def digest(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def copy(source, target):
    # Separate inode, shared extents on Btrfs; never chmod a hardlinked user file.
    with Path(source).open('rb') as src, target.open('xb') as dst:
        try:
            fcntl.ioctl(dst.fileno(), 0x40049409, src.fileno())  # FICLONE
        except OSError:
            shutil.copyfileobj(src, dst)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime', 'endpoint', 'transport', 'config', 'directory'):
        p.add_argument('--' + name, required=True, type=Path)
    p.add_argument('--endpoint-sha256', required=True)
    p.add_argument('--transport-sha256', required=True)
    a = p.parse_args()
    if (digest(a.runtime) != RUNTIME or digest(a.endpoint) != a.endpoint_sha256
            or digest(a.transport) != a.transport_sha256 or not a.config.is_absolute() or not a.config.is_file()):
        raise SystemExit('Runtime, helper or native settings changed')
    if not a.directory.is_absolute() or a.directory.exists() or a.directory.is_symlink():
        raise SystemExit('Choose a new absolute package directory')
    os.umask(0o077)
    a.directory.mkdir(parents=True, mode=0o700)
    files = {}
    for name, source in [('xemu.AppImage', a.runtime), ('bba_endpoint', a.endpoint),
                         ('stream_transport', a.transport), ('session.py', Path(__file__).with_name('session.py'))]:
        target = a.directory / name
        copy(source, target)
        target.chmod(0o600 if name.endswith('.py') else 0o700)
        files[name] = digest(target)
        if files[name] != digest(source):
            raise SystemExit('Package changed during staging')
    value = {'protocol': 1, 'source': SOURCE, 'settings': 'xemu-system-link-owned-v1',
             'config': str(a.config), 'files': files}
    with (a.directory / 'manifest.json').open('x') as f:
        json.dump(value, f, indent=2)
        f.flush()
        os.fsync(f.fileno())
    print('System Link package staged; native HDD/EEPROM/settings retained')


if __name__ == '__main__':
    main()
