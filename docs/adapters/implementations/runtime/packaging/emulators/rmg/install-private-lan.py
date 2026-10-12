#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage native RMG plus a pinned private LAN helper in a new version directory."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--existing', type=Path, required=True)
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--requirements', type=Path, required=True)
    args = parser.parse_args()
    source, target = args.existing.resolve(strict=True), args.directory
    if sys.platform != 'linux' or not target.is_absolute() or target.exists() or target.is_symlink():
        raise SystemExit('Choose a new absolute Linux version directory')
    manifest = json.loads((source/'manifest.json').read_text())
    if (manifest.get('protocol') != 1 or manifest.get('source') != '4777ed3f1fb7f1e4a0e70438a27dc4a5e9fb1927'
            or manifest.get('serverSource') != 'bf0f28746971fcb6341fac55c0d2d19f646a6e09'
            or manifest.get('renderer') not in ('opengl', 'vulkan') or manifest.get('privateLanVersion')):
        raise SystemExit('Use the reviewed original native RMG bundle')
    for name, expected in manifest['files'].items():
        path = source/name
        if (Path(name).is_absolute() or '\\' in name or '..' in Path(name).parts
                or not path.resolve().is_relative_to(source) or digest(path) != expected):
            raise SystemExit('The native RMG bundle changed')
    requirements = args.requirements.read_text().splitlines()
    if any(line.lower().startswith('websocket-client') for line in requirements):
        raise SystemExit('Review the existing WebSocket dependency')
    requirements.append('websocket-client==1.9.0')
    recipe = Path(__file__).resolve().parent
    shared = recipe.parent/'native-lan'
    os.umask(0o077)
    target.mkdir(mode=0o700)
    # Copy only the manifest's installed native tree and retained notices.
    # Existing runtime selection and all ordinary config/progress stay untouched.
    for name in manifest['files']:
        path = target/name
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source/name, path)
    for path in source.iterdir():
        if path.is_file() and (path.name.startswith('LICENSE') or path.name.startswith('NOTICE')):
            shutil.copy2(path, target/path.name)
    transport = target/'transport'
    transport.mkdir(mode=0o700)
    (transport/'requirements.lock').write_text('\n'.join(sorted(requirements))+'\n')
    subprocess.run([sys.executable, '-m', 'venv', str(transport/'venv')], check=True)
    python = transport/'venv/bin/python'
    subprocess.run([str(python), '-m', 'pip', 'install', '-r', str(transport/'requirements.lock')], check=True)
    freeze = subprocess.check_output([str(python), '-m', 'pip', 'freeze']).decode().splitlines()
    if sorted(freeze) != sorted(requirements):
        raise SystemExit('Review changed transport dependencies')
    (transport/'dependencies.json').write_bytes(subprocess.check_output([str(python), '-m', 'pip', 'inspect']))
    for name in ('session.py', 'island.py', 'ethernet.py', 'packets.py', 'native_log.py'):
        shutil.copyfile(shared/name, transport/name)
    for name in ('native_lobby.py', 'network-worker.py'):
        shutil.copyfile(recipe/name, transport/name)
    channel = (recipe.parent/'melonds/secure-channel.py').read_text()
    if channel.count('"org.traineros.ds-channel/v1"') != 1 or channel.count('"traineros-ds-records-v1"') != 1:
        raise SystemExit('Review the shared channel version')
    channel = channel.replace('"org.traineros.ds-channel/v1"', '"org.traineros.rmg-channel/v1"')
    channel = channel.replace('"traineros-ds-records-v1"', '"traineros-rmg-records-v1"')
    (transport/'secure-channel.py').write_text(channel)
    for path in transport.iterdir():
        if path.is_file() and path.name != 'dependencies.json':
            manifest['files'][path.relative_to(target).as_posix()] = digest(path)
    manifest['privateLanVersion'] = 1
    manifest['interpreterSha256'] = digest(python)
    with (target/'manifest.pending').open('x') as stream:
        json.dump(manifest, stream, indent=2)
        stream.write('\n')
        stream.flush()
        os.fsync(stream.fileno())
    (target/'manifest.pending').replace(target/'manifest.json')
    print('Private RMG bundle staged; runtime selection unchanged')


if __name__ == '__main__':
    main()
