#!/usr/bin/python3
"""Official GE-Proton artifacts + pinned UMU, immutable shared cache.

The shell owns UI/Downloads and per-game selection; this helper never edits a
registration, host tool, game environment or save. No runtime is executed here.
"""
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import sys
import tarfile
import tempfile
import time
from urllib.request import Request, urlopen

API = 'https://api.github.com/repos/GloriousEggroll/proton-ge-custom/releases?per_page=6'
UMU_URL = 'https://github.com/Open-Wine-Components/umu-launcher/releases/download/1.4.4/umu-launcher-1.4.4-zipapp.tar'
UMU_SHA = 'eb590691841f7fad3fc3ad8fd5db4ccb87849fe7948e62b28ece7a4ee48cc851'
MAX_ARCHIVE = 5 * 1024**3


def emit(**value):
    print(json.dumps(value), flush=True)


def request(url):
    if not url.startswith(('https://api.github.com/', 'https://github.com/')):
        raise ValueError('Unsupported runtime source')
    return urlopen(Request(url, headers={'User-Agent': 'TrainerOS-runtime-manager', 'Accept': 'application/vnd.github+json'}), timeout=30)


def catalogue(arch):
    suffix = 'aarch64' if arch in ('arm64', 'aarch64') else 'x86_64' if arch == 'x86_64' else ''
    if not suffix:
        return []
    with request(API) as response:
        releases = json.loads(response.read(2 * 1024**2))
    result = []
    for release in releases:
        if release.get('draft') or release.get('prerelease'):
            continue
        for asset in release['assets']:
            name = asset['name']
            digest = asset.get('digest', '')
            if (re.fullmatch(r'GE-Proton[\w.-]+-' + suffix + r'\.tar\.gz', name)
                    and re.fullmatch(r'sha256:[a-f0-9]{64}', digest or '')
                    and 0 < asset['size'] <= MAX_ARCHIVE):
                result.append(dict(title=name[:-7], url=asset['browser_download_url'],
                                   sha256=digest[7:], size=asset['size'], arch=suffix))
    return result


def download(url, digest, cache, size=0):
    target = cache / digest
    if target.is_file() and not target.is_symlink():
        with target.open('rb') as f:
            if hashlib.file_digest(f, 'sha256').hexdigest() == digest:
                return target
    partial = cache / (digest + '.part')
    fd = os.open(partial, os.O_WRONLY | os.O_CREAT | os.O_TRUNC | getattr(os, 'O_NOFOLLOW', 0), 0o600)
    total, last = 0, 0
    h = hashlib.sha256()
    with os.fdopen(fd, 'wb') as stream, request(url) as response:
        while chunk := response.read(1024**2):
            total += len(chunk)
            if total > MAX_ARCHIVE or (size and total > size):
                raise ValueError('Runtime download exceeds its published size')
            stream.write(chunk)
            h.update(chunk)
            if time.monotonic() - last > .25:
                emit(received=total, total=size, detail='Downloading official runtime')
                last = time.monotonic()
        stream.flush()
        os.fsync(stream.fileno())
    if (size and total != size) or h.hexdigest() != digest:
        raise ValueError('Runtime download failed its size or SHA-256 check')
    os.replace(partial, target)
    return target


def extract(archive, destination):
    with tarfile.open(archive) as tar:
        total, count = 0, 0
        for member in tar:
            total += max(0, member.size)
            count += 1
            if total > 20 * 1024**3 or count > 300000:
                raise ValueError('Runtime archive exceeds extraction limits')
        if shutil.disk_usage(destination).free < total + 128 * 1024**2:
            raise ValueError('Not enough space for the runtime')
        # Python data filter refuses traversal, devices and escaping links.
        # In-tree symlinks are essential for Wine and are kept intact.
        tar.extractall(destination, filter='data')


def validate_proton(root, arch):
    wine = root / 'files/bin/wine'
    with wine.open('rb') as stream:
        h = stream.read(20)
    machine = 183 if arch == 'aarch64' else 62
    if len(h) < 20 or h[:4] != b'\x7fELF' or h[5] != 1 or int.from_bytes(h[18:20], 'little') != machine:
        raise ValueError('Runtime Wine architecture does not match this device')
    manifest = (root / 'toolmanifest.vdf').read_text()[:65536]
    required = re.search(r'"require_tool_appid"\s+"(\d+)"', manifest)
    allowed = {'4185400'} if arch == 'aarch64' else {'1391110', '1628350', '4183110'}
    if required and required[1] not in allowed:
        raise ValueError('This runtime needs an unsupported Steam runtime')
    if not (root / 'proton').is_file():
        raise ValueError('Runtime launcher is missing')


def ensure_umu(base, cache):
    umu = base / 'umu/1.4.4'
    if (umu / 'umu-run').is_file() and (umu / 'umu_run.py').is_file():
        return
    emit(detail='Preparing UMU 1.4.4')
    archive = download(UMU_URL, UMU_SHA, cache)
    with tempfile.TemporaryDirectory(prefix='.umu-', dir=base) as temporary:
        stage = Path(temporary)
        extract(archive, stage)
        if not (stage / 'umu/umu-run').is_file() or not (stage / 'umu/umu_run.py').is_file():
            raise ValueError('Unexpected UMU archive layout')
        umu.parent.mkdir(exist_ok=True)
        if umu.exists():
            raise ValueError('The existing UMU installation needs repair; it was kept')
        os.replace(stage / 'umu', umu)


def install(state, item):
    arch = 'aarch64' if platform.machine() in ('arm64', 'aarch64') else platform.machine()
    if item.get('arch') != arch or arch not in ('x86_64', 'aarch64'):
        raise ValueError('This runtime does not match the device architecture')
    if not re.fullmatch(r'GE-Proton[\w.-]+-' + arch, item.get('title', '')):
        raise ValueError('Invalid runtime identity')
    if not re.fullmatch(r'[a-f0-9]{64}', item.get('sha256', '')):
        raise ValueError('Missing official artifact digest')
    if not item.get('url', '').startswith('https://github.com/GloriousEggroll/proton-ge-custom/releases/download/'):
        raise ValueError('Unsupported runtime source')
    base = state / 'runtimes'
    base.mkdir(parents=True, exist_ok=True)
    if base.is_symlink():
        raise ValueError('Runtime cache must be a local directory')
    for name in ('cache', 'proton', 'umu', 'umu/1.4.4', 'download.lock'):
        if (base / name).is_symlink():
            raise ValueError('Runtime cache contains a redirected managed path')
    import fcntl
    with (base / 'download.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        cache = base / 'cache'
        cache.mkdir(exist_ok=True)
        target = base / 'proton' / item['title']
        target.parent.mkdir(exist_ok=True)
        if target.exists():
            if target.is_symlink() or not (target / '.traineros-runtime.json').is_file():
                raise ValueError('An unmanaged folder already has this name')
            receipt = json.loads((target / '.traineros-runtime.json').read_text())
            if receipt['sha256'] != item['sha256']:
                raise ValueError('This version has changed upstream; the retained copy stays untouched')
            validate_proton(target, arch)
            ensure_umu(base, cache)
            return target
        archive = download(item['url'], item['sha256'], cache, item['size'])
        emit(detail='Verifying and preparing the shared runtime')
        with tempfile.TemporaryDirectory(prefix='.prepare-', dir=base) as temporary:
            stage = Path(temporary)
            extract(archive, stage)
            roots = [p.parent for p in stage.glob('*/proton') if p.is_file()]
            if len(roots) != 1:
                raise ValueError('Unexpected Proton archive layout')
            validate_proton(roots[0], arch)
            ensure_umu(base, cache)
            (roots[0] / '.traineros-runtime.json').write_text(json.dumps(item))
            os.replace(roots[0], target)
        return target


def main():
    def cancel(*_):
        raise InterruptedError('Runtime download cancelled; completed cache is retained')
    signal.signal(signal.SIGTERM, cancel)
    signal.signal(signal.SIGINT, cancel)
    try:
        if len(sys.argv) == 3 and sys.argv[1] == 'catalogue':
            emit(catalogue=catalogue(sys.argv[2]))
        elif len(sys.argv) == 4 and sys.argv[1] == 'install':
            emit(installed=str(install(Path(sys.argv[2]), json.loads(sys.argv[3]))))
        else:
            raise ValueError('Invalid runtime request')
    except Exception as error:
        emit(error=str(error)[:600])
        sys.exit(1)


if __name__ == '__main__':
    main()
