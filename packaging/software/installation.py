#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Owned Software versions, atomic receipt and explicit recovery; no OS updater."""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import time
import subprocess
import releases as sw
import channels
import host

DEFAULT_ROOT = Path.home() / '.local/share/traineros-software'


def boot_id():
    return Path('/proc/sys/kernel/random/boot_id').read_text().strip()


def ensure_no_orphans(root):
    marker = read(Path(root) / 'runtime-session.json')
    if marker:
        if marker.get('boot') == boot_id():
            raise RuntimeError('A managed session ended without releasing its games. Restart the device before maintenance.')
        (Path(root) / 'runtime-session.json').unlink()


def private(path, directory=True):
    path = Path(path).absolute()
    # Check existing ancestors, so an intermediate symlink cannot redirect writes.
    if any(p.is_symlink() for p in (path, *path.parents)):
        raise ValueError('Installation paths must not contain symlinks')
    if path.exists() and os.name != 'nt':
        info = path.stat()
        if info.st_uid != os.getuid() or info.st_mode & 0o022:
            raise ValueError('Installation must be owned by this user and private from other writers')
    if directory: path.mkdir(parents=True, exist_ok=True, mode=0o700)
    return path


def read(path, fallback=None):
    path = Path(path)
    if not path.exists(): return fallback
    private(path, False)
    if path.stat().st_size > sw.MAX_MANIFEST: raise ValueError('Oversized installation record')
    return sw.decode(path.read_bytes())


def configuration(root):
    root = private(root)
    config = read(root / 'config.json')
    if not isinstance(config, dict) or set(config) != {'schema', 'feeds', 'dataDirectory'} or config['schema'] != 1:
        raise ValueError('Missing trusted installation configuration')
    if set(config['feeds']) != {'beta', 'preview'}: raise ValueError('Both update channels are required')
    for url in config['feeds'].values(): sw.https(url)
    data = Path(config['dataDirectory']).absolute()
    if root == data or root in data.parents or data in root.parents:
        raise ValueError('Personal data must be outside the Software installation')
    private(root / 'trusted-key.pem', False)
    return config


def initialize(root, feeds, key, data):
    root = private(root)
    config = {'schema': 1, 'feeds': feeds, 'dataDirectory': str(Path(data).absolute())}
    if set(feeds) != {'beta', 'preview'}: raise ValueError('Both update channels are required')
    personal = Path(config['dataDirectory'])
    if root == personal or root in personal.parents or personal in root.parents:
        raise ValueError('Personal data must be outside the Software installation')
    for url in feeds.values(): sw.https(url)
    existing = read(root / 'config.json')
    if existing is not None and (existing != config or (root / 'trusted-key.pem').read_bytes() != Path(key).read_bytes()):
        raise ValueError('Existing bootstrap identity differs; do not replace its trust or personal-data path')
    if existing is None:
        key_bytes = Path(key).read_bytes()
        children = list(root.iterdir())
        if children and not (children == [root / 'trusted-key.pem'] and children[0].read_bytes() == key_bytes):
            raise ValueError('Use an empty owned Software installation directory')
        if len(key_bytes) > 8192 or b'PRIVATE KEY' in key_bytes: raise ValueError('A public verification key is required')
        subprocess.run(['openssl', 'pkey', '-pubin', '-in', str(key), '-noout'], check=True, capture_output=True, timeout=10)
        (root / 'trusted-key.pem').write_bytes(key_bytes)
        sw.atomic(root / 'config.json', config)
    configuration(root)
    return root


def receipt(root):
    result = read(Path(root) / 'receipt.json', {'schema': 1, 'active': '', 'previous': '', 'health': 'not-installed'})
    if result.get('schema') != 1 or any(v and not sw.HASH.fullmatch(v) for v in (result.get('active', ''), result.get('previous', ''))):
        raise ValueError('Invalid installation receipt')
    return result


def version(root, identity, verify=True):
    if not sw.HASH.fullmatch(identity): raise ValueError('Invalid installed version identity')
    path = private(Path(root) / 'versions' / identity, False)
    release = sw.verified(path / 'release.json', Path(root) / 'trusted-key.pem')['signed']
    if hashlib.sha256(sw.canonical(release)).hexdigest() != identity:
        raise ValueError('Installed identity differs from its signed manifest')
    if verify: sw.check_tree(path / 'payload', release)
    return release


def record_event(root, operation, **details):
    # Fixed-size diagnostic state contains identities, never account/ROM data.
    sw.atomic(Path(root) / 'operation.json', {'operation': operation, 'time': int(time.time()), **details})


def install_prepared(root, prepared, *, repair=False, checkpoint=lambda phase: None):
    root = private(root); config = configuration(root)
    prepared = Path(prepared)
    envelope = sw.verified(prepared / 'release.json', root / 'trusted-key.pem')
    release = envelope['signed']; host.compatible(release, host.schemas(config['dataDirectory']))
    sw.check_tree(prepared / 'payload', release)
    identity = hashlib.sha256(sw.canonical(release)).hexdigest()
    previous = receipt(root)
    if previous['active']:
        old = version(root, previous['active'], verify=False)
        if release['sequence'] < old['sequence'] and not repair:
            raise ValueError('Use explicit compatible rollback, not a downgrade install')
        if release['sequence'] == old['sequence'] and identity != previous['active']:
            raise ValueError('A release sequence cannot identify different installed content')
        if repair and identity != previous['active']: raise ValueError('Repair must use the installed signed release')
    record_event(root, 'install', phase='copy', identity=identity)
    versions = private(root / 'versions')
    needed = sum(f['size'] for f in release['files']) + 64 * 1024**2
    if shutil.disk_usage(versions).free < needed: raise ValueError('Not enough space for the installed release')
    destination = versions / identity
    if destination.is_symlink(): raise ValueError('Redirected installed version')
    if destination.exists():
        try: sw.check_tree(destination / 'payload', release)
        except (ValueError, OSError):
            if not repair: raise
            # Runtime lock is held by the caller. A crash preserves the signed
            # manifest in quarantine so a later explicit repair can finish.
            quarantine = versions / ('.damaged-' + identity)
            if quarantine.exists(): raise ValueError('Previous repair requires recovery')
            os.replace(destination, quarantine)
    if not destination.exists():
        with tempfile.TemporaryDirectory(dir=versions, prefix='.install-') as temporary:
            staged = Path(temporary) / 'version'; staged.mkdir()
            shutil.copytree(prepared / 'payload', staged / 'payload')
            sw.check_tree(staged / 'payload', release)
            # Durability before publishing the version / receipt.
            for path in (staged / 'payload').rglob('*'):
                if path.is_file():
                    with path.open('r+b') as stream: os.fsync(stream.fileno())
            if os.name != 'nt':
                for path in [*sorted((p for p in staged.rglob('*') if p.is_dir()), key=lambda p: len(p.parts), reverse=True), staged]:
                    fd = os.open(path, os.O_RDONLY)
                    try: os.fsync(fd)
                    finally: os.close(fd)
            sw.atomic(staged / 'release.json', envelope)
            checkpoint('copied'); os.replace(staged, destination)
    sw.atomic(destination / 'release.json', envelope)
    if os.name != 'nt':
        fd = os.open(versions, os.O_RDONLY)
        try: os.fsync(fd)
        finally: os.close(fd)
    checkpoint('version-published')
    # The launcher reads this single atomic record; no second active symlink.
    next_receipt = {'schema': 1, 'active': identity,
                    'previous': previous['previous'] if identity == previous['active'] else previous['active'],
                    'health': 'awaiting-first-frame'}
    host.compatible(release, host.schemas(config['dataDirectory']))
    sw.atomic(root / 'receipt.json', next_receipt)
    checkpoint('receipt-published')
    record_event(root, 'install', phase='complete', identity=identity)
    return {'status': 'installed', 'version': release['version'], 'sequence': release['sequence'], 'identity': identity}


def rollback(root):
    root = private(root); config = configuration(root); current = receipt(root)
    if not current['previous']: raise ValueError('No previous owned version is available')
    release = version(root, current['previous'])
    host.compatible(release, host.schemas(config['dataDirectory']))
    current['active'], current['previous'] = current['previous'], current['active']
    current['health'] = 'awaiting-first-frame'
    sw.atomic(root / 'receipt.json', current)
    record_event(root, 'rollback', phase='complete', identity=current['active'])
    return {'status': 'installed', 'version': release['version'], 'sequence': release['sequence']}


def context(root, check_host=True):
    root = private(root); config = configuration(root); current = receipt(root)
    installed = version(root, current['active'], verify=False) if current['active'] else None
    report = host.probe(root) if check_host else {'supported': True, 'target': 'linux-aarch64'}
    data = host.schemas(config['dataDirectory'])
    return {'status': 'context', 'supported': report['supported'], 'host': report,
            'installedVersion': installed['version'] if installed else '',
            'installedSequence': installed['sequence'] if installed else 0, 'target': report.get('target', ''),
            'sqliteSchema': data['sqlite'], 'firstRunSchema': data['firstRun'],
            'publicKey': str(root / 'trusted-key.pem'), 'betaUrl': config['feeds']['beta'], 'previewUrl': config['feeds']['preview'],
            'state': str(root / 'channels'), 'cache': str(root / 'cache'), 'health': current['health'],
            'rollbackAvailable': bool(current['previous']), 'dataDirectory': config['dataDirectory'],
            'executable': str(root / 'versions' / current['active'] / 'payload/bin/traineros') if installed else ''}


def resolve(root, channel, prepare=False, expected=None, progress=sw.event):
    root = Path(root); config = configuration(root); current = receipt(root)
    installed = version(root, current['active'], verify=False) if current['active'] else {'sequence': 0}
    report = host.probe(root)
    if not report['supported']: raise ValueError('; '.join(report['errors']))
    return channels.resolve(config['feeds'][channel], root / 'trusted-key.pem', channel, root / 'channels',
        installed['sequence'], report['target'], host.schemas(config['dataDirectory']),
        cache=root / 'cache' if prepare else None, expected_sequence=expected, progress=progress)


def request(root, operation, channel='beta', sequence=None):
    if operation not in ('apply', 'rollback', 'repair'): raise ValueError('Unknown maintenance operation')
    root = private(root)
    if operation == 'apply':
        if not sw.integer(sequence, 1, channels.MAX_SEQUENCE): raise ValueError('Selected sequence is required')
        result = resolve(root, channel, expected=sequence)
        if result['status'] != 'available': raise ValueError('Update changed or is no longer compatible; check again')
    elif operation == 'rollback':
        current = receipt(root); release = version(root, current['previous'])
        host.compatible(release, host.schemas(configuration(root)['dataDirectory']))
    sw.atomic(root / 'request.json', {'operation': operation, 'channel': channel, 'sequence': sequence,
                                     'active': receipt(root)['active'], 'created': int(time.time())})
    return {'status': 'restart-required'}


def apply_request(root):
    root = Path(root); job = read(root / 'request.json')
    if not job: return None
    try:
        if job['active'] != receipt(root)['active'] or not 0 <= time.time() - job['created'] < 3600:
            raise ValueError('Maintenance request is stale; review it again')
        if job['operation'] == 'apply':
            result = resolve(root, job['channel'], prepare=True, expected=job['sequence'])
            if result['status'] != 'prepared': raise ValueError('Update is no longer available')
            from installer import payload_dependencies
            payload_dependencies(result['prepared'])
            result = install_prepared(root, result['prepared'])
        elif job['operation'] == 'rollback': result = rollback(root)
        elif job['operation'] == 'repair': result = repair(root)
        else: raise ValueError('Unknown maintenance request')
        return result
    finally:
        # Never silently replay a failed request on the next unrelated launch.
        (root / 'request.json').unlink(missing_ok=True)


def audit(root):
    root = Path(root); config = configuration(root); current = receipt(root)
    problems = []
    if not current['active']: return {'status': 'not-installed', 'problems': []}
    try:
        release = version(root, current['active'])
        host.compatible(release, host.schemas(config['dataDirectory']))
    except (OSError, ValueError, KeyError) as error: problems.append(str(error))
    return {'status': 'repairable-drift' if problems else 'ready', 'scope': 'software-files',
            'sessionIntegration': 'recorded-not-audited' if read(root / 'integration.json', {}).get('systemInstalled') else 'not-recorded',
            'health': current['health'], 'problems': problems}


def repair(root, progress=sw.event, before_install=lambda: None):
    root = Path(root); identity = receipt(root)['active']
    directory = root / 'versions' / identity
    if not directory.exists(): directory = root / 'versions' / ('.damaged-' + identity)
    manifest = directory / 'release.json'
    envelope = sw.verified(manifest, root / 'trusted-key.pem')
    with sw.cache_lock(root / 'repair-cache') as cache:
        candidate = cache / hashlib.sha256(sw.canonical(envelope['signed'])).hexdigest()
        if candidate.exists():
            try: sw.check_tree(candidate / 'payload', envelope['signed'])
            except (OSError, ValueError): remove_owned_tree(candidate, cache)
    prepared = sw.prepare(manifest, root / 'trusted-key.pem', root / 'repair-cache', progress=progress)
    before_install()
    # Restore a recoverable quarantined record before another repair attempt.
    if directory.name.startswith('.damaged-'): os.replace(directory, root / 'versions' / identity)
    result = install_prepared(root, prepared, repair=True)
    quarantine = root / 'versions' / ('.damaged-' + identity)
    if quarantine.exists(): remove_owned_tree(quarantine, root / 'versions')
    return result


def remove_owned_tree(path, parent):
    path, parent = Path(path), Path(parent).resolve()
    if path.is_symlink() or path.resolve().parent != parent: raise ValueError('Refusing redirected cleanup')
    shutil.rmtree(path)


def clean_cache(root):
    root = Path(root); reclaimed = 0
    for name in ('cache', 'repair-cache'):
        with sw.cache_lock(root / name) as cache:
            for path in cache.iterdir():
                if path.name == '.prepare-lock': continue
                if path.is_symlink(): raise ValueError('Refusing redirected cache cleanup')
                if path.is_file() and (path.name.endswith('.part') and sw.HASH.fullmatch(path.stem)):
                    reclaimed += path.stat().st_size; path.unlink()
                elif path.is_dir() and (sw.HASH.fullmatch(path.name) or path.name.startswith('.extract-')):
                    reclaimed += sum(p.stat().st_size for p in path.rglob('*') if p.is_file() and not p.is_symlink())
                    remove_owned_tree(path, cache)
    return {'status': 'cache-cleared', 'bytes': reclaimed}


def uninstall(root, home=None, preflight=False):
    root = private(root); configuration(root)
    # Integrations are removed separately by their privileged owner. Keep the
    # bootstrap/recovery launcher, trust state and personal data for reinstall.
    integrations = read(root / 'integration.json', {})
    if integrations.get('systemInstalled') and not preflight: raise ValueError('Remove the system session integration before uninstalling Software')
    from installer import shortcut_contents
    allowed = {str(p) for p in shortcut_contents(root, home)}
    for record in read(root / 'shortcuts.json', []):
        if set(record) != {'path', 'sha256'} or record['path'] not in allowed:
            raise ValueError('Unexpected launcher ownership record')
        path = Path(record['path'])
        if path.is_symlink() or (path.exists() and sw.sha256(path) != record['sha256']):
            raise ValueError('A launcher was changed by another owner; preserve it and resolve manually')
    if preflight: return {'status': 'removal-reviewed'}
    sw.atomic(root / 'receipt.json', {'schema': 1, 'active': '', 'previous': '', 'health': 'not-installed'})
    for record in read(root / 'shortcuts.json', []): Path(record['path']).unlink(missing_ok=True)
    for name in ('versions',):
        if (root / name).exists(): remove_owned_tree(root / name, root)
    clean_cache(root)
    (root / 'request.json').unlink(missing_ok=True)
    sw.atomic(root / 'shortcuts.json', [])
    record_event(root, 'uninstall', phase='complete')
    return {'status': 'uninstalled', 'personalDataPreserved': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', default=str(DEFAULT_ROOT))
    parser.add_argument('command', choices=('context', 'audit', 'check', 'prepare', 'request', 'apply-pending', 'rollback', 'repair', 'clean-cache', 'uninstall'))
    parser.add_argument('--channel', choices=('beta', 'preview'), default='beta')
    parser.add_argument('--sequence', type=int)
    parser.add_argument('--operation', choices=('apply', 'rollback', 'repair'))
    args = parser.parse_args()
    try:
        root = private(args.root)
        with sw.cache_lock(root / 'transaction'):
            if args.command == 'context': result = context(root)
            elif args.command == 'audit': result = audit(root)
            elif args.command == 'clean-cache': result = clean_cache(root)
            elif args.command in ('check', 'prepare'): result = resolve(root, args.channel, args.command == 'prepare', args.sequence)
            elif args.command == 'request': result = request(root, args.operation, args.channel, args.sequence)
            else:
                # The running shell/descendants hold this lease through launcher.py.
                with sw.cache_lock(root / 'runtime'):
                    ensure_no_orphans(root)
                    result = {'apply-pending': apply_request, 'rollback': rollback, 'repair': repair,
                              'clean-cache': clean_cache, 'uninstall': uninstall}[args.command](root)
        print(json.dumps(result)); return 2 if result and result.get('status') == 'blocked' else 0
    except (OSError, ValueError, KeyError, TypeError, RuntimeError, subprocess.SubprocessError) as error:
        print(json.dumps({'status': 'error', 'error': str(error)})); return 1


if __name__ == '__main__': raise SystemExit(main())
