#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""TrainerOS Software release preparation; never activates code or modifies user data.

Installer and OTA share this verified cache. The trust key comes from the caller's
trusted bootstrap, never from the downloaded envelope. No production key/feed is
invented here. Crates retains its own catalogue, receipts and install lifecycle.
"""
import argparse
import contextlib
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tarfile
import tempfile
import urllib.parse
import urllib.request

# Reuse existing audited canonical JSON / Ed25519 / fsync+replace primitives.
_spec = importlib.util.spec_from_file_location('trainer_signed_json', Path(__file__).resolve().parents[1] / 'crates/catalogue.py')
_signed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_signed)
canonical, decode, atomic, signature = _signed.canonical, _signed.decode, _signed.atomic, _signed.signature

MAX_MANIFEST = 8 * 1024 * 1024
MAX_ARCHIVE = 8 * 1024**3
MAX_EXPANDED = 20 * 1024**3
CHUNK = 1024 * 1024
HASH = re.compile(r'[0-9a-f]{64}\Z')


def sha256(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def relative(value):
    # Portable, unambiguous names; no links, devices, absolute or Windows paths.
    if not isinstance(value, str) or len(value) > 240 or not re.fullmatch(r'[A-Za-z0-9_./+-]+', value):
        raise ValueError('Invalid release path')
    parts = value.split('/')
    if any(p in ('', '.', '..') for p in parts) or PurePosixPath(value).is_absolute():
        raise ValueError('Release path escapes its directory')
    if any(p.split('.')[0].upper() in {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(10)), *(f'LPT{i}' for i in range(10))} for p in parts):
        raise ValueError('Reserved release path')
    return value


def integer(value, minimum, maximum):
    return type(value) is int and minimum <= value <= maximum


def https(url):
    u = urllib.parse.urlsplit(url)
    if u.scheme != 'https' or not u.hostname or u.username or u.password or u.fragment:
        raise ValueError('Release downloads require HTTPS without embedded credentials')
    return url


def validate(release):
    if set(release) != {'schema', 'product', 'version', 'sequence', 'source', 'target', 'data', 'archive', 'files'}:
        raise ValueError('Unknown release fields')
    if release['schema'] != 1 or release['product'] != 'traineros-software' or release['target'] != 'linux-aarch64':
        raise ValueError('Unsupported Software release')
    if not isinstance(release['version'], str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._+-]{0,79}', release['version']):
        raise ValueError('Invalid release version')
    if not integer(release['sequence'], 1, 2**53 - 1) or not re.fullmatch('[0-9a-f]{40}', release['source']):
        raise ValueError('Release sequence and source revision are required')
    if set(release['data']) != {'sqlite', 'firstRun'}:
        raise ValueError('Data compatibility contract required')
    for contract in release['data'].values():
        if set(contract) != {'min', 'max'} or not integer(contract['min'], 0, 10000) or not integer(contract['max'], contract['min'], 10000):
            raise ValueError('Invalid data compatibility range')
    archive = release['archive']
    if set(archive) != {'sha256', 'size', 'url'} or not HASH.fullmatch(archive['sha256']) or not integer(archive['size'], 1, MAX_ARCHIVE):
        raise ValueError('Invalid archive identity')
    https(archive['url'])
    files = release['files']
    if not isinstance(files, list) or not 1 <= len(files) <= 20000:
        raise ValueError('Invalid file inventory')
    names, total = set(), 0
    for f in files:
        if set(f) != {'path', 'size', 'sha256', 'mode'}:
            raise ValueError('Invalid file record')
        name = relative(f['path']).lower()
        if name in names or not integer(f['size'], 0, MAX_ARCHIVE) or not HASH.fullmatch(f['sha256']) or type(f['mode']) is not int or f['mode'] not in (0o644, 0o755):
            raise ValueError('Invalid or duplicate file')
        names.add(name); total += f['size']
    if total > MAX_EXPANDED or 'bin/traineros' not in names:
        raise ValueError('Missing application or oversized payload')
    entry = next(f for f in files if f['path'].lower() == 'bin/traineros')
    if entry['path'] != 'bin/traineros' or entry['mode'] != 0o755:
        raise ValueError('Invalid application entrypoint')
    for name in names:
        if any(str(parent) in names for parent in PurePosixPath(name).parents if str(parent) != '.'):
            raise ValueError('File/directory collision')
    return release


def verified(path, public_key):
    with Path(path).open('rb') as stream:
        raw = stream.read(MAX_MANIFEST + 1)
    if len(raw) > MAX_MANIFEST:
        raise ValueError('Release envelope too large')
    envelope = decode(raw)
    if set(envelope) != {'signed', 'signature'}:
        raise ValueError('Invalid release envelope')
    signature(envelope['signed'], public_key, envelope['signature'])
    validate(envelope['signed'])
    return envelope


@contextlib.contextmanager
def cache_lock(root):
    root = Path(root)
    if root.is_symlink():
        raise ValueError('Cache must be a real directory')
    root.mkdir(parents=True, exist_ok=True, mode=0o700)
    if os.name != 'nt' and (root.stat().st_uid != os.getuid() or root.stat().st_mode & 0o022):
        raise ValueError('Cache must be owned by this user and not writable by others')
    lock = root / '.prepare-lock'
    if lock.is_symlink():
        raise ValueError('Invalid cache lock')
    with lock.open('a+b') as stream:
        if not os.fstat(stream.fileno()).st_size:
            stream.write(b'0'); stream.flush()
        stream.seek(0)
        try:
            if os.name == 'nt':
                import msvcrt
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as e:
            raise RuntimeError('Another release preparation is running') from e
        try:
            yield root
        finally:
            if os.name == 'nt':
                stream.seek(0); msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(stream, fcntl.LOCK_UN)


def event(phase, done=0, total=0):
    print(json.dumps({'phase': phase, 'bytes': done, 'totalBytes': total}), flush=True)


class SecureRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, response, code, message, headers, url):
        https(url)
        return super().redirect_request(request, response, code, message, headers, url)


def download(archive, part, progress=event, opener=None):
    """Interrupted transfers keep a bounded part; final identity is always hashed."""
    part = Path(part)
    if part.is_symlink():
        raise ValueError('Invalid partial download')
    expected = archive['size']
    offset = part.stat().st_size if part.exists() else 0
    if offset > expected:
        part.unlink(); offset = 0
    if offset < expected:
        request = urllib.request.Request(https(archive['url']), headers={'Accept-Encoding': 'identity', 'User-Agent': 'TrainerOS-Software/1'})
        if offset:
            request.add_header('Range', f'bytes={offset}-')
        open_url = opener or urllib.request.build_opener(SecureRedirect()).open
        with open_url(request, timeout=30) as response:
            https(response.geturl())
            if response.headers.get('Content-Encoding', 'identity') != 'identity':
                raise ValueError('Encoded release response is not supported')
            if response.status == 200:
                offset = 0
            elif response.status == 206:
                span = re.fullmatch(r'bytes (\d+)-(\d+)/(\d+)', response.headers.get('Content-Range', ''))
                if not span or tuple(map(int, span.groups())) != (offset, expected - 1, expected):
                    raise ValueError('Server returned an unexpected download range')
            else:
                raise ValueError('Unexpected download response')
            length = response.headers.get('Content-Length')
            if length is not None and int(length) != expected - offset:
                raise ValueError('Download size differs from signed release')
            with part.open('ab' if offset else 'wb') as stream:
                progress('download', offset, expected)
                while chunk := response.read(min(CHUNK, expected - offset + 1)):
                    if offset + len(chunk) > expected:
                        raise ValueError('Download exceeds signed size')
                    stream.write(chunk); offset += len(chunk); progress('download', offset, expected)
                stream.flush(); os.fsync(stream.fileno())
    if offset != expected:
        raise ValueError('Download interrupted; retry to continue')
    progress('verify-archive', offset, expected)
    if sha256(part) != archive['sha256']:
        part.unlink()
        raise ValueError('Release archive hash does not match; retry download')


def extract(archive, destination, release, progress=event):
    records = {f['path']: f for f in release['files']}
    seen, done = set(), 0
    total = sum(f['size'] for f in records.values())
    # Stream manually, never tar.extract(all); reject links, devices and extras.
    with tarfile.open(archive, 'r|gz') as tar:
        for member in tar:
            name = relative(member.name)
            if name not in records or name in seen or not member.isfile() or member.pax_headers:
                raise ValueError('Archive contains an unlisted/unsafe entry')
            record = records[name]
            if member.size != record['size'] or member.mode != record['mode']:
                raise ValueError('Archive metadata differs from signed inventory')
            path = destination / name
            path.parent.mkdir(parents=True, exist_ok=True)
            checksum = hashlib.sha256()
            with tar.extractfile(member) as source, path.open('xb') as target:
                while chunk := source.read(CHUNK):
                    target.write(chunk); checksum.update(chunk); done += len(chunk)
                    progress('extract', done, total)
                target.flush(); os.fsync(target.fileno())
            if checksum.hexdigest() != record['sha256']:
                raise ValueError('Extracted file hash does not match')
            path.chmod(record['mode']); seen.add(name)
    if seen != set(records):
        raise ValueError('Archive is incomplete')


def check_tree(directory, release):
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError('Missing or redirected prepared payload')
    files = {p.relative_to(directory).as_posix(): p for p in directory.rglob('*') if not p.is_dir() or p.is_symlink()}
    if set(files) != {f['path'] for f in release['files']}:
        raise ValueError('Prepared inventory changed')
    for f in release['files']:
        path = files[f['path']]
        if path.is_symlink() or not path.is_file() or path.stat().st_size != f['size'] or sha256(path) != f['sha256']:
            raise ValueError('Prepared payload is damaged')
        if os.name != 'nt' and path.stat().st_mode & 0o7777 != f['mode']:
            raise ValueError('Prepared permissions changed')


def prepare(envelope_path, public_key, cache, progress=event, opener=None):
    envelope = verified(envelope_path, public_key)
    release = envelope['signed']; identity = hashlib.sha256(canonical(release)).hexdigest()
    with cache_lock(cache) as root:
        destination = root / identity
        if destination.is_symlink():
            raise ValueError('Invalid prepared directory')
        if destination.exists():
            check_tree(destination / 'payload', release)
            atomic(destination / 'release.json', envelope)
            progress('prepared', 0, 0)
            return destination
        needed = release['archive']['size'] + sum(f['size'] for f in release['files']) + 64 * 1024**2
        if shutil.disk_usage(root).free < needed:
            raise ValueError('Not enough space to prepare the complete release')
        part = root / (release['archive']['sha256'] + '.part')
        download(release['archive'], part, progress, opener)
        # No active pointer: a verified directory means prepared, never installed.
        with tempfile.TemporaryDirectory(dir=root, prefix='.extract-') as temporary:
            staged = Path(temporary) / 'release'; staged.mkdir()
            payload = staged / 'payload'; payload.mkdir()
            extract(part, payload, release, progress)
            atomic(staged / 'release.json', envelope)
            check_tree(payload, release)
            os.replace(staged, destination)
            if os.name != 'nt':
                fd = os.open(root, os.O_RDONLY)
                try: os.fsync(fd)
                finally: os.close(fd)
        part.unlink(missing_ok=True)
        progress('prepared', 0, 0)
        return destination


def build(payload, output, private_key, version, sequence, source, url, data):
    """Publisher command, containing only an explicitly selected application tree."""
    payload, output = Path(payload).resolve(), Path(output).resolve()
    if output == payload or payload in output.parents:
        raise ValueError('Release output must be outside the payload')
    if output.exists():
        raise ValueError('Use a new release output directory')
    files = []
    for path in sorted(payload.rglob('*')):
        if path.is_symlink():
            raise ValueError('Payload links must be resolved by the bundle builder')
        if path.is_dir(): continue
        if not path.is_file(): raise ValueError('Only regular payload files are supported')
        name = relative(path.relative_to(payload).as_posix())
        mode = 0o755 if name == 'bin/traineros' or path.stat().st_mode & 0o111 else 0o644
        files.append({'path': name, 'size': path.stat().st_size, 'sha256': sha256(path), 'mode': mode})
    output.mkdir(parents=True)
    archive = output / 'traineros.tar.gz'
    with archive.open('wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', filename='', mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w', format=tarfile.USTAR_FORMAT) as tar:
            for f in files:
                info = tarfile.TarInfo(f['path']); info.size = f['size']; info.mode = f['mode']
                with (payload / f['path']).open('rb') as stream: tar.addfile(info, stream)
    release = validate({'schema': 1, 'product': 'traineros-software', 'version': version, 'sequence': sequence,
        'source': source, 'target': 'linux-aarch64', 'data': data,
        'archive': {'sha256': sha256(archive), 'size': archive.stat().st_size, 'url': url}, 'files': files})
    atomic(output / 'release.json', {'signed': release, 'signature': signature(release, private_key)})
    return output / 'release.json'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    for name in ('verify', 'prepare'):
        p = commands.add_parser(name); p.add_argument('--manifest', required=True); p.add_argument('--key', required=True)
        if name == 'prepare': p.add_argument('--cache', required=True)
    p = commands.add_parser('build')
    for name in ('payload', 'output', 'key', 'version', 'source', 'url'):
        p.add_argument('--' + name, required=True)
    p.add_argument('--sequence', type=int, required=True)
    for name in ('sqlite-min', 'sqlite-max', 'first-run-min', 'first-run-max'):
        p.add_argument('--' + name, type=int, required=True)
    args = parser.parse_args()
    try:
        if args.command == 'build':
            path = build(args.payload, args.output, args.key, args.version, args.sequence, args.source, args.url,
                {'sqlite': {'min': args.sqlite_min, 'max': args.sqlite_max}, 'firstRun': {'min': args.first_run_min, 'max': args.first_run_max}})
            print(json.dumps({'manifest': str(path)}))
        elif args.command == 'verify':
            release = verified(args.manifest, args.key)['signed']
            print(json.dumps({'verified': True, 'version': release['version'], 'target': release['target']}))
        else:
            path = prepare(args.manifest, args.key, args.cache)
            print(json.dumps({'prepared': str(path), 'installed': False}))
    except (OSError, ValueError, RuntimeError, KeyError, TypeError, tarfile.TarError, subprocess.SubprocessError) as error:
        print(json.dumps({'error': str(error)})); return 1
    except KeyboardInterrupt:
        print(json.dumps({'cancelled': True, 'installed': False})); return 130
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
