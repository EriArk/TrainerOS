#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare reviewed GitHub release assets locally. Never publishes or creates keys."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tarfile
import tempfile
import urllib.parse
import releases as sw
import channels
import installer

SOURCE = Path(__file__).resolve().parents[2]


def build(payload, output, private_key, public_key, version, sequence, source, repository, tag,
          channel, channel_sequence, issued, expires, data, notes, provenance):
    payload, output = Path(payload), Path(output)
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository): raise ValueError('Invalid GitHub repository')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._+-]{0,100}', tag): raise ValueError('Invalid immutable release tag')
    if output.exists(): raise ValueError('Use a new publication output directory')
    record = sw.decode(Path(provenance).read_bytes())
    required = {'source', 'target', 'buildCommand', 'dependencies', 'sourceArchive', 'notices', 'redistributionReviewed'}
    if set(record) != required or record['source'] != source or record['target'] != 'linux-aarch64' or record['redistributionReviewed'] is not True:
        raise ValueError('Exact build provenance and redistribution review are required')
    if not record['buildCommand'] or not record['dependencies'] or not isinstance(record['dependencies'], dict):
        raise ValueError('Record the build command and exact dependency versions')
    for field in ('sourceArchive', 'notices'):
        path = Path(record[field])
        if not path.is_file() or not path.stat().st_size: raise ValueError('Missing ' + field)
    executable = payload / 'bin/traineros'
    with executable.open('rb') as stream: header = stream.read(64)
    if len(header) < 64 or header[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', header, 18)[0] != 183:
        raise ValueError('Payload entrypoint must be a Linux ARM64 ELF executable')
    base = f'https://github.com/{repository}/releases/download/{urllib.parse.quote(tag)}/'
    manifest = sw.build(payload, output, private_key, version, sequence, source, base + 'traineros.tar.gz', data)
    sw.verified(manifest, public_key)  # catches mismatched signing/public keys
    notes_text = Path(notes).read_text(encoding='utf-8') if notes else ''
    channels.build(manifest, public_key, private_key, output / (channel + '.json'), channel, channel_sequence,
                   base + 'release.json', issued, expires, notes_text)
    shutil.copyfile(record['sourceArchive'], output / 'corresponding-source.tar.gz')
    shutil.copyfile(record['notices'], output / 'THIRD_PARTY_NOTICES.txt')
    shutil.copyfile(SOURCE / 'LICENSE', output / 'LICENSE')
    published_record = {**record, 'sourceArchive': 'corresponding-source.tar.gz', 'notices': 'THIRD_PARTY_NOTICES.txt'}
    sw.atomic(output / 'build-provenance.json', published_record)
    with tempfile.TemporaryDirectory() as temporary:
        tree = Path(temporary) / 'TrainerOS-Setup'; tree.mkdir()
        # Use the same explicit helper inventory as the installed bootstrap.
        holder = Path(temporary) / 'holder'; holder.mkdir()
        installer.bootstrap(holder, SOURCE / 'packaging')
        shutil.copytree(holder / 'bootstrap/packaging', tree / 'packaging')
        shutil.copyfile(public_key, tree / 'trusted-key.pem')
        feeds = {name: f'https://raw.githubusercontent.com/{repository}/updates/{name}.json' for name in ('beta', 'preview')}
        sw.atomic(tree / 'bootstrap.json', {'schema': 1, 'feeds': feeds, 'publicKey': 'trusted-key.pem'})
        launch = tree / 'Install TrainerOS.sh'
        launch.write_bytes(b'#!/bin/sh\nset -eu\nHERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec /usr/bin/python3 "$HERE/packaging/software/installer.py" --gui\n')
        launch.chmod(0o755)
        (tree / 'README.txt').write_text('Extract this archive in Armada Desktop and open Install TrainerOS.sh.\n'
            'Keep the folder together. Setup/Recovery preserves personal data.\n'
            'Do not run as root. The session step uses the normal system authorization dialog.\n'
            'Hardware installation acceptance is required before advertising a supported public beta.\n')
        shutil.copyfile(SOURCE / 'LICENSE', tree / 'LICENSE')
        def permissions(info):
            info.uid = info.gid = 0; info.uname = info.gname = ''
            info.mode = 0o755 if info.isdir() or info.name.endswith('.sh') else 0o644
            return info
        with tarfile.open(output / 'TrainerOS-Setup.tar.gz', 'w:gz') as archive:
            archive.add(tree, arcname=tree.name, filter=permissions)
    hashes = {p.name: sw.sha256(p) for p in output.iterdir() if p.is_file()}
    sw.atomic(output / 'publication.json', {'schema': 1, 'repository': repository, 'tag': tag,
        'channel': channel, 'files': hashes, 'published': False})
    return {'output': str(output), 'published': False, 'publicKeyFingerprint': sw.sha256(public_key)}


def verify_remote(directory):
    """After operator publication: anonymous HTTPS byte identity, not GitHub API latest."""
    root = Path(directory); plan = sw.decode((root / 'publication.json').read_bytes())
    checked = []
    for name, digest in plan['files'].items():
        sw.relative(name)
        if sw.sha256(root / name) != digest: raise ValueError('Local publication asset changed')
        url = f"https://github.com/{plan['repository']}/releases/download/{plan['tag']}/{urllib.parse.quote(name)}"
        # Reuse the bounded streaming downloader instead of loading multi-GB assets.
        with tempfile.TemporaryDirectory() as temporary:
            sw.download({'url': url, 'sha256': digest, 'size': (root / name).stat().st_size}, Path(temporary) / 'asset', progress=lambda *args: None)
        checked.append(name)
    return {'verifiedAnonymousDownloads': checked, 'channelBranchPublished': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    p = commands.add_parser('build')
    for field in ('payload', 'output', 'private-key', 'public-key', 'version', 'source', 'repository', 'tag', 'issued', 'expires', 'provenance'):
        p.add_argument('--' + field, required=True)
    for field in ('sequence', 'channel-sequence', 'sqlite-min', 'sqlite-max', 'first-run-min', 'first-run-max'): p.add_argument('--' + field, type=int, required=True)
    p.add_argument('--channel', choices=('beta', 'preview'), required=True); p.add_argument('--notes')
    p = commands.add_parser('verify-remote'); p.add_argument('--directory', required=True)
    args = parser.parse_args()
    try:
        if args.command == 'verify-remote': result = verify_remote(args.directory)
        else:
            result = build(args.payload, args.output, args.private_key, args.public_key, args.version, args.sequence, args.source,
                args.repository, args.tag, args.channel, args.channel_sequence, args.issued, args.expires,
                {'sqlite': {'min': args.sqlite_min, 'max': args.sqlite_max}, 'firstRun': {'min': args.first_run_min, 'max': args.first_run_max}},
                args.notes, args.provenance)
        print(json.dumps(result)); return 0
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(json.dumps({'error': str(error), 'published': False})); return 1


if __name__ == '__main__': raise SystemExit(main())
