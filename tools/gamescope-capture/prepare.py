#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare pinned Gamescope + Armada sources in a NEW directory; never install."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    dest = args.directory.resolve()
    if dest.exists():
        parser.error('Use a new directory; existing work is never reset')
    here = Path(__file__).resolve().parent
    pins = json.loads((here / 'sources.json').read_text())
    subprocess.run(['git', 'clone', '--no-checkout', 'https://github.com/OpenGamingCollective/gamescope.git', str(dest)], check=True)
    def git(*args):
        subprocess.run(['git', '-C', str(dest), *args], check=True)
    git('checkout', '--detach', pins['gamescope'])
    git('submodule', 'update', '--init', '--recursive', '--depth', '1')
    patches = dest / '.traineros-patches'
    patches.mkdir()
    for name, digest in pins['patches'].items():
        url = f"https://raw.githubusercontent.com/armada-os/armada/{pins['armada']}/packages/gamescope/patches/{name}"
        with urllib.request.urlopen(url, timeout=30) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != digest:
            raise RuntimeError('Armada patch digest mismatch: ' + name)
        patch = patches / name
        patch.write_bytes(data)
        git('apply', '--check', str(patch))
        git('apply', str(patch))
    patch = here / '0001-pipewire-buffer-association.patch'
    git('apply', '--check', str(patch))
    git('apply', str(patch))
    print('Prepared:', dest)


if __name__ == '__main__':
    main()
