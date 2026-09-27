#!/usr/bin/env python3
"""Install missing official Linux/aarch64 Libretro cores; never replace a core.

Explicit --apply is required. Firmware and ROMs are never downloaded. Keep the
generated provenance report beside the private installation, outside Git.
"""
import argparse
import concurrent.futures
import hashlib
import io
import json
import os
import platform
import struct
import urllib.request
import zipfile
from pathlib import Path

BASE = 'https://buildbot.libretro.com/nightly/linux/aarch64/latest/'


def install(core, directory):
    name = core + '_libretro.so'
    target = directory / name
    if target.exists() or target.is_symlink():
        return {'core': core, 'status': 'preserved'}
    url = BASE + name + '.zip'
    try:
        with urllib.request.urlopen(url, timeout=60) as response:
            archive = response.read(256 * 1024 * 1024 + 1)
        if len(archive) > 256 * 1024 * 1024:
            raise ValueError('Archive exceeds size bound')
        with zipfile.ZipFile(io.BytesIO(archive)) as z:
            info = z.getinfo(name)
            if info.file_size > 512 * 1024 * 1024:
                raise ValueError('Core exceeds size bound')
            data = z.read(info)
        if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', data, 18)[0] != 183:
            raise ValueError('Not an ELF64 little-endian AArch64 core')
        # New-only temporary file and atomic no-clobber hard link on Linux.
        temp = directory / ('.' + name + '.download')
        with temp.open('xb') as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        try:
            os.link(temp, target)
        finally:
            temp.unlink()
        return {'core': core, 'status': 'installed', 'url': url,
                'archive_sha256': hashlib.sha256(archive).hexdigest(),
                'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}
    except Exception as exc:
        return {'core': core, 'status': 'unavailable', 'reason': type(exc).__name__}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=Path)
    p.add_argument('--registry', type=Path, default=Path(__file__).resolve().parents[1] / 'data/rom-platforms.json')
    p.add_argument('--apply', action='store_true')
    args = p.parse_args()
    data = json.loads(args.registry.read_text(encoding='utf8'))
    cores = sorted({p['core'] for p in data['platforms'] if p['core']})
    if not args.apply:
        print(json.dumps({'cores': cores, 'source': BASE}, indent=2))
    else:
        if platform.system() != 'Linux' or platform.machine() not in ('aarch64', 'arm64'):
            raise SystemExit('This installer is for native ARM64 Linux only')
        directory = args.directory.resolve(strict=True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
            rows = list(pool.map(lambda c: install(c, directory), cores))
        report = directory / 'traineros-core-install-report.json'
        # Preserve the previous provenance report, including previously installed cores.
        previous = json.loads(report.read_text()) if report.exists() else []
        by_core = {r['core']: r for r in previous}
        for r in rows:
            if r['status'] != 'preserved' or r['core'] not in by_core:
                by_core[r['core']] = r
        temp = report.with_suffix('.json.new')
        with temp.open('x') as f:
            json.dump(list(by_core.values()), f, indent=2)
        os.replace(temp, report)
        print(json.dumps(rows, indent=2))
