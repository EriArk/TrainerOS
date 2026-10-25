#!/usr/bin/env python3
"""Install the optional CBIN reader beside ordinary emulators, preserving them."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import uuid

SOURCE = 'c8816799b50388e61cfe237fe2cdbb7d8175f20a'


def verify(root):
    manifest = json.loads((root / 'manifest.json').read_text())
    assert manifest['protocol'] == 'pcsx-cbin-v1' and manifest['source'] == SOURCE
    assert set(manifest['files']) == {'core.so', 'cbin-reader.patch'}
    assert set(manifest['sources']) == {'source.tar.gz', 'build.sh', 'check-cbin.py'}
    for name, expected in {**manifest['files'], **manifest['sources']}.items():
        assert hashlib.sha256((root / name).read_bytes()).hexdigest() == expected, name
    assert (root / 'core.so').read_bytes()[:4] == b'\x7fELF'
    assert (root / 'LICENSE.upstream').is_file()
    return manifest


def install(package, target):
    manifest = verify(package)
    # Do not replace a runtime which a current session may still use.
    for process in Path('/proc').iterdir():
        if not process.name.isdecimal(): continue
        try: name = (process / 'comm').read_text().strip().lower()
        except FileNotFoundError: continue
        assert not name.startswith('retroarch'), 'Close RetroArch before updating its reader'
    target.parent.mkdir(parents=True, exist_ok=True)
    assert not target.is_symlink(), 'Unexpected linked package target'
    with tempfile.TemporaryDirectory(prefix='.pcsx-cbin-', dir=target.parent) as temporary:
        staged = Path(temporary) / 'package'; staged.mkdir()
        for name in (*manifest['files'], *manifest['sources'], 'manifest.json', 'LICENSE.upstream'):
            shutil.copyfile(package / name, staged / name)
        verify(staged)
        check = subprocess.run(['ldd', str(staged / 'core.so')], capture_output=True, text=True)
        assert check.returncode == 0 and 'not found' not in check.stdout, check.stdout
        backup = target.with_name(target.name + '-previous-' + uuid.uuid4().hex)
        had_target = target.exists()
        if had_target: os.rename(target, backup)
        try: os.rename(staged, target)
        except Exception:
            if had_target: os.rename(backup, target)
            raise
    return {'core': manifest['files']['core.so'], 'previous': str(backup) if had_target else None}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--destination', type=Path, default=Path(os.environ.get('XDG_DATA_HOME', str(Path.home()/'.local/share'))) / 'TrainerOS/emulators/pcsx-rearmed-cbin')
    args = parser.parse_args()
    print(json.dumps(install(args.package.resolve(), args.destination.absolute())))
