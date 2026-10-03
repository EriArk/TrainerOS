#!/usr/bin/env python3
"""Capture, verify and install an exact offline Linux ARM64 core bundle.

Only *_libretro.so files are managed. Configuration, firmware and saves never
enter a bundle. Apply keeps a durable rollback journal; an interrupted apply
must be rolled back before another update. Run with the emulator closed.
"""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import tempfile


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def atomic_json(path, data):
    fd, name = tempfile.mkstemp(prefix='.bundle-', dir=path.parent)
    try:
        with os.fdopen(fd, 'w') as stream:
            json.dump(data, stream, indent=2); stream.write('\n')
            stream.flush(); os.fsync(stream.fileno())
        os.replace(name, path)
        sync_directory(path.parent)
    finally:
        if os.path.exists(name): os.unlink(name)


def sync_directory(path):
    if os.name == 'posix':
        fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
        try: os.fsync(fd)
        finally: os.close(fd)


def checked_file(path, expected=None):
    if path.is_symlink() or not path.is_file():
        raise ValueError('Expected a regular file: ' + str(path))
    with path.open('rb') as stream: header = stream.read(20)
    if len(header) != 20 or header[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', header, 18)[0] != 183:
        raise ValueError('Expected ELF64 little-endian AArch64: ' + str(path))
    sha = digest(path)
    if expected and (sha != expected['sha256'] or path.stat().st_size != expected['bytes']):
        raise ValueError('Core checksum mismatch: ' + str(path))
    return {'name': path.name, 'bytes': path.stat().st_size, 'sha256': sha}


def read_manifest(bundle):
    path = bundle / 'manifest.json'
    if path.stat().st_size > 1024 * 1024: raise ValueError('Manifest too large')
    data = json.loads(path.read_text())
    if data.get('version') != 1 or data.get('architecture') != 'linux-aarch64':
        raise ValueError('Unsupported core bundle')
    if not re.fullmatch(r'[a-zA-Z0-9._-]{1,80}', data.get('revision', '')):
        raise ValueError('Invalid bundle revision')
    rows = data['cores']
    if not isinstance(rows, list) or not 1 <= len(rows) <= 256: raise ValueError('Invalid core count')
    names = set()
    for row in rows:
        name = row['name']
        if not re.fullmatch(r'[a-zA-Z0-9_+-]+_libretro\.so', name) or name in names:
            raise ValueError('Invalid or duplicate core name')
        if not re.fullmatch(r'[a-f0-9]{64}', row['sha256']) or not 20 <= row['bytes'] <= 1024**3:
            raise ValueError('Invalid core identity')
        names.add(name)
        checked_file(bundle / name, row)
    return data


def capture(directory, bundle, revision):
    if not re.fullmatch(r'[a-zA-Z0-9._-]{1,80}', revision): raise ValueError('Invalid revision')
    source = directory.resolve(strict=True)
    bundle.mkdir()  # Never replace an existing immutable bundle.
    rows = []
    for path in sorted(source.glob('*_libretro.so')):
        before = checked_file(path)
        shutil.copyfile(path, bundle / path.name)
        checked_file(bundle / path.name, before)
        rows.append(before)
    data = {'version': 1, 'revision': revision, 'architecture': 'linux-aarch64', 'cores': rows}
    atomic_json(bundle / 'manifest.json', data)
    read_manifest(bundle)
    return data


def idle():
    if not Path('/proc').is_dir(): raise RuntimeError('Apply/rollback require Linux')
    for entry in Path('/proc').iterdir():
        if not entry.name.isdigit(): continue
        try: name = (entry / 'comm').read_text().strip().lower()
        except (FileNotFoundError, PermissionError, ProcessLookupError): continue
        if name == 'retroarch': raise RuntimeError('Close RetroArch before changing cores')


@contextlib.contextmanager
def locked(directory):
    import fcntl
    directory.mkdir(parents=True, exist_ok=True)
    if directory.is_symlink(): raise ValueError('Journal directory must not be a symlink')
    with (directory / '.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield


def replace_file(source, target, expected):
    fd, temp = tempfile.mkstemp(prefix='.traineros-core-', dir=target.parent)
    try:
        with os.fdopen(fd, 'wb') as out, source.open('rb') as inp:
            shutil.copyfileobj(inp, out)
            out.flush(); os.fsync(out.fileno()); os.fchmod(out.fileno(), 0o755)
        checked_file(Path(temp), expected)
        os.replace(temp, target); sync_directory(target.parent)
    finally:
        if os.path.exists(temp): os.unlink(temp)


def apply(bundle, directory, state):
    data = read_manifest(bundle)  # Validate every artifact before any mutation.
    target = directory.resolve(strict=True)
    state.mkdir(parents=True, exist_ok=True)
    with locked(target), locked(state):
        idle()
        journal = state / 'transaction.json'
        if journal.exists(): raise RuntimeError('Existing transaction: rollback it or use a new journal directory')
        changes = []
        for row in data['cores']:
            path = target / row['name']
            old = checked_file(path) if path.exists() or path.is_symlink() else None
            if old != row: changes.append({'new': row, 'old': old})
        backup = state / 'before'
        backup.mkdir(exist_ok=True)
        if backup.is_symlink(): raise ValueError('Rollback directory must not be a symlink')
        for change in changes:
            old = change['old']
            if old:
                path = backup / old['name']
                if path.exists(): raise ValueError('Existing rollback file')
                shutil.copyfile(target / old['name'], path)
                checked_file(path, old)
                with path.open('rb') as stream: os.fsync(stream.fileno())
        sync_directory(backup)
        transaction = {'version': 1, 'revision': data['revision'], 'directory': str(target),
                       'status': 'applying', 'changes': changes}
        atomic_json(journal, transaction)
        for change in changes:
            idle()
            row, old = change['new'], change['old']
            path = target / row['name']
            if old: checked_file(path, old)
            elif path.exists() or path.is_symlink(): raise ValueError('Core appeared during apply')
            replace_file(bundle / row['name'], path, row)
        result = verify(bundle, target)
        if result['different']: raise RuntimeError('Installed bundle did not verify')
        transaction['status'] = 'applied'
        atomic_json(journal, transaction)
        return {'revision': data['revision'], 'changed': len(changes), 'verified': result['verified']}


def verify(bundle, directory):
    data = read_manifest(bundle)
    different = []
    for row in data['cores']:
        try: checked_file(directory / row['name'], row)
        except (ValueError, FileNotFoundError): different.append(row['name'])
    return {'revision': data['revision'], 'verified': len(data['cores']) - len(different), 'different': different}


def rollback(state, directory):
    target = directory.resolve(strict=True)
    with locked(target), locked(state):
        idle()
        journal = state / 'transaction.json'
        transaction = json.loads(journal.read_text())
        if transaction['directory'] != str(target): raise ValueError('Rollback target does not match')
        # Validate journal names/identities and ALL current files before restoring any.
        for change in transaction['changes']:
            row, old = change['new'], change['old']
            if not re.fullmatch(r'[a-zA-Z0-9_+-]+_libretro\.so', row['name']): raise ValueError('Invalid journal path')
            path = target / row['name']
            current = checked_file(path) if path.exists() or path.is_symlink() else None
            if current not in (row, old): raise ValueError('Core changed since update; preserve it: ' + row['name'])
            if old:
                if old['name'] != row['name']: raise ValueError('Invalid rollback identity')
                checked_file(state / 'before' / row['name'], old)
        transaction['status'] = 'rolling-back'
        atomic_json(journal, transaction)
        for change in transaction['changes']:
            idle()
            row, old = change['new'], change['old']
            path = target / row['name']
            current = checked_file(path) if path.exists() or path.is_symlink() else None
            if current not in (row, old): raise ValueError('Core changed during rollback: ' + row['name'])
            if old: replace_file(state / 'before' / row['name'], path, old)
            elif path.exists(): path.unlink(); sync_directory(target)
        transaction['status'] = 'rolled-back'
        atomic_json(journal, transaction)
        return {'revision': transaction['revision'], 'status': 'rolled-back'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=['capture', 'verify', 'apply', 'rollback'])
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--bundle', type=Path)
    parser.add_argument('--state', type=Path)
    parser.add_argument('--revision')
    args = parser.parse_args()
    if args.operation != 'rollback' and args.bundle is None: parser.error('--bundle is required')
    if args.operation in ('apply', 'rollback') and args.state is None: parser.error('--state is required')
    if args.operation == 'capture' and not args.revision: parser.error('--revision is required')
    if args.operation == 'capture': result = capture(args.directory, args.bundle, args.revision)
    elif args.operation == 'apply': result = apply(args.bundle, args.directory, args.state)
    elif args.operation == 'verify': result = verify(args.bundle, args.directory)
    else: result = rollback(args.state, args.directory)
    print(json.dumps(result, indent=2))
    return 1 if result.get('different') else 0


if __name__ == '__main__':
    raise SystemExit(main())
