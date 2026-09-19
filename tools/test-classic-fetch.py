"""Offline downloader tests; synthetic PNGs, no third-party network calls."""
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import urllib.error
import urllib.robotparser

from PIL import Image

spec = importlib.util.spec_from_file_location('fetcher', Path(__file__).with_name('fetch-classic-art.py'))
f = importlib.util.module_from_spec(spec)
spec.loader.exec_module(f)
PAGE = f.ORIGIN + '/wiki/File:0001Bulbasaur.png'
IMAGE = f.ORIGIN + '/media/upload/a/aa/0001Bulbasaur.png'
HTML = ('<a class="internal" href="' + IMAGE + '">Original</a>'
        '<a href="/wiki/Category:Ken_Sugimori_Pok%C3%A9mon_artwork">Art</a>').encode()
buf = io.BytesIO()
Image.new('RGBA', (12, 8), 'green').save(buf, format='PNG')
PNG = buf.getvalue()


class Fake:
    policy = b'User-agent: *\nAllow: /wiki/\nDisallow: /w/\nCrawl-delay: 5\n'
    def __init__(self):
        self.requests = []

    def get(self, url, limit):
        self.requests.append(url)
        return HTML if url == PAGE else PNG


class FetchTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        (self.root / 'corpus').mkdir()
        self.index = self.root / 'corpus/index.json'
        self.index.write_text(json.dumps({'images': [{}], 'provenance': {'referenceSha256': 'fixture'},
                                         'missing': [{'target': 'bulbasaur/1'}], 'associations': {'ivysaur/2': 'hash'}}))
        self.plan = self.root / 'plan.json'
        self.plan.write_text(json.dumps([{'target': 'bulbasaur/1', 'page': PAGE}]))
        self.output = self.root / 'supplement'
        self.client = Fake()

    def fetch(self):
        return f.fetch(self.index, self.plan, self.output, lambda: self.client)

    def test_missing_only_and_verified_cache(self):
        self.plan.write_text(json.dumps([{'target': 'bulbasaur/1', 'page': PAGE},
                                        {'target': 'ivysaur/2', 'page': PAGE}]))
        self.assertEqual(self.fetch(), {'downloaded': 1, 'reused': 0, 'coveredTargetsSkipped': 1})
        self.assertEqual(self.client.requests, [PAGE, IMAGE])
        self.client.requests.clear()
        self.assertEqual(self.fetch()['reused'], 1)
        self.assertEqual(self.client.requests, [])
        next(self.output.glob('*.png')).write_bytes(b'corrupt')
        with self.assertRaises(ValueError):
            self.fetch()

    def test_resume_after_later_candidate_failure(self):
        self.plan.write_text(json.dumps([{'target': 'bulbasaur/1', 'page': PAGE},
                                        {'target': 'bulbasaur/1', 'page': PAGE + '?variant=2'}]))
        with self.assertRaises(ValueError):
            self.fetch()
        self.plan.write_text(json.dumps([{'target': 'bulbasaur/1', 'page': PAGE}]))
        self.assertEqual(self.fetch()['reused'], 1)

    def test_seed_required_and_unknown_target(self):
        self.plan.write_text(json.dumps([{'target': 'unknown/1', 'page': PAGE}]))
        with self.assertRaises(ValueError):
            self.fetch()
        self.index.write_text('{}')
        with self.assertRaises(ValueError):
            self.fetch()
        self.assertEqual(self.client.requests, [])

    def test_sprite_page_and_external_host_rejected(self):
        class Sprite(Fake):
            def get(self, url, limit):
                return ('<a class="internal" href="' + IMAGE + '">Original</a>').encode()
        self.client = Sprite()
        with self.assertRaises(ValueError):
            self.fetch()
        self.assertFalse(f.allowed_url('https://other.example/file.png'))
        self.assertFalse(f.allowed_url('http://archives.bulbagarden.net/file.png'))

    def test_robots_and_rate_limit(self):
        client = f.Client.__new__(f.Client)
        client.robot = urllib.robotparser.RobotFileParser()
        client.robot.parse(Fake.policy.decode().splitlines())
        client.last, client.delay = 0, 5
        class Response(io.BytesIO):
            pass
        class Opener:
            calls = 0
            def open(self, request, timeout):
                self.calls += 1
                if self.calls == 1:
                    raise urllib.error.HTTPError(PAGE, 429, 'slow down', {'Retry-After': '12'}, None)
                return Response(HTML)
        client.opener = Opener()
        with self.assertRaises(ValueError):
            client.get(f.ORIGIN + '/w/api.php', 100)
        with patch.object(f.time, 'sleep') as sleep, patch.object(f.time, 'monotonic', return_value=100):
            self.assertEqual(client.get(PAGE, 1000), HTML)
            self.assertEqual(sleep.call_args_list[-1].args, (12,))
        self.assertEqual(client.opener.calls, 2)


if __name__ == '__main__':
    unittest.main()
