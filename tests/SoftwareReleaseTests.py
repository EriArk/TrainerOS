# SPDX-License-Identifier: GPL-3.0-or-later
"""No device/network access: real OpenSSL signatures, archive/filesystem and locks."""
import copy
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('software_releases', SOURCE / 'packaging/software/releases.py')
sw = importlib.util.module_from_spec(spec); spec.loader.exec_module(sw)


class Response(io.BytesIO):
    def __init__(self, data, status=200, headers=None):
        super().__init__(data); self.status = status; self.headers = headers or {}
    def geturl(self): return 'https://release.example/traineros.tar.gz'


class SoftwareReleaseTests(unittest.TestCase):
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
        self.root = Path(self.temp.name); self.payload = self.root / 'source'; (self.payload / 'bin').mkdir(parents=True)
        (self.payload / 'bin/traineros').write_bytes(b'local fixture, not an executable release')
        (self.payload / 'NOTICE').write_bytes(b'fixture license notice\n')
        self.manifest = sw.build(self.payload, self.root / 'release', self.key, 'fixture-1', 1, 'a' * 40,
            'https://release.example/traineros.tar.gz', {'sqlite': {'min': 0, 'max': 16}, 'firstRun': {'min': 1, 'max': 2}})
        self.envelope = sw.decode(self.manifest.read_bytes()); self.release = self.envelope['signed']
        self.archive = self.root / 'release/traineros.tar.gz'; self.cache = self.root / 'cache'
        self.events = []

    def progress(self, *args): self.events.append(args)
    def opener(self, request, timeout): return Response(self.archive.read_bytes())
    def prepare(self, opener=None): return sw.prepare(self.manifest, self.public, self.cache, self.progress, opener or self.opener)
    def resign(self):
        sw.atomic(self.manifest, {'signed': self.release, 'signature': sw.signature(self.release, self.key)})
    def altered_archive(self, members):
        with tarfile.open(self.archive, 'w:gz') as tar:
            for entry, data in members: tar.addfile(entry, io.BytesIO(data))
        self.release['archive']['size'] = self.archive.stat().st_size
        self.release['archive']['sha256'] = sw.sha256(self.archive)
        self.resign()

    def test_prepares_verified_tree_without_activation_or_user_data_changes(self):
        user = self.root / 'traineros.sqlite3'; user.write_bytes(b'private data')
        path = self.prepare()
        self.assertEqual((path / 'payload/bin/traineros').read_bytes(), (self.payload / 'bin/traineros').read_bytes())
        self.assertEqual(user.read_bytes(), b'private data')
        self.assertFalse((self.cache / 'active.json').exists())
        self.assertEqual(sw.verified(path / 'release.json', self.public)['signed'], self.release)
        self.assertEqual(self.events[-1][0], 'prepared')
        def no_network(*a, **kw): self.fail('Repeated preparation downloaded again')
        self.assertEqual(self.prepare(no_network), path)

    def test_bad_signature_never_downloads_or_creates_cache(self):
        self.envelope['signed']['version'] = 'tampered'; sw.atomic(self.manifest, self.envelope)
        with self.assertRaises(ValueError): self.prepare()
        self.assertFalse(self.cache.exists())

    def test_same_payload_and_contract_produce_identical_signed_artifacts(self):
        with patch('gzip.time.time', return_value=12345678):
            again = sw.build(self.payload, self.root / 'release-again', self.key, 'fixture-1', 1, 'a' * 40,
                self.release['archive']['url'], self.release['data'])
        self.assertEqual(again.read_bytes(), self.manifest.read_bytes())
        self.assertEqual(again.with_name('traineros.tar.gz').read_bytes(), self.archive.read_bytes())

    def test_wrong_trust_key_is_rejected(self):
        other = self.root / 'other.pem'; pub = self.root / 'other.pub'
        subprocess.run(['openssl', 'genpkey', '-algorithm', 'ED25519', '-out', str(other)], check=True, capture_output=True)
        subprocess.run(['openssl', 'pkey', '-in', str(other), '-pubout', '-out', str(pub)], check=True, capture_output=True)
        with self.assertRaises(ValueError): sw.verified(self.manifest, pub)

    def test_duplicate_json_keys_rejected(self):
        self.manifest.write_text('{"signed":{},"signed":{},"signature":""}')
        with self.assertRaises(ValueError): self.prepare()

    def test_interruption_resumes_by_range_and_verifies_full_hash(self):
        raw = self.archive.read_bytes(); cut = len(raw) // 2
        with self.assertRaisesRegex(ValueError, 'interrupted'): self.prepare(lambda *a, **kw: Response(raw[:cut]))
        part = self.cache / (self.release['archive']['sha256'] + '.part')
        self.assertEqual(part.read_bytes(), raw[:cut])
        def resume(request, timeout):
            self.assertEqual(request.get_header('Range'), f'bytes={cut}-')
            return Response(raw[cut:], 206, {'Content-Range': f'bytes {cut}-{len(raw)-1}/{len(raw)}'})
        self.prepare(resume); self.assertFalse(part.exists())

    def test_server_ignoring_range_restarts_without_appending(self):
        raw = self.archive.read_bytes()
        with self.assertRaises(ValueError): self.prepare(lambda *a, **kw: Response(raw[:30]))
        self.prepare()

    def test_invalid_range_and_oversized_response_are_rejected(self):
        raw = self.archive.read_bytes()
        with self.assertRaises(ValueError): self.prepare(lambda *a, **kw: Response(raw[:30]))
        with self.assertRaisesRegex(ValueError, 'range'):
            self.prepare(lambda *a, **kw: Response(raw, 206, {'Content-Range': f'bytes 0-{len(raw)-1}/{len(raw)}'}))
        with self.assertRaisesRegex(ValueError, 'exceeds'):
            self.prepare(lambda *a, **kw: Response(raw + b'extra'))

    def test_hash_mismatch_removes_corrupt_partial(self):
        raw = bytearray(self.archive.read_bytes()); raw[0] ^= 1
        with self.assertRaisesRegex(ValueError, 'hash'): self.prepare(lambda *a, **kw: Response(raw))
        self.assertFalse(list(self.cache.glob('*.part')))
        self.assertNotIn('prepared', [e[0] for e in self.events])

    def test_cancelled_extract_leaves_only_download_for_retry(self):
        def cancel(phase, *args):
            if phase == 'extract': raise KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            sw.prepare(self.manifest, self.public, self.cache, cancel, self.opener)
        self.assertFalse([p for p in self.cache.iterdir() if p.is_dir()])
        self.assertEqual(len(list(self.cache.glob('*.part'))), 1)
        def no_network(*a, **kw): self.fail('Complete verified part was downloaded again')
        self.prepare(no_network)

    def test_redirect_downgrade_and_encoded_body_rejected(self):
        import urllib.request
        request = urllib.request.Request('https://release.example/package')
        with self.assertRaisesRegex(ValueError, 'HTTPS'):
            sw.SecureRedirect().redirect_request(request, None, 302, '', {}, 'http://release.example/package')
        with self.assertRaisesRegex(ValueError, 'Encoded'):
            self.prepare(lambda *a, **kw: Response(self.archive.read_bytes(), headers={'Content-Encoding': 'gzip'}))

    def test_path_escape_and_inventory_collisions_rejected(self):
        for name in ('../outside', '/absolute', 'C:/bad', 'bin\\traineros', 'bin//file', 'bin/CON', 'bin/../bad'):
            release = copy.deepcopy(self.release); release['files'][0]['path'] = name
            with self.subTest(name=name), self.assertRaises(ValueError): sw.validate(release)
        for name in ('BIN/TRAINEROS', 'bin/traineros/child'):
            release = copy.deepcopy(self.release); extra = copy.deepcopy(release['files'][-1]); extra['path'] = name; release['files'].append(extra)
            with self.subTest(name=name), self.assertRaises(ValueError): sw.validate(release)

    def test_signed_but_unlisted_or_link_archive_is_rejected(self):
        for name, kind in (('../outside', tarfile.REGTYPE), ('extra', tarfile.REGTYPE), ('bin/traineros', tarfile.SYMTYPE)):
            with self.subTest(name=name):
                entry = tarfile.TarInfo(name); entry.type = kind; entry.linkname = '../../outside'
                self.altered_archive([(entry, b'')])
                with self.assertRaises(ValueError): self.prepare()
                self.assertFalse((self.root / 'outside').exists())
                self.assertFalse([p for p in self.cache.iterdir() if p.is_dir()])

    def test_signed_but_incomplete_or_wrong_file_hash_is_rejected(self):
        record = self.release['files'][0]; entry = tarfile.TarInfo(record['path']); entry.size = record['size']; entry.mode = record['mode']
        self.altered_archive([(entry, b'x' * entry.size)])
        with self.assertRaisesRegex(ValueError, 'hash'): self.prepare()
        self.altered_archive([(entry, (self.payload / record['path']).read_bytes())])
        with self.assertRaisesRegex(ValueError, 'incomplete'): self.prepare()

    def test_prepared_payload_is_rechecked_on_reuse(self):
        path = self.prepare(); (path / 'payload/bin/traineros').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'damaged'): self.prepare()

    def test_no_space_never_downloads(self):
        with patch.object(sw.shutil, 'disk_usage', return_value=type('Space', (), {'free': 0})()):
            with self.assertRaisesRegex(ValueError, 'space'): self.prepare()
        self.assertFalse(list(self.cache.glob('*.part')))

    def test_cache_lock_blocks_a_second_process(self):
        script = 'import sys;sys.path.insert(0,sys.argv[1]);import releases;\nwith releases.cache_lock(sys.argv[2]): pass'
        import sys
        with sw.cache_lock(self.cache):
            result = subprocess.run([sys.executable, '-c', script, str(SOURCE / 'packaging/software'), str(self.cache)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0); self.assertIn('Another release preparation', result.stderr)
        self.prepare()

    def test_https_and_data_compatibility_contract_required(self):
        for url in ('http://release.example/package', 'https://user:password@release.example/package', 'file:///tmp/package'):
            release = copy.deepcopy(self.release); release['archive']['url'] = url
            with self.assertRaises(ValueError): sw.validate(release)
        release = copy.deepcopy(self.release); release['data']['sqlite'] = {'min': 16, 'max': 15}
        with self.assertRaises(ValueError): sw.validate(release)

    def test_cli_uses_real_signatures_and_reports_verification(self):
        import sys
        result = subprocess.run([sys.executable, str(SOURCE / 'packaging/software/releases.py'), 'verify', '--manifest', str(self.manifest), '--key', str(self.public)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(result.stdout)['verified'])


if __name__ == '__main__': unittest.main()
