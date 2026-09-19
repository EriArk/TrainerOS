"""Synthetic image fixtures only; run with Python + Pillow on Windows/Linux."""
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile

from PIL import Image

spec = importlib.util.spec_from_file_location('classic', Path(__file__).with_name('import-classic-art.py'))
art = importlib.util.module_from_spec(spec)
spec.loader.exec_module(art)


def png(color):
    stream = io.BytesIO()
    Image.new('RGBA', (8, 12), color).save(stream, format='PNG')
    return stream.getvalue()


class CorpusTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.seed = self.root / 'seed'
        self.seed.mkdir()
        self.out = self.root / 'corpus'
        self.ref = self.root / 'reference.json'
        self.ref.write_text(json.dumps({'entries': [
            {'id': 'bulbasaur', 'number': 1, 'name': 'Bulbasaur',
             'forms': [{'id': '1', 'name': 'Standard'}]},
            {'id': 'venusaur', 'number': 3, 'name': 'Venusaur',
             'forms': [{'id': '3', 'name': 'Standard'}, {'id': '10133', 'name': 'Mega Venusaur'}]}
        ]}), encoding='utf8')

    def run_import(self, seed=None, review=None):
        return art.run(seed or self.seed, self.out, self.ref, 'fixture:original', review)

    def test_duplicates_resume_and_missing_forms(self):
        data = png('red')
        (self.seed / '0001 Bulbasaur.png').write_bytes(data)
        (self.seed / '0003 Venusaur.png').write_bytes(png('blue'))
        (self.seed / 'copy').mkdir()
        (self.seed / 'copy/0001 Bulbasaur.png').write_bytes(data)
        result = self.run_import()
        self.assertEqual(result['uniqueImages'], 2)
        self.assertEqual(result['coveredForms'], 2)
        missing = json.loads((self.out / 'missing.json').read_text())
        self.assertEqual(missing[0]['target'], 'venusaur/10133')
        before = {p.name: p.stat().st_mtime_ns for p in (self.out / 'originals').iterdir()}
        self.assertEqual(result, self.run_import())
        self.assertEqual(before, {p.name: p.stat().st_mtime_ns for p in (self.out / 'originals').iterdir()})

    def test_conflict_never_first_filename(self):
        (self.seed / '0001 Bulbasaur.png').write_bytes(png('red'))
        (self.seed / '1Bulbasaur.png').write_bytes(png('blue'))
        self.assertEqual(self.run_import()['coveredForms'], 0)
        self.assertEqual(self.run_import()['conflictingTargets'], 1)

    def test_exact_forms_unknown_suffix_and_review(self):
        data = png('red')
        (self.seed / '0003 Venusaur Mega.png').write_bytes(data)
        (self.seed / '0001 Bulbasaur Shiny.png').write_bytes(png('green'))
        self.assertEqual(self.run_import()['coveredForms'], 1)
        review = self.root / 'review.json'
        review.write_text(json.dumps({art.digest(png('green')): 'bulbasaur/1'}))
        self.assertEqual(self.run_import(review=review)['coveredForms'], 2)

    def test_zip_and_windows_paths(self):
        archive = self.root / 'seed.zip'
        with zipfile.ZipFile(archive, 'w') as z:
            z.writestr('folder\\0001 Bulbasaur.png', png('red'))
        self.assertEqual(self.run_import(archive)['coveredForms'], 1)
        for name in ('../escape.png', 'C:\\escape.png', '/absolute.png', 'x:stream.png'):
            with self.subTest(name=name), zipfile.ZipFile(archive, 'w') as z:
                z.writestr(name, png('red'))
            with self.assertRaises(ValueError):
                self.run_import(archive)

    def test_corrupt_image_report_and_corrupt_cache_failure(self):
        (self.seed / '0001 Bulbasaur.png').write_bytes(png('red'))
        (self.seed / 'broken.png').write_bytes(b'not an image')
        self.assertEqual(self.run_import()['invalidImages'], 1)
        next((self.out / 'originals').iterdir()).write_bytes(b'corrupt')
        with self.assertRaises(ValueError):
            self.run_import()

    def test_interrupted_reports_regenerate(self):
        (self.seed / '0001 Bulbasaur.png').write_bytes(png('red'))
        self.run_import()
        (self.out / 'corpus-index.json').unlink()
        (self.out / 'missing.json').write_text('interrupted')
        self.assertEqual(self.run_import()['coveredForms'], 1)
        self.assertIsInstance(json.loads((self.out / 'missing.json').read_text()), list)

    def test_git_output_guard(self):
        repo = self.root / 'repo'
        repo.mkdir()
        subprocess.run(['git', 'init', '-q', str(repo)], check=True)
        (repo / '.gitignore').write_text('/private/\n')
        with self.assertRaises(ValueError):
            art.guard_output(repo / 'assets', self.seed)
        self.assertEqual(art.guard_output(repo / 'private/art', self.seed), (repo / 'private/art').resolve())
        (repo / 'private').mkdir()
        (repo / 'private/tracked').write_text('tracked')
        subprocess.run(['git', '-C', str(repo), 'add', '-f', 'private/tracked'], check=True)
        with self.assertRaises(ValueError):
            art.guard_output(repo / 'private', self.seed)

    def test_limits_and_nested_output(self):
        with self.assertRaises(ValueError):
            art.guard_output(self.seed / 'output', self.seed)
        original = art.MAX_FILE
        self.addCleanup(setattr, art, 'MAX_FILE', original)
        art.MAX_FILE = 2
        (self.seed / '0001 Bulbasaur.png').write_bytes(png('red'))
        with self.assertRaises(ValueError):
            self.run_import()


if __name__ == '__main__':
    unittest.main()
