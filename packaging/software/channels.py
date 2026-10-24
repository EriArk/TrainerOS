#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Signed Software channel discovery and explicit candidate preparation.

Public HTTPS assets (including GitHub Releases) need no account or API token.
The bootstrap supplies URL, trust key and installed/data identity; these are not
discovered from untrusted metadata. This tool never activates a release.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import http.client
import json
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile
import time
import urllib.request

import releases as sw

PROTOCOL = 1
MAX_CHANNEL = 64 * 1024
MAX_LIFETIME = 31 * 24 * 3600
MAX_SEQUENCE = 2**53 - 1
CHANNELS = ('beta', 'preview')


def fields(value, names):
    if not isinstance(value, dict) or set(value) != set(names.split()):
        raise ValueError('Invalid channel metadata fields')


def timestamp(value):
    try:
        return int(datetime.strptime(value, '%Y-%m-%dT%H:%M:%SZ').replace(tzinfo=timezone.utc).timestamp())
    except (ValueError, TypeError, OverflowError) as error:
        raise ValueError('Timestamp must be UTC YYYY-MM-DDTHH:MM:SSZ') from error


def validate(feed):
    fields(feed, 'schema product channel sequence issuedAt expiresAt release notes')
    if type(feed['schema']) is not int or feed['schema'] != 1 or feed['product'] != 'traineros-software-channel' or feed['channel'] not in CHANNELS:
        raise ValueError('Unsupported Software channel')
    if not sw.integer(feed['sequence'], 1, MAX_SEQUENCE):
        raise ValueError('Invalid channel sequence')
    if not 0 < timestamp(feed['expiresAt']) - timestamp(feed['issuedAt']) <= MAX_LIFETIME:
        raise ValueError('Channel validity must be at most 31 days')
    if not isinstance(feed['notes'], str) or len(feed['notes']) > 8000:
        raise ValueError('Invalid release notes')
    release = feed['release']
    fields(release, 'version sequence target minimumUpdater manifest')
    if not isinstance(release['version'], str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._+-]{0,79}', release['version']):
        raise ValueError('Invalid channel release version')
    if release['target'] != 'linux-aarch64' or not sw.integer(release['sequence'], 1, MAX_SEQUENCE) or not sw.integer(release['minimumUpdater'], 1, MAX_SEQUENCE):
        raise ValueError('Invalid channel release contract')
    manifest = release['manifest']
    fields(manifest, 'url size sha256')
    sw.https(manifest['url'])
    if not sw.integer(manifest['size'], 1, sw.MAX_MANIFEST) or not isinstance(manifest['sha256'], str) or not sw.HASH.fullmatch(manifest['sha256']):
        raise ValueError('Invalid manifest identity')
    return feed


def verified(raw, public_key):
    if len(raw) > MAX_CHANNEL:
        raise ValueError('Channel envelope too large')
    envelope = sw.decode(raw)
    fields(envelope, 'signed signature')
    sw.signature(envelope['signed'], public_key, envelope['signature'])
    return envelope, validate(envelope['signed'])


def fetch(url, limit, opener=None):
    """Bounded metadata only; payload downloads use the existing Range path."""
    request = urllib.request.Request(sw.https(url), headers={
        'User-Agent': 'TrainerOS-Software/1', 'Accept-Encoding': 'identity', 'Cache-Control': 'no-cache'})
    open_url = opener or urllib.request.build_opener(sw.SecureRedirect()).open
    with open_url(request, timeout=30) as response:
        sw.https(response.geturl())
        if response.status != 200 or response.headers.get('Content-Encoding', 'identity') != 'identity':
            raise ValueError('Unexpected metadata response')
        length = response.headers.get('Content-Length')
        if length is not None and not 0 < int(length) <= limit:
            raise ValueError('Metadata response size is invalid')
        raw = response.read(limit + 1)
        if not raw or len(raw) > limit or (length is not None and len(raw) != int(length)):
            raise ValueError('Metadata response is incomplete or oversized')
        return raw


def pin(root, envelope, feed, public_key, minimum_feed, minimum_release):
    """Persist authenticated high-water marks, independently for each channel.

    Expired saved metadata still protects sequencing; it never grants permission
    to download. Loss of state removes this local history, not bootstrap floors.
    """
    path = root / (feed['channel'] + '.json')
    if path.is_symlink():
        raise ValueError('Redirected channel state')
    if feed['sequence'] < minimum_feed or feed['release']['sequence'] < minimum_release:
        raise ValueError('Channel is older than the trusted bootstrap minimum')
    if path.exists():
        with path.open('rb') as stream:
            _, old = verified(stream.read(MAX_CHANNEL + 1), public_key)
        if old['channel'] != feed['channel']:
            raise ValueError('Channel state identity mismatch')
        if feed['sequence'] < old['sequence']:
            raise ValueError('Channel replay rejected')
        if feed['sequence'] == old['sequence'] and sw.canonical(feed) != sw.canonical(old):
            raise ValueError('Channel sequence was reused with different contents')
        current, previous = feed['release'], old['release']
        if current['sequence'] < previous['sequence']:
            raise ValueError('Release downgrade in channel rejected')
        if current['sequence'] == previous['sequence'] and any(current[k] != previous[k] for k in ('version', 'target', 'manifest')):
            raise ValueError('Release sequence was reused with different contents')
        if timestamp(feed['issuedAt']) < timestamp(old['issuedAt']):
            raise ValueError('Channel publication time moved backwards')
    sw.atomic(path, envelope)


def resolve(feed_url, public_key, channel, state, installed_sequence, target, data,
            *, cache=None, minimum_feed=0, minimum_release=0, now=None, opener=None, progress=sw.event):
    """Check or explicitly prepare; installation identity comes from the caller.

    check downloads only metadata. prepare rechecks current signed metadata and
    all compatibility gates, then reuses releases.prepare for the payload.
    """
    if channel not in CHANNELS or any(not sw.integer(v, 0, MAX_SEQUENCE) for v in (installed_sequence, minimum_feed, minimum_release)):
        raise ValueError('Invalid bootstrap channel/sequence')
    fields(data, 'sqlite firstRun')
    if any(not sw.integer(v, 0, 10000) for v in data.values()):
        raise ValueError('Actual data schema versions are required')
    if cache is not None and Path(cache).resolve() == Path(state).resolve():
        raise ValueError('Channel state and payload cache must be separate directories')
    with sw.cache_lock(state) as root:
        envelope, feed = verified(fetch(feed_url, MAX_CHANNEL, opener), public_key)
        if feed['channel'] != channel:
            raise ValueError('Unexpected channel identity')
        clock = time.time() if now is None else now
        if not timestamp(feed['issuedAt']) <= clock < timestamp(feed['expiresAt']):
            raise ValueError('Channel expired or not yet valid; check connection and system clock')
        # Authenticate and retain the newest observation even if this client is
        # incompatible or fetching its release manifest subsequently fails.
        pin(root, envelope, feed, public_key, minimum_feed, minimum_release)
        candidate = feed['release']
        result = {'channel': channel, 'version': candidate['version'], 'sequence': candidate['sequence'],
                  'notes': feed['notes'], 'installed': False}
        if candidate['minimumUpdater'] > PROTOCOL:
            return {**result, 'status': 'blocked', 'reason': 'updater-required'}
        if candidate['target'] != target:
            return {**result, 'status': 'blocked', 'reason': 'target-incompatible'}
        if candidate['sequence'] <= installed_sequence:
            return {**result, 'status': 'up-to-date'}
        identity = candidate['manifest']
        raw = fetch(identity['url'], identity['size'], opener)
        if len(raw) != identity['size'] or hashlib.sha256(raw).hexdigest() != identity['sha256']:
            raise ValueError('Manifest differs from signed channel')
        release_envelope = sw.verified_bytes(raw, public_key)
        release = release_envelope['signed']
        if any(release[k] != candidate[k] for k in ('version', 'sequence', 'target')):
            raise ValueError('Release identity differs from signed channel')
        for name, version in data.items():
            if not release['data'][name]['min'] <= version <= release['data'][name]['max']:
                return {**result, 'status': 'blocked', 'reason': 'data-incompatible', 'data': name}
        result.update(status='available', downloadBytes=release['archive']['size'], source=release['source'])
        if cache is not None:
            # Private transient manifest prevents a check/prepare file substitution.
            with tempfile.TemporaryDirectory(dir=root, prefix='.manifest-') as temporary:
                manifest = Path(temporary) / 'release.json'
                sw.atomic(manifest, release_envelope)
                destination = sw.prepare(manifest, public_key, cache, progress, opener)
            result.update(status='prepared', prepared=str(destination))
        return result


def build(manifest, public_key, private_key, output, channel, sequence, url,
          issued_at, expires_at, notes='', minimum_updater=PROTOCOL):
    """Create a local signed channel asset. Publication is a separate owner step."""
    with Path(manifest).open('rb') as stream:
        raw = stream.read(sw.MAX_MANIFEST + 1)
    release = sw.verified_bytes(raw, public_key)['signed']
    feed = validate({'schema': 1, 'product': 'traineros-software-channel', 'channel': channel,
        'sequence': sequence, 'issuedAt': issued_at, 'expiresAt': expires_at, 'notes': notes,
        'release': {**{k: release[k] for k in ('version', 'sequence', 'target')}, 'minimumUpdater': minimum_updater,
                    'manifest': {'url': url, 'size': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}}})
    envelope = {'signed': feed, 'signature': sw.signature(feed, private_key)}
    # Catch a mismatched publisher key before writing anything for upload.
    verified(sw.canonical(envelope), public_key)
    output = Path(output)
    if output.exists() or output.is_symlink():
        raise ValueError('Use a new channel output path')
    sw.atomic(output, envelope)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    for name in ('check', 'prepare'):
        p = commands.add_parser(name)
        for field in ('feed-url', 'key', 'state', 'target'):
            p.add_argument('--' + field, required=True)
        p.add_argument('--channel', choices=CHANNELS, required=True)
        for field in ('installed-sequence', 'sqlite', 'first-run'):
            p.add_argument('--' + field, type=int, required=True)
        for field in ('minimum-feed-sequence', 'minimum-release-sequence'):
            p.add_argument('--' + field, type=int, default=0)
        if name == 'prepare':
            p.add_argument('--cache', required=True)
    p = commands.add_parser('build')
    for field in ('manifest', 'public-key', 'signing-key', 'output', 'manifest-url', 'issued-at', 'expires-at'):
        p.add_argument('--' + field, required=True)
    p.add_argument('--channel', choices=CHANNELS, required=True)
    p.add_argument('--sequence', type=int, required=True)
    p.add_argument('--minimum-updater', type=int, default=PROTOCOL)
    p.add_argument('--notes-file')
    args = parser.parse_args()
    try:
        if args.command == 'build':
            notes = Path(args.notes_file).read_text(encoding='utf-8') if args.notes_file else ''
            output = build(args.manifest, args.public_key, args.signing_key, args.output, args.channel,
                args.sequence, args.manifest_url, args.issued_at, args.expires_at, notes, args.minimum_updater)
            result = {'channelFile': str(output), 'published': False}
        else:
            result = resolve(args.feed_url, args.key, args.channel, args.state, args.installed_sequence,
                args.target, {'sqlite': args.sqlite, 'firstRun': args.first_run}, cache=getattr(args, 'cache', None),
                minimum_feed=args.minimum_feed_sequence, minimum_release=args.minimum_release_sequence)
        print(json.dumps(result, ensure_ascii=True))
        return 2 if result.get('status') == 'blocked' else 0
    except (OSError, ValueError, RuntimeError, KeyError, TypeError, RecursionError, http.client.HTTPException,
            tarfile.TarError, subprocess.SubprocessError) as error:
        print(json.dumps({'error': str(error), 'installed': False})); return 1
    except KeyboardInterrupt:
        print(json.dumps({'cancelled': True, 'installed': False})); return 130


if __name__ == '__main__':
    raise SystemExit(main())
