"""Offline source-discovery/audit fixtures; no artwork or network required."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(filename))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


d = module('discovery', 'discover-classic-art.py')
a = module('audit', 'audit-classic-art.py')


class DiscoveryTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        (self.root / 'corpus').mkdir()
        self.index = self.root / 'corpus/index.json'
        self.index.write_text(json.dumps({'images': [{}], 'provenance': {'referenceSha256': 'fixture'},
            'associations': {'ivysaur/2': 'hash'}, 'summary': {},
            'missing': [{'target': 'bulbasaur/1', 'speciesId': 'bulbasaur', 'number': 1, 'name': 'Bulbasaur'}]}))
        self.output = self.root / 'discovery'
        class Client:
            policy = b'User-agent: *\nAllow: /wiki/\n'
            calls = 0
            def get(self, url, limit):
                self.calls += 1
                return b'<a href="/wiki/File:0001Bulbasaur.png">art</a><a href="/wiki/File:0002Ivysaur.png">other</a><a href="/wiki/File:Spr_1_001.png">sprite</a><a href="/wiki/Category:Bulbasaur?filefrom=next">next</a>'
        self.client = Client()

    def discover(self):
        return d.discover(self.index, self.output, lambda: self.client)

    def test_missing_species_only_and_cached_metadata(self):
        self.discover()
        self.discover()
        self.assertEqual(self.client.calls, 1)
        row = json.loads((self.output / 'discovery.json').read_text())['bulbasaur']
        self.assertEqual([r['sourceName'] for r in row['candidates']], ['0001Bulbasaur.png'])
        self.assertTrue(row['firstCategoryPageOnly'])
        self.assertTrue(row['hasMoreCategoryFiles'])

    def test_corrupt_cache_rejected(self):
        self.discover()
        (self.output / 'bulbasaur.html').write_text('changed')
        with self.assertRaises(ValueError):
            self.discover()

    def test_audit_preserves_unknown_and_scope(self):
        self.discover()
        original = self.index.read_bytes()
        a.audit(self.index, self.output, self.root / 'audit')
        report = json.loads((self.root / 'audit/source-review.json').read_text())
        self.assertEqual(report['unresolved'][0]['disposition'], 'no-confirmed-exact-classic-illustration')
        self.assertTrue(report['unresolved'][0]['categoryHasMoreFiles'])
        self.assertEqual(self.index.read_bytes(), original)

    def test_audit_refuses_unchecked_species(self):
        self.output.mkdir()
        (self.output / 'discovery.json').write_text('{}')
        with self.assertRaises(ValueError):
            a.audit(self.index, self.output, self.root / 'audit')

    def test_blocked_discovery_can_resume(self):
        with self.assertRaises(RuntimeError):
            d.discover(self.index, self.output, lambda: self.Broken())
        self.assertEqual(json.loads((self.output / 'discovery.json').read_text())['bulbasaur']['state'], 'blocked')
        self.discover()
        self.assertEqual(self.client.calls, 1)

    def test_numeric_pagination_uses_allowed_route(self):
        class Paged:
            policy = b''
            calls = []
            def get(self, url, limit):
                self.calls.append(url)
                if 'filefrom' not in url:
                    return b'<a href="/wiki/File:0001Bulbasaur.png">a</a><a href="/w/index.php?title=Category:Bulbasaur&amp;filefrom=0001Bulbasaur-Other.png#mw-category-media">next</a>'
                return b'<a href="/wiki/File:0001Bulbasaur-Other.png">b</a><a href="/w/index.php?title=Category:Bulbasaur&amp;filefrom=Spr+1#mw-category-media">next</a>'
        client = Paged()
        d.discover(self.index, self.output, lambda: client)
        report = json.loads((self.output / 'discovery.json').read_text())['bulbasaur']
        self.assertEqual(len(report['candidates']), 2)
        self.assertEqual(len(report['pages']), 1)
        self.assertIn('/wiki/Category:', client.calls[1])
        self.assertNotIn('/w/index.php', client.calls[1])
        (self.output / report['pages'][0]['file']).write_text('tampered')
        with self.assertRaises(ValueError):
            d.discover(self.index, self.output, lambda: client)
        with self.assertRaises(ValueError):
            a.audit(self.index, self.output, self.root / 'audit')

    def test_audit_refuses_incomplete_numeric_range(self):
        self.discover()
        path = self.output / 'discovery.json'
        report = json.loads(path.read_text())
        report['bulbasaur'].pop('numberedRangeChecked')
        path.write_text(json.dumps(report))
        with self.assertRaises(ValueError):
            a.audit(self.index, self.output, self.root / 'audit')

    class Broken:
        policy = b''
        def get(self, url, limit):
            raise RuntimeError('source unavailable')


if __name__ == '__main__':
    unittest.main()
