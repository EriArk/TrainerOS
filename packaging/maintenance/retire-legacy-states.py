#!/usr/bin/env python3
"""Offline, reversible retirement of exact TrainerOS mgba-entry-v1 folders.

Default: inventory only. --apply archives only fully recognized flat sessions.
--restore MANIFEST reverses recorded moves after verifying every byte.
Never follows symlinks, overwrites destinations or deletes save/media files.
"""
import argparse
from contextlib import closing, contextmanager
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import uuid


def sha(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def inventory(data):
    profile = json.loads((data / 'integrations/retroarch.json').read_text())
    if profile.get('resumeProtocol') != 'mgba-entry-v1':
        raise ValueError('No recognized legacy profile; nothing is authorized for retirement.')
    root = Path(profile['resumeDirectory'])
    if not root.is_absolute() or root.is_symlink() or not root.is_dir():
        raise ValueError('Legacy root must be an existing absolute directory, not a symlink.')
    root = root.resolve()
    with closing(sqlite3.connect((data / 'traineros.sqlite3').resolve().as_uri() + '?mode=ro', uri=True)) as db:
        if db.execute('PRAGMA user_version').fetchone()[0] < 7:
            raise ValueError('Install the ordinary-save/media migration first.')
        records = db.execute("SELECT id,content_path FROM adventures WHERE adapter_id='retroarch'").fetchall()
    eligible, kept = [], []
    known = {hashlib.sha256(identity.encode()).hexdigest(): Path(content).stem for identity, content in records}
    for owner in sorted(root.iterdir()):
        if owner.name not in known or owner.is_symlink() or not owner.is_dir():
            kept.append(str(owner.relative_to(root))); continue
        stem = known[owner.name]
        for session in sorted(owner.iterdir()):
            relative = str(session.relative_to(root))
            try:
                if session.is_symlink() or not session.is_dir() or str(uuid.UUID(session.name)) != session.name:
                    raise ValueError('Unknown session')
                files = list(session.iterdir())
                allowed = {'launch.cfg', stem + '.state.auto', stem + '.state.auto.png', stem + '.state0.entry'}
                if not files or any(f.is_symlink() or not f.is_file() or f.name not in allowed or f.stat().st_size > 16 * 1024 * 1024 for f in files):
                    raise ValueError('Contains an ordinary save, subdirectory or unknown file')
                config = (session / 'launch.cfg').read_bytes()
                expected = ('savestate_directory = "' + str(session) + '"\n'
                    'sort_savestates_enable = "false"\nsort_savestates_by_content_enable = "false"\n'
                    'savestates_in_content_dir = "false"\nsavestate_auto_save = "{automatic}"\n'
                    'savestate_auto_load = "false"\nsavestate_thumbnail_enable = "{automatic}"\n'
                    'config_save_on_exit = "false"\nauto_overrides_enable = "false"\n')
                if config not in [expected.format(automatic=value).encode() for value in ('true', 'false')]:
                    raise ValueError('Unrecognized launch settings')
                eligible.append({'relative': relative, 'files': {f.name: {'bytes': f.stat().st_size, 'sha256': sha(f)} for f in files}})
            except (ValueError, OSError):
                kept.append(relative)
    return {'version': 1, 'root': str(root), 'sessions': eligible, 'kept': kept}


def verify(folder, files):
    if folder.is_symlink() or not folder.is_dir() or {f.name for f in folder.iterdir()} != set(files):
        raise ValueError('Session contents changed; no overwrite is allowed.')
    for name, evidence in files.items():
        path = folder / name
        if path.is_symlink() or not path.is_file() or path.stat().st_size != evidence['bytes'] or sha(path) != evidence['sha256']:
            raise ValueError('Session bytes changed; keep both copies and inspect manually.')


def sync(directory):
    if os.name == 'posix':
        fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)


def move(report, archive, restore=False):
    root = Path(report['root'])
    # A manifest cannot request moves outside the two verified sibling trees.
    if report.get('version') != 1 or root.is_symlink() or root.resolve() != root or not root.is_dir():
        raise ValueError('Legacy root changed.')
    if archive.is_symlink() or archive.resolve() != archive or archive.parent != root.parent or not archive.name.startswith('traineros-retired-'):
        raise ValueError('Archive must be a real sibling directory.')
    moves = []
    seen = set()
    for entry in report['sessions']:
        relative = Path(entry['relative'])
        if relative in seen or relative.is_absolute() or len(relative.parts) != 2 or any(p in ('.', '..') for p in relative.parts):
            raise ValueError('Invalid manifest path.')
        seen.add(relative)
        if any(Path(name).name != name or '/' in name or '\\' in name for name in entry['files']):
            raise ValueError('Invalid manifest filename.')
        original, retired = root / relative, archive / relative
        source, target = (retired, original) if restore else (original, retired)
        for path in (source, target):
            if path.is_symlink() or path.parent.is_symlink() or path.resolve() != path:
                raise ValueError('Changed path or symlink.')
        if source.exists() and not target.exists():
            verify(source, entry['files']); moves.append((source, target, entry['files']))
        elif target.exists() and not source.exists():
            verify(target, entry['files']) # Already moved/restored before interruption.
        else:
            raise ValueError('Ambiguous source/destination; nothing is overwritten.')
    for source, target, files in moves:
        verify(source, files)
        target.parent.mkdir(exist_ok=True)
        if target.exists():
            raise ValueError('Destination appeared; stop without replacing it.')
        source.rename(target)
        sync(source.parent); sync(target.parent); sync(target.parent.parent)
        verify(target, files)


def retire(report):
    root = Path(report['root'])
    archive = root.parent / ('traineros-retired-' + str(uuid.uuid4()))
    archive.mkdir(mode=0o700)
    manifest = archive / 'manifest.json'
    with manifest.open('x', encoding='utf-8') as output:
        json.dump(report, output, indent=2); output.flush(); os.fsync(output.fileno())
    sync(archive); sync(archive.parent)
    move(report, archive)
    return manifest


def idle():
    if os.name != 'posix' or not Path('/proc').is_dir():
        raise ValueError('Apply/restore requires the offline Linux maintenance environment.')
    for proc in Path('/proc').glob('[0-9]*'):
        try:
            name = (proc / 'comm').read_text().strip()
            state = (proc / 'stat').read_text().split(') ', 1)[1][0]
        except (OSError, IndexError):
            continue
        if state != 'Z' and name in ('traineros', 'retroarch'):
            raise ValueError('Stop TrainerOS and RetroArch before moving legacy files.')


@contextmanager
def maintenance_lock(root):
    import fcntl
    path = Path(root).parent / '.traineros-retirement.lock'
    fd = os.open(path, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        idle()
        yield
    finally:
        os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-dir', type=Path)
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--restore', type=Path)
    args = parser.parse_args()
    if args.restore:
        if args.apply or args.data_dir:
            parser.error('--restore is a separate operation')
        idle()
        manifest = args.restore.resolve(strict=True)
        report = json.loads(manifest.read_text())
        with maintenance_lock(report['root']):
            move(report, manifest.parent, restore=True)
        print('Restored verified legacy folders; the archive manifest is retained.')
    else:
        if not args.data_dir:
            parser.error('--data-dir is required')
        if args.apply:
            idle()
        report = inventory(args.data_dir.resolve(strict=True))
        if args.apply:
            with maintenance_lock(report['root']):
                print(retire(report))
        else:
            print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
