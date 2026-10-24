#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Package the session-owned PSP relay and existing authenticated P2P helper.

No downloads or installation. Build with build-host.sh and the existing
remote-play-stream/transport CMake recipe first. Refuse to replace a bundle.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--relay-source', required=True, type=Path)
    parser.add_argument('--relay-binary', required=True, type=Path)
    parser.add_argument('--transport-build', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    recipe = Path(__file__).resolve().parent
    source = recipe.parents[1]
    revision = subprocess.check_output(['git', '-C', str(args.relay_source), 'rev-parse', 'HEAD'], text=True).strip()
    assert revision == '572de778fabcbda819ab0f677368c3fbc3033e19'
    files = {'psp_relay': args.relay_binary, 'stream_transport': args.transport_build / 'stream_transport'}
    notices = {'TrainerOS/LICENSE': source / 'LICENSE', 'aemu/gpl-3.0.txt': args.relay_source / 'gpl-3.0.txt'}
    cache = (args.transport_build / 'CMakeCache.txt').read_text()
    dependency = args.transport_build / '_deps/datachannel-src'
    for line in cache.splitlines():
        if line.startswith('FETCHCONTENT_SOURCE_DIR_DATACHANNEL:') and line.split('=', 1)[1]:
            dependency = Path(line.split('=', 1)[1])
    licenses = [p for p in dependency.rglob('*') if p.is_file() and not p.is_symlink()
                and p.name.lower().startswith(('license', 'copying')) and '.git' not in p.parts]
    assert licenses, 'Missing transport dependency notices'
    notices.update({'datachannel/' + p.relative_to(dependency).as_posix(): p for p in licenses})
    for path in [*files.values(), *notices.values()]:
        assert path.is_file() and not path.is_symlink() and path.stat().st_size <= 64*1024*1024, str(path)
    args.output.mkdir(mode=0o755)
    hashes = {}
    for name, path in files.items():
        target = args.output / name
        shutil.copyfile(path, target);target.chmod(0o755)
        hashes[name] = hashlib.sha256(target.read_bytes()).hexdigest()
    for name, path in notices.items():
        target = args.output / 'notices' / name
        target.parent.mkdir(parents=True, exist_ok=True);shutil.copyfile(path, target)
    # Keep the small integration recipe with the installed bundle. Full pinned
    # dependency sources remain in the reproducible build checkout.
    for name in ('build-host.sh', 'host-main.cpp', 'host-bind.patch', 'package-host.py'):
        shutil.copyfile(recipe / name, args.output / name)
    (args.output / 'manifest.json').write_text(json.dumps({
        'recipe': 'psp-host-v1', 'relayRevision': revision, 'files': hashes,
    }, indent=2) + '\n')
    print('PSP_HOST_BUNDLE', json.dumps(hashes, sort_keys=True))


if __name__ == '__main__':
    main()
