# SPDX-License-Identifier: GPL-3.0-or-later
"""Real keys/files and local HTTPS; never contacts devices or publishes releases."""
import copy
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
import os
from pathlib import Path
import ssl
import subprocess
import sys
import tempfile
import threading
import unittest
import urllib.request

SOURCE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SOURCE / 'packaging/software'))
import channels as ch
import releases as sw


class Response(io.BytesIO):
    def __init__(self, raw, status=200, headers=None, url='https://github.com/fixture/TrainerOS'):
        super().__init__(raw); self.status = status; self.headers = headers or {}; self.url = url
    def geturl(self): return self.url


class SoftwareChannelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.keys = tempfile.TemporaryDirectory()
        cls.key = Path(cls.keys.name) / 'private.pem'; cls.public = Path(cls.keys.name) / 'public.pem'
        subprocess.run(['openssl', 'genpkey', '-algorithm', 'ED25519', '-out', str(cls.key)], check=True, capture_output=True)
        subprocess.run(['openssl', 'pkey', '-in', str(cls.key), '-pubout', '-out', str(cls.public)], check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls): cls.keys.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name); payload = self.root / 'payload'; (payload / 'bin').mkdir(parents=True)
        (payload / 'bin/traineros').write_bytes(b'local fixture, not a distributable executable')
        self.feed_url = 'https://github.com/fixture/TrainerOS/releases/download/ota-channels/beta.json'
        self.manifest_url = 'https://github.com/fixture/TrainerOS/releases/download/fixture-7/release.json'
        self.archive_url = 'https://github.com/fixture/TrainerOS/releases/download/fixture-7/traineros.tar.gz'
        self.manifest = sw.build(payload, self.root / 'release', self.key, 'fixture-7', 7, 'a' * 40,
            self.archive_url, {'sqlite': {'min': 0, 'max': 16}, 'firstRun': {'min': 0, 'max': 2}})
        self.archive = self.root / 'release/traineros.tar.gz'
        self.channel = ch.build(self.manifest, self.public, self.key, self.root / 'beta.json', 'beta', 10,
            self.manifest_url, '2026-10-24T00:00:00Z', '2026-11-01T00:00:00Z', 'Fixture notes')
        self.feed = sw.decode(self.channel.read_bytes())['signed']
        self.state = self.root / 'state'; self.cache = self.root / 'cache'; self.requests = []
        self.now = ch.timestamp('2026-10-24T12:00:00Z')

    def resign(self):
        sw.atomic(self.channel, {'signed': self.feed, 'signature': sw.signature(self.feed, self.key)})

    def opener(self, request, timeout):
        self.requests.append(request.full_url)
        self.assertIsNone(request.get_header('Authorization'))
        self.assertEqual(request.get_header('Accept-encoding'), 'identity')
        path = {self.feed_url: self.channel, self.manifest_url: self.manifest, self.archive_url: self.archive}[request.full_url]
        return Response(path.read_bytes(), url=request.full_url)

    def resolve(self, **overrides):
        args = dict(feed_url=self.feed_url, public_key=self.public, channel='beta', state=self.state,
            installed_sequence=6, target='linux-aarch64', data={'sqlite': 16, 'firstRun': 2},
            now=self.now, opener=self.opener, progress=lambda *args: None)
        args.update(overrides)
        return ch.resolve(**args)

    def test_check_fetches_metadata_only_without_personal_data_or_payload(self):
        user = self.root / 'personal.sqlite'; user.write_bytes(b'untouched')
        result = self.resolve()
        self.assertEqual(result['status'], 'available'); self.assertFalse(result['installed'])
        self.assertEqual(result['notes'], 'Fixture notes'); self.assertEqual(result['downloadBytes'], self.archive.stat().st_size)
        self.assertEqual(self.requests, [self.feed_url, self.manifest_url])
        self.assertFalse(self.cache.exists()); self.assertEqual(user.read_bytes(), b'untouched')

    def test_explicit_prepare_reuses_verified_package_pipeline(self):
        result = self.resolve(cache=self.cache)
        self.assertEqual(result['status'], 'prepared'); self.assertFalse(result['installed'])
        self.assertTrue((Path(result['prepared']) / 'payload/bin/traineros').is_file())
        self.requests.clear(); self.resolve(cache=self.cache)
        self.assertNotIn(self.archive_url, self.requests)
        self.assertFalse((self.state / 'active.json').exists())

    def test_check_does_not_authorize_later_expired_preparation(self):
        self.resolve(); self.requests.clear()
        with self.assertRaisesRegex(ValueError, 'expired'):
            self.resolve(cache=self.cache, now=ch.timestamp(self.feed['expiresAt']))
        self.assertEqual(self.requests, [self.feed_url]); self.assertFalse(self.cache.exists())

    def test_channel_signature_failure_stops_before_manifest_and_pin(self):
        envelope = sw.decode(self.channel.read_bytes()); envelope['signed']['notes'] = 'tampered'
        sw.atomic(self.channel, envelope)
        with self.assertRaises(ValueError): self.resolve(cache=self.cache)
        self.assertEqual(self.requests, [self.feed_url]); self.assertFalse((self.state / 'beta.json').exists())

    def test_wrong_trust_key_rejected(self):
        key = self.root / 'other.pem'; public = self.root / 'other.pub'
        subprocess.run(['openssl', 'genpkey', '-algorithm', 'ED25519', '-out', str(key)], check=True, capture_output=True)
        subprocess.run(['openssl', 'pkey', '-in', str(key), '-pubout', '-out', str(public)], check=True, capture_output=True)
        with self.assertRaises(ValueError): self.resolve(public_key=public)

    def test_channel_identity_is_bound_to_request(self):
        with self.assertRaisesRegex(ValueError, 'identity'): self.resolve(channel='preview')
        self.assertFalse((self.state / 'preview.json').exists())

    def test_expired_and_future_feeds_fail_closed(self):
        for now in (ch.timestamp(self.feed['issuedAt']) - 1, ch.timestamp(self.feed['expiresAt'])):
            with self.subTest(now=now), self.assertRaisesRegex(ValueError, 'expired'):
                self.resolve(now=now)
        self.assertFalse((self.state / 'beta.json').exists())

    def test_publisher_cannot_create_unbounded_freshness(self):
        self.feed['expiresAt'] = '2027-01-01T00:00:00Z'
        with self.assertRaisesRegex(ValueError, '31 days'): ch.validate(self.feed)

    def test_durable_pin_blocks_replay_and_same_sequence_equivocation(self):
        self.resolve()
        self.feed['sequence'] = 9; self.resign()
        with self.assertRaisesRegex(ValueError, 'replay'): self.resolve()
        self.feed['sequence'] = 10; self.feed['notes'] = 'changed'; self.resign()
        with self.assertRaisesRegex(ValueError, 'reused'): self.resolve()
        self.assertEqual(sw.decode((self.state / 'beta.json').read_bytes())['signed']['notes'], 'Fixture notes')

    def test_new_feed_cannot_downgrade_or_redefine_release_sequence(self):
        self.resolve(); self.feed['sequence'] += 1
        self.feed['release']['sequence'] -= 1; self.resign()
        with self.assertRaisesRegex(ValueError, 'downgrade'): self.resolve()
        self.feed['release']['sequence'] += 1; self.feed['release']['manifest']['sha256'] = '0' * 64; self.resign()
        with self.assertRaisesRegex(ValueError, 'reused'): self.resolve()

    def test_refresh_can_renew_same_release_with_new_feed_sequence(self):
        self.resolve(); self.feed['sequence'] += 1; self.feed['issuedAt'] = '2026-10-25T00:00:00Z'
        self.feed['expiresAt'] = '2026-11-10T00:00:00Z'; self.resign()
        result = self.resolve(now=ch.timestamp('2026-11-02T00:00:00Z'))
        self.assertEqual(result['status'], 'available')

    def test_backward_publication_time_rejected(self):
        self.resolve(); self.feed['sequence'] += 1; self.feed['issuedAt'] = '2026-10-23T00:00:00Z'; self.resign()
        with self.assertRaisesRegex(ValueError, 'backwards'): self.resolve()

    def test_bootstrap_floors_apply_without_local_history(self):
        for kwargs in ({'minimum_feed': 11}, {'minimum_release': 8}):
            with self.subTest(kwargs=kwargs), self.assertRaisesRegex(ValueError, 'bootstrap'):
                self.resolve(**kwargs)
        self.assertFalse((self.state / 'beta.json').exists())

    def test_damaged_saved_state_is_not_silently_reset(self):
        self.resolve(); (self.state / 'beta.json').write_bytes(b'{}')
        with self.assertRaises(ValueError): self.resolve()

    def test_beta_and_preview_have_independent_high_water_marks(self):
        self.resolve(); self.feed['channel'] = 'preview'; self.feed['sequence'] = 1; self.resign()
        result = self.resolve(channel='preview')
        self.assertEqual(result['status'], 'available')
        self.assertTrue((self.state / 'beta.json').exists()); self.assertTrue((self.state / 'preview.json').exists())

    def test_newer_installed_preview_does_not_download_older_beta(self):
        for sequence in (7, 8):
            self.requests.clear(); result = self.resolve(installed_sequence=sequence, cache=self.cache)
            self.assertEqual(result['status'], 'up-to-date')
            self.assertEqual(self.requests, [self.feed_url])
        self.assertFalse(self.cache.exists())

    def test_target_and_updater_gates_precede_manifest_fetch(self):
        self.assertEqual(self.resolve(target='linux-x86_64')['reason'], 'target-incompatible')
        self.assertEqual(self.requests, [self.feed_url])
        self.feed['sequence'] += 1; self.feed['release']['minimumUpdater'] = ch.PROTOCOL + 1; self.resign()
        self.requests.clear(); result = self.resolve(cache=self.cache)
        self.assertEqual(result['reason'], 'updater-required'); self.assertEqual(self.requests, [self.feed_url])
        self.assertFalse(self.cache.exists())

    def test_data_ranges_gate_payload_download(self):
        for data in ({'sqlite': 17, 'firstRun': 2}, {'sqlite': 16, 'firstRun': 3}):
            result = self.resolve(data=data, cache=self.cache)
            self.assertEqual(result['reason'], 'data-incompatible')
        self.assertNotIn(self.archive_url, self.requests); self.assertFalse(self.cache.exists())

    def test_manifest_hash_and_signed_identity_both_required(self):
        original = self.manifest.read_bytes(); self.manifest.write_bytes(original + b' ')
        with self.assertRaises(ValueError): self.resolve(cache=self.cache)
        self.manifest.write_bytes(original)
        self.feed['sequence'] += 1; self.feed['release']['sequence'] += 1; self.resign()
        with self.assertRaisesRegex(ValueError, 'identity'): self.resolve(cache=self.cache)
        self.assertNotIn(self.archive_url, self.requests)

    def test_release_signature_required_even_with_valid_channel_hash(self):
        envelope = sw.decode(self.manifest.read_bytes()); envelope['signed']['source'] = 'b' * 40
        sw.atomic(self.manifest, envelope)
        self.feed['release']['manifest'].update(size=self.manifest.stat().st_size, sha256=sw.sha256(self.manifest)); self.resign()
        with self.assertRaisesRegex(ValueError, 'signature'): self.resolve(cache=self.cache)
        self.assertNotIn(self.archive_url, self.requests)

    def test_metadata_bounds_encoding_redirect_and_status(self):
        responses = [Response(b'x' * (ch.MAX_CHANNEL + 1)), Response(b'{}', headers={'Content-Length': '10'}),
                     Response(b'{}', headers={'Content-Encoding': 'gzip'}), Response(b'{}', status=206),
                     Response(b'{}', url='http://example.test/channel'), Response(b'', headers={'Content-Length': '-1'})]
        for response in responses:
            with self.subTest(response=response), self.assertRaises(ValueError):
                self.resolve(opener=lambda *a, **kw: response)
        self.assertFalse((self.state / 'beta.json').exists())

    def test_duplicate_keys_and_invalid_contract_are_rejected(self):
        self.channel.write_bytes(b'{"signed":{},"signed":{},"signature":""}')
        with self.assertRaises(ValueError): self.resolve()
        for field, value in (('schema', True), ('sequence', -1), ('notes', 5), ('release', [])):
            feed = copy.deepcopy(self.feed); feed[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError): ch.validate(feed)

    def test_interrupted_payload_resumes_through_remote_channel(self):
        raw = self.archive.read_bytes(); cut = len(raw) // 2
        def interrupt(request, timeout):
            return Response(raw[:cut]) if request.full_url == self.archive_url else self.opener(request, timeout)
        with self.assertRaisesRegex(ValueError, 'interrupted'): self.resolve(cache=self.cache, opener=interrupt)
        def resume(request, timeout):
            if request.full_url != self.archive_url: return self.opener(request, timeout)
            self.assertEqual(request.get_header('Range'), f'bytes={cut}-')
            return Response(raw[cut:], 206, {'Content-Range': f'bytes {cut}-{len(raw)-1}/{len(raw)}'})
        self.assertEqual(self.resolve(cache=self.cache, opener=resume)['status'], 'prepared')

    def test_network_failure_does_not_return_cached_available_or_reset_pin(self):
        self.resolve(); old = (self.state / 'beta.json').read_bytes()
        with self.assertRaises(OSError): self.resolve(opener=lambda *a, **kw: (_ for _ in ()).throw(OSError('offline')))
        self.assertEqual((self.state / 'beta.json').read_bytes(), old)

    def test_same_state_and_cache_rejected_without_network(self):
        with self.assertRaisesRegex(ValueError, 'separate'): self.resolve(cache=self.state)
        self.assertEqual(self.requests, [])

    def test_channel_lock_blocks_other_process(self):
        script = 'import sys;sys.path.insert(0,sys.argv[1]);import releases;\nwith releases.cache_lock(sys.argv[2]): pass'
        with sw.cache_lock(self.state):
            result = subprocess.run([sys.executable, '-c', script, str(SOURCE / 'packaging/software'), str(self.state)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0); self.assertIn('Another release preparation', result.stderr)

    def test_publisher_cli_builds_verifiable_asset_without_publishing(self):
        output = self.root / 'publisher.json'
        result = subprocess.run([sys.executable, str(SOURCE / 'packaging/software/channels.py'), 'build',
            '--manifest', str(self.manifest), '--public-key', str(self.public), '--signing-key', str(self.key),
            '--output', str(output), '--manifest-url', self.manifest_url, '--channel', 'beta', '--sequence', '10',
            '--issued-at', self.feed['issuedAt'], '--expires-at', self.feed['expiresAt']], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertFalse(json.loads(result.stdout)['published'])
        self.assertEqual(ch.verified(output.read_bytes(), self.public)[1]['release'], self.feed['release'])

    def test_real_https_redirect_and_cli_check_prepare(self):
        # Loopback-only TLS server with a temporary trusted self-signed certificate.
        cert, key = self.root / 'tls.pem', self.root / 'tls-key.pem'
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
            '-keyout', str(key), '-out', str(cert), '-subj', '/CN=localhost',
            '-addext', 'subjectAltName=DNS:localhost'], check=True, capture_output=True)
        routes, requests = {}, []
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args): pass
            def do_GET(self):
                requests.append((self.path, self.headers.get('Authorization')))
                if self.path == '/beta.json':
                    self.send_response(302); self.send_header('Location', '/assets/beta.json'); self.end_headers(); return
                raw = routes.get(self.path)
                if raw is None:
                    self.send_error(404); return
                self.send_response(200); self.send_header('Content-Length', str(len(raw))); self.end_headers(); self.wfile.write(raw)
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); context.load_cert_chain(cert, key)
        server.socket = context.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
        try:
            base = f'https://localhost:{server.server_port}'
            release = sw.decode(self.manifest.read_bytes())['signed']; release['archive']['url'] = base + '/traineros.tar.gz'
            sw.atomic(self.manifest, {'signed': release, 'signature': sw.signature(release, self.key)})
            self.feed['release']['manifest'] = {'url': base + '/release.json', 'size': self.manifest.stat().st_size, 'sha256': sw.sha256(self.manifest)}
            from datetime import datetime, timezone, timedelta
            now = datetime.now(timezone.utc)
            self.feed['issuedAt'] = (now - timedelta(minutes=1)).strftime('%Y-%m-%dT%H:%M:%SZ')
            self.feed['expiresAt'] = (now + timedelta(days=1)).strftime('%Y-%m-%dT%H:%M:%SZ'); self.resign()
            routes.update({'/assets/beta.json': self.channel.read_bytes(), '/release.json': self.manifest.read_bytes(), '/traineros.tar.gz': self.archive.read_bytes()})
            env = dict(os.environ, SSL_CERT_FILE=str(cert), NO_PROXY='localhost,127.0.0.1')
            args = ['--feed-url', base + '/beta.json', '--key', str(self.public), '--channel', 'beta',
                    '--state', str(self.state), '--installed-sequence', '6', '--target', 'linux-aarch64', '--sqlite', '16', '--first-run', '2']
            for command in ('check', 'prepare'):
                extra = ['--cache', str(self.cache)] if command == 'prepare' else []
                result = subprocess.run([sys.executable, str(SOURCE / 'packaging/software/channels.py'), command, *args, *extra],
                    env=env, capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                events = [json.loads(line) for line in result.stdout.splitlines()]
                self.assertEqual(events[-1]['status'], 'available' if command == 'check' else 'prepared')
                self.assertFalse(events[-1]['installed'])
                if command == 'check': self.assertNotIn('/traineros.tar.gz', [r[0] for r in requests])
            self.assertIn('/assets/beta.json', [r[0] for r in requests])
            self.assertIn('/traineros.tar.gz', [r[0] for r in requests])
            self.assertTrue(all(auth is None for _, auth in requests))
        finally:
            server.shutdown(); server.server_close(); thread.join(timeout=5)


if __name__ == '__main__': unittest.main()
