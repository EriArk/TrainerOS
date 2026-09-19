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

    def test_explicit_selection_preserves_other_candidates(self):
        (self.seed / '0001 Bulbasaur.png').write_bytes(png('red'))
        (self.seed / '1Bulbasaur.png').write_bytes(png('blue'))
        selection = self.root / 'selection.json'
        selection.write_text(json.dumps({'bulbasaur/1': art.digest(png('blue'))}))
        result = art.run(self.seed, self.out, self.ref, 'fixture', selection_path=selection)
        self.assertEqual(result['coveredForms'], 1)
        self.assertEqual(result['images'], 2)
        selection.write_text(json.dumps({'venusaur/3': art.digest(png('blue'))}))
        with self.assertRaises(ValueError):
            art.run(self.seed, self.out, self.ref, 'fixture', selection_path=selection)

    def test_regional_names_and_punctuation_are_identity(self):
        species, _ = art.inventory({'entries': [
            {'id': 'unown', 'number': 201, 'name': 'Unown', 'forms': [
                {'id': 'q', 'name': '?'}, {'id': 'e', 'name': '!'}]},
            {'id': 'tauros', 'number': 128, 'name': 'Tauros', 'forms': [
                {'id': 'aqua', 'name': 'Paldean Form (Aqua Breed)'},
                {'id': 'blaze', 'name': 'Paldean Form (Blaze Breed)'}]},
            {'id': 'darmanitan', 'number': 555, 'name': 'Darmanitan', 'forms': [
                {'id': 'zen1', 'name': 'Zen Mode'}, {'id': 'zen2', 'name': 'Zen Mode'}]}
        ]})
        self.assertEqual(art.candidates('0201 Unown.png', species)[0], [])
        self.assertEqual(art.candidates('0201 Unown!.png', species)[0], ['unown/e'])
        self.assertEqual(art.candidates('0128 Tauros Paldea Aqua.png', species)[0], ['tauros/aqua'])
        self.assertEqual(len(art.candidates('0555 Darmanitan Zen Mode.png', species)[0]), 2)

    def test_review_html_escapes_source_names(self):
        (self.seed / '0001 Bulbasaur.png').write_bytes(png('red'))
        self.run_import()
        _, targets = art.inventory(json.loads(self.ref.read_text()))
        art.write_review(self.out, targets, [{'speciesId': 'venusaur', 'sourceName': '<script>bad</script>',
                         'path': 'originals/x.png', 'sha256': 'a' * 64, 'candidates': []}], {})
        html = (self.out / 'review.html').read_text(encoding='utf8')
        self.assertNotIn('<script>', html)
        self.assertIn('&lt;script&gt;', html)

    def test_supplement_provenance_does_not_force_proposed_target(self):
        (self.seed / '0001 Bulbasaur.png').write_bytes(png('red'))
        supplement = self.root / 'supplement'
        supplement.mkdir()
        data = png('blue')
        sha = art.digest(data)
        (supplement / (sha + '.png')).write_bytes(data)
        (supplement / 'acquisition.json').write_text(json.dumps({'page': {
            'file': sha + '.png', 'sha256': sha, 'sourceName': '0003 Venusaur unknown.png',
            'source': 'fixture:separate-source', 'target': 'venusaur/3'}}))
        result = art.run(self.seed, self.out, self.ref, 'fixture:seed', supplement=supplement)
        self.assertEqual(result['coveredForms'], 1)
        report = json.loads((self.out / 'corpus-index.json').read_text())
        self.assertEqual(report['images'][-1]['source'], 'fixture:separate-source')
        self.assertEqual(report['images'][-1]['candidates'], [])
        (supplement / (sha + '.png')).unlink()
        with self.assertRaises(ValueError):
            art.run(self.seed, self.out, self.ref, 'fixture:seed', supplement=supplement)


if __name__ == '__main__':
    unittest.main()
