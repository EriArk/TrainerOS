"""Package approved separate garments and reversible hair fits into the shared pack.

Run after build_pack/cartoon/expressions and build_golf_clothes; idempotent.
The approved body, faces and garment GLBs are copied without geometry changes.
"""
import hashlib
import json
from pathlib import Path
import numpy as np
from build_pack import GLB
from build_golf_clothes import normals
from golf_hair_fit import fit_positions

ROOT = Path(__file__).resolve().parents[2]


def main():
    pack = ROOT / 'assets/avatar/packs/trainer-avatar-1'
    collection = ROOT / 'assets/avatar/collections/golf-v1'
    manifest = json.loads((pack / 'manifest.json').read_text())
    source = GLB(pack / 'template.glb')
    catalogue = json.loads((collection / 'collection.json').read_text())
    slots = {'top': ['polo-short-sleeve', 'polo-sleeveless'],
             'bottom': ['golf-shorts', 'pleated-skirt'],
             'socks': ['ankle-socks', 'crew-socks'],
             'shoes': ['golf-shoes-classic', 'golf-shoes-sport'],
             'headwear': ['golf-cap', 'golf-visor']}
    wearables = {}
    for slot, items in slots.items():
        for item in items:
            record = catalogue['items'][item]
            data = (collection / record['file']).read_bytes()
            assert hashlib.sha256(data).hexdigest() == record['sha256']
            file = 'wear-' + record['file']
            (pack / file).write_bytes(data)
            entry = {'slot': slot, 'file': file, 'sha256': record['sha256']}
            if slot in ('top', 'bottom'):
                tint = 'ivory' if item in ('polo-short-sleeve', 'pleated-skirt') else 'forest'
                entry.update(tintMaterial=tint, tintRole='shirtColor' if slot == 'top' else 'shortsColor',
                             defaultColor='#' + catalogue['palette'][tint])
            wearables[item] = entry
            manifest['files'][file] = record['sha256']
    fits = {}
    for hat in slots['headwear']:
        for hair, node in manifest['hairNodes'].items():
            primitives = source.g['meshes'][source.g['nodes'][node]['mesh']]['primitives']
            patches = []
            for index, primitive in enumerate(primitives):
                position = source.acc(primitive['attributes']['POSITION'])
                faces = source.acc(primitive['indices']).reshape(-1, 3)
                fitted = fit_positions(position, hat)
                if hair == 'ponytail':
                    # The tied tail exits behind the band; only fit the scalp.
                    influence = np.clip((position[:, 2] + .16) / .04, 0, 1)
                    fitted = position + (fitted-position)*influence[:, None]
                assert np.isfinite(fitted).all() and np.linalg.norm(fitted-position, axis=1).max() < .08
                for attribute, values in [('POSITION', fitted), ('NORMAL', normals(fitted, faces))]:
                    accessor = primitive['attributes'][attribute]
                    original = source.acc(accessor).astype('<f4').tobytes()
                    data = values.astype('<f4').tobytes()
                    file = f'fit-{hat}-{hair}-{index}-{attribute.lower()}.bin'
                    (pack / file).write_bytes(data)
                    sha = hashlib.sha256(data).hexdigest()
                    manifest['files'][file] = sha
                    patches.append({'accessor': accessor, 'file': file, 'sha256': sha,
                                    'sourceSha256': hashlib.sha256(original).hexdigest(),
                                    'min': values.min(axis=0).tolist(), 'max': values.max(axis=0).tolist()})
            fits[hat + '/' + hair] = patches
    manifest.update(revision=7, resolvedGeometry="selected-meshes-v1", wearables=wearables, headwearFits=fits,
                    wardrobeDefaults={'top': 'tank', 'bottom': 'base-shorts', 'socks': 'none',
                                      'shoes': 'sandals', 'headwear': 'none'})
    encoded = json.dumps(manifest, indent=2) + '\n'
    assert len(encoded.encode()) < 65536
    # Pack identity hashes the literal manifest bytes on Windows and Linux.
    (pack / 'manifest.json').write_text(encoded, encoding='utf-8', newline='\n')
    print('Pack revision 7:', len(wearables), 'garments;', len(fits), 'hair fits')


if __name__ == '__main__':
    main()
