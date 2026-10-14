#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Assemble the bounded local runtime; installation/OS dependencies are separate.

Build this directory and transport/ first. No downloads, service installation,
credentials, ROMs or saves are performed by this command.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def cache(directory):
    result = {}
    for line in (directory / 'CMakeCache.txt').read_text().splitlines():
        if '=' in line and not line.startswith(('#', '//')):
            name, value = line.split('=', 1)
            result[name.split(':', 1)[0]] = value
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--media-build', type=Path, required=True)
    parser.add_argument('--transport-build', type=Path, required=True)
    parser.add_argument('--trainer-source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    media, transport = args.media_build.resolve(), args.transport_build.resolve()
    source = Path(__file__).resolve().parent
    media_cache, transport_cache = cache(media), cache(transport)
    files = {name: media / name for name in ('stream_host', 'stream_guest')}
    files['stream_transport'] = transport / 'stream_transport'
    files['libmoonlight-common-c.so'] = media / '_deps/ml-build/libmoonlight-common-c.so'
    for name in ('run-game.py', 'run-group.py', 'capture-game.py', 'capture-game-audio.py', 'lobby_lease.py'):
        files[name] = source / name
    def part(values, directory, name):
        return Path(values.get('FETCHCONTENT_SOURCE_DIR_' + name.upper()) or directory / ('_deps/' + name + '-src'))
    parts = {
        'moonlight': Path(media_cache['moonlight-common-c_SOURCE_DIR']),
        'inputtino': Path(media_cache['inputtino_SOURCE_DIR']),
        'wolf': part(media_cache, media, 'wolf'),
        'nanors': part(media_cache, media, 'wolf_rs'),
        'datachannel': part(transport_cache, transport, 'datachannel'),
    }
    # Refuse an incomplete package before creating its destination.
    for name, path in files.items():
        if not path.is_file() or path.stat().st_size > 64 * 1024 * 1024:
            raise SystemExit('Missing or oversized runtime file: ' + name)
    notices = {'TrainerOS/LICENSE': args.trainer_source / 'LICENSE'}
    for name, root in parts.items():
        found = [p for p in root.rglob('*') if p.is_file() and not p.is_symlink()
                 and p.name.lower().startswith(('license', 'copying')) and '.git' not in p.parts]
        if not found:
            raise SystemExit('Missing third-party notices: ' + name)
        notices.update({name + '/' + p.relative_to(root).as_posix(): p for p in found})
    for path in notices.values():
        if not path.is_file():
            raise SystemExit('Missing TrainerOS licence')
    output = args.output.resolve()
    output.mkdir(mode=0o755)  # Never replace an existing installation.
    hashes = {}
    for name, source_file in files.items():
        target = output / name
        shutil.copyfile(source_file, target)
        target.chmod(0o755 if name.startswith('stream_') else 0o644)
        hashes[name] = hashlib.sha256(target.read_bytes()).hexdigest()
    for name, path in notices.items():
        target = output / 'notices' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
    (output / 'manifest.json').write_text(json.dumps({'recipe': 'remoteplay.v1', 'files': hashes}, indent=2) + '\n')
    print('REMOTE_PLAY_BUNDLE', json.dumps(hashes, sort_keys=True))


if __name__ == '__main__':
    main()
