#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage a pinned ARM64 Vita bundle; selecting it and user data are separate."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

APPIMAGE_SHA='8a6b8ec4b65cc6769d69e4a48449d286ecd564ebe3a945a39218d0fa5ae3bf58'


def digest(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--appimage',type=Path,required=True)
    parser.add_argument('--directory',type=Path,required=True)
    parser.add_argument('--requirements',type=Path,required=True)
    args=parser.parse_args();output=args.directory
    if sys.platform!='linux' or digest(args.appimage)!=APPIMAGE_SHA:
        raise SystemExit('Use the pinned official ARM64 Vita3K AppImage')
    if not output.is_absolute() or output.exists() or output.is_symlink():
        raise SystemExit('Choose a new absolute version directory')
    recipe=Path(__file__).resolve().parent;os.umask(0o077)
    (output/'transport').mkdir(parents=True,mode=0o700)
    (output/'bin').mkdir(mode=0o700)
    subprocess.run(['unsquashfs','-no-progress','-o','936456','-d',str(output/'runtime'),str(args.appimage)],check=True)
    wrapped=output/'runtime/AppRun.wrapped'
    text=wrapped.read_text();old='\t"${APPDIR}/usr/bin/Vita3K" $@'
    if text.count(old)!=1:raise SystemExit('Review the upstream AppRun wrapper before updating')
    wrapped.write_text(text.replace(old,'\texec "${APPDIR}/usr/bin/Vita3K" "$@"'))
    # Keep one owned PID through AppRun and preserve arguments containing spaces.
    # No native emulation or upstream networking code is changed.
    requirements=args.requirements.read_text().splitlines()
    if not any(line.lower().startswith('pyyaml==') for line in requirements):requirements.append('PyYAML==6.0.3')
    if not any(line=='PyYAML==6.0.3' for line in requirements):raise SystemExit('Review the YAML parser version')
    (output/'transport/requirements.lock').write_text('\n'.join(sorted(requirements))+'\n')
    subprocess.run([sys.executable,'-m','venv',str(output/'transport/venv')],check=True)
    python=output/'transport/venv/bin/python'
    subprocess.run([str(python),'-m','pip','install','-r',str(output/'transport/requirements.lock')],check=True)
    freeze=subprocess.check_output([str(python),'-m','pip','freeze']).decode().splitlines()
    if sorted(freeze)!=sorted(requirements):raise SystemExit('Review changed transport dependencies')
    (output/'transport/dependencies.json').write_bytes(subprocess.check_output([str(python),'-m','pip','inspect']))
    for name in ['game.py','launcher.py','native_log.py','network-session.py','network-worker.py','packets.py','profiles.json']:
        shutil.copyfile(recipe/name,output/'transport'/name)
    channel=(recipe.parent/'melonds/secure-channel.py').read_text()
    assert channel.count('"org.traineros.ds-channel/v1"')==1 and channel.count('"traineros-ds-records-v1"')==1
    channel=channel.replace('"org.traineros.ds-channel/v1"','"org.traineros.vita-channel/v1"')
    channel=channel.replace('"traineros-ds-records-v1"','"traineros-vita-records-v1"')
    (output/'transport/secure-channel.py').write_text(channel)
    launcher=output/'bin/vita3k'
    launcher.write_text('#!/bin/sh\nset -eu\nlauncher=$(readlink -f "$0")\n'
        'root=$(dirname "$(dirname "$launcher")")\n'
        'export PYTHONDONTWRITEBYTECODE=1\n'
        'exec "$root/transport/venv/bin/python" "$root/transport/launcher.py" solo "$@"\n')
    launcher.chmod(0o700)
    shutil.copyfile(recipe/'LICENSE.upstream',output/'LICENSE.upstream')
    files={}
    for directory in [output/'runtime',output/'bin']:
        for path in sorted(directory.rglob('*')):
            if not path.resolve().is_relative_to(output.resolve()):raise SystemExit('Bundle link escapes its version directory')
            if path.is_file():files[path.relative_to(output).as_posix()]=digest(path)
    for path in sorted((output/'transport').iterdir()):
        if path.is_file() and path.name!='dependencies.json':files[path.relative_to(output).as_posix()]=digest(path)
    manifest={'protocol':1,'source':'46602447c7f84d53790b44e1869ab5105178bf16',
        'settings':'native-lan-two-owned-v1','appImageSha256':APPIMAGE_SHA,
        'files':files,'interpreterSha256':digest(python)}
    with (output/'manifest.pending').open('x') as stream:
        json.dump(manifest,stream,indent=2);stream.write('\n');stream.flush();os.fsync(stream.fileno())
    (output/'manifest.pending').replace(output/'manifest.json')
    print('Vita bundle staged; current-version selection and personal data unchanged')


if __name__=='__main__':main()
