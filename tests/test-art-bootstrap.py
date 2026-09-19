import importlib.util
import json
import tempfile
import unittest
import hashlib
from pathlib import Path
from PIL import Image
spec = importlib.util.spec_from_file_location("bootstrap", Path(__file__).resolve().parents[1] / "tools/bootstrap-classic-art.py")
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)


class BootstrapTests(unittest.TestCase):
    def test_transparent_margins_tall_wide_and_no_opaque_background_removal(self):
        for shape in ((8, 90), (90, 8)):
            image = Image.new("RGBA", (100, 100)); image.paste((255, 0, 0, 255), (3, 4, 3+shape[0], 4+shape[1]))
            result, metadata = module.frame(image, (96, 96))
            self.assertEqual(result.size, (96, 96))
            self.assertEqual(metadata['sourceContentSize'], list(shape))
            bbox = result.getchannel('A').getbbox()
            self.assertGreaterEqual(min(bbox), 0)
            self.assertLessEqual(max(bbox), 96)
        _, meta = module.frame(Image.new('RGB', (100, 100), 'white'), (96, 96))
        self.assertEqual(meta['alphaBounds'], [0, 0, 100, 100])

    def test_empty_and_unsafe_framing_rejected(self):
        with self.assertRaises(ValueError): module.frame(Image.new('RGBA', (10, 10)), (96, 96))
        for adjustment in ({'scale': 3}, {'x': .5}, {'y': float('nan')}):
            with self.assertRaises(ValueError): module.frame(Image.new('RGBA', (10, 10), 'red'), (96, 96), adjustment)

    def test_bootstrap_identity_hash_and_new_directory_gate(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); corpus = root/'corpus'; corpus.mkdir()
            image = corpus/'image.png'; Image.new('RGBA', (20, 20), 'red').save(image)
            digest = hashlib.sha256(image.read_bytes()).hexdigest()
            module.write(corpus/'corpus-index.json', {'images': [{'sha256':digest, 'path':'image.png', 'sourceName':'synthetic',
                'candidates':['test/1'], 'confidence':'reviewed'}], 'associations':{'test/1':digest}})
            module.write(root/'reference.json', {'entries':[{'id':'test','forms':[{'id':'1'},{'id':'2'}]}]})
            module.write(root/'profiles.json', {'profiles':{'pokedexListArt':{'size':[96,96]}}})
            output = root/'out'
            built = module.build(corpus,root/'reference.json',root/'profiles.json',output)
            self.assertIsNone(built['targets']['test/2']['selected'])
            self.assertIsNone(built['targets']['test/2']['fallback'])
            self.assertEqual(built['targets']['test/1']['selected'],digest)
            with self.assertRaises(ValueError): module.build(corpus,root/'reference.json',root/'profiles.json',output)
            image.write_bytes(b'changed')
            with self.assertRaises(ValueError): module.build(corpus,root/'reference.json',root/'profiles.json',root/'bad')


if __name__ == '__main__': unittest.main()
