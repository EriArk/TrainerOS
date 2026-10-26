import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tarfile
import tempfile
import types
import sys
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('runtimes', Path(__file__).parents[1] / 'packaging/pc/runtimes.py')
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)


class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def test_verified_cache_reused_offline(self):
        data = b'official artifact'
        digest = hashlib.sha256(data).hexdigest()
        with patch.object(r, 'request', return_value=io.BytesIO(data)) as request:
            first = r.download('https://github.com/fixture', digest, self.root, len(data))
            second = r.download('https://github.com/fixture', digest, self.root, len(data))
            self.assertEqual(first, second)
            request.assert_called_once()
        self.assertEqual(first.read_bytes(), data)

    def test_bad_hash_and_truncation_never_publish(self):
        digest = 'a' * 64
        with patch.object(r, 'request', return_value=io.BytesIO(b'bad')):
            with self.assertRaises(ValueError):
                r.download('https://github.com/fixture', digest, self.root, 9)
        self.assertFalse((self.root / digest).exists())
        with patch.object(r, 'request', return_value=io.BytesIO(b'bad')):
            with self.assertRaises(ValueError):
                r.download('https://github.com/fixture', digest, self.root, 3)
        self.assertFalse((self.root / digest).exists())

    def archive(self, name, link=None):
        archive = self.root / 'test.tar'
        with tarfile.open(archive, 'w') as stream:
            entry = tarfile.TarInfo(name)
            if link:
                entry.type, entry.linkname = tarfile.SYMTYPE, link
                stream.addfile(entry)
            else:
                entry.size = 4
                stream.addfile(entry, io.BytesIO(b'test'))
        return archive

    def test_archive_traversal_and_escape_links_refused(self):
        for name, target in [('../escape', None), ('safe', '../../escape')]:
            with self.subTest(name=name):
                dest = self.root / 'extract'
                dest.mkdir(exist_ok=True)
                with self.assertRaises(tarfile.FilterError):
                    r.extract(self.archive(name, target), dest)
                self.assertFalse((self.root / 'escape').exists())

    def test_normal_archive_and_architecture_validation(self):
        dest = self.root / 'extract'
        dest.mkdir()
        r.extract(self.archive('bundle/notice'), dest)
        self.assertEqual((dest / 'bundle/notice').read_bytes(), b'test')
        bundle = dest / 'bundle'
        (bundle / 'files/bin').mkdir(parents=True)
        h = bytearray(20)
        h[:4], h[5], h[18] = b'\x7fELF', 1, 62
        (bundle / 'files/bin/wine').write_bytes(h)
        (bundle / 'proton').write_text('launcher')
        (bundle / 'toolmanifest.vdf').write_text('"require_tool_appid" "4183110"')
        r.validate_proton(bundle, 'x86_64')
        with self.assertRaises(ValueError):
            r.validate_proton(bundle, 'aarch64')
        (bundle / 'toolmanifest.vdf').write_text('"require_tool_appid" "9999"')
        with self.assertRaises(ValueError):
            r.validate_proton(bundle, 'x86_64')

    def test_catalogue_requires_matching_architecture_and_digest(self):
        assets = [dict(name='GE-Proton11-1-x86_64.tar.gz', digest='sha256:' + 'a' * 64, size=123, browser_download_url='https://github.com/a'),
                  dict(name='GE-Proton11-1-aarch64.tar.gz', digest='sha256:' + 'b' * 64, size=456, browser_download_url='https://github.com/b'),
                  dict(name='GE-Proton11-2-x86_64.tar.gz', digest=None, size=123, browser_download_url='https://github.com/c')]
        with patch.object(r, 'request', return_value=io.BytesIO(json.dumps([{'assets': assets}]).encode())):
            entries = r.catalogue('x86_64')
        self.assertEqual(len(entries), 1)
        self.assertEqual(entries[0]['sha256'], 'a' * 64)

    def test_invalid_sources_and_architecture_refused_before_cache(self):
        with self.assertRaises(ValueError):
            r.request('http://example.org/runtime')
        with self.assertRaises(ValueError):
            r.install(self.root, dict(arch='other'))
        self.assertFalse((self.root / 'runtimes').exists())

    def test_publish_reuse_and_failed_prepare_keep_existing_versions(self):
        h = bytearray(20)
        h[:4], h[5], h[18] = b'\x7fELF', 1, 62
        proton, umu = self.root / 'proton.tar', self.root / 'umu.tar'
        for target, files in [(proton, {'GE/proton': b'launcher', 'GE/files/bin/wine': h,
                                       'GE/toolmanifest.vdf': b'"require_tool_appid" "4183110"', 'GE/LICENSE': b'licence'}),
                              (umu, {'umu/umu-run': b'zipapp', 'umu/umu_run.py': b'entry'})]:
            with tarfile.open(target, 'w') as stream:
                for name, data in files.items():
                    entry = tarfile.TarInfo(name)
                    entry.size = len(data)
                    stream.addfile(entry, io.BytesIO(data))
        item = dict(title='GE-Proton11-1-x86_64', arch='x86_64', size=proton.stat().st_size,
                    sha256='a' * 64, url='https://github.com/GloriousEggroll/proton-ge-custom/releases/download/test/proton.tar.gz')
        # Windows runs the filesystem transaction, not a Linux flock assertion.
        fake_lock = types.SimpleNamespace(LOCK_EX=1, LOCK_NB=2, flock=lambda *_: None)
        with patch.dict(sys.modules, {'fcntl': fake_lock}), patch.object(r.platform, 'machine', return_value='x86_64'):
            with patch.object(r, 'download', side_effect=lambda url, *_: umu if url == r.UMU_URL else proton):
                installed = r.install(self.root / 'state', item)
            self.assertTrue((installed / 'files/bin/wine').is_file())
            self.assertEqual((installed / 'LICENSE').read_text(), 'licence')
            with patch.object(r, 'download') as network:
                self.assertEqual(r.install(self.root / 'state', item), installed)
                network.assert_not_called()
            with self.assertRaises(ValueError):
                r.install(self.root / 'state', dict(item, sha256='b' * 64))
            self.assertTrue((installed / 'proton').is_file())
            bad = dict(item, title='GE-Proton11-2-x86_64')
            with patch.object(r, 'download', return_value=self.archive('../escape')):
                with self.assertRaises(tarfile.FilterError):
                    r.install(self.root / 'state', bad)
            self.assertFalse((installed.parent / bad['title']).exists())
            self.assertEqual(list((self.root / 'state/runtimes').glob('.prepare-*')), [])


if __name__ == '__main__':
    unittest.main()
