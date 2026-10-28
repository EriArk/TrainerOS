"""Add the approved starter/racing collection to a face-bridge revision-8 pack."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def build(pack):
    collection = ROOT/'assets/avatar/collections/starter-racing-v1'
    catalogue = json.loads((collection/'collection.json').read_text())
    m = json.loads((pack/'manifest.json').read_text()); assert m['revision'] == 8
    specs = {'sundress': ('top', 'dress', 'shirtColor'),
             'everyday-tee': ('top', 'ivory', 'shirtColor'),
             'denim-breeches': ('bottom', 'denim', 'shortsColor'),
             'canvas-shoes': ('shoes', 'ivory', 'shoesColor')}
    for prefix, colour in [('', 'race'), ('rose-', 'rose')]:
        for part, slot, tint in [('suit', 'top', colour), ('helmet', 'headwear', 'ivory' if prefix else 'race'),
                                 ('boots', 'shoes', colour), ('gloves', 'gloves', 'plum' if prefix else 'dark')]:
            specs[prefix+'racing-'+part] = (slot, tint, {'top': 'shirtColor', 'headwear': 'headwearColor',
                                                       'shoes': 'shoesColor', 'gloves': 'shirtColor'}[slot])
    for name, (slot, tint, role) in specs.items():
        record = catalogue['items'][name]; data = (collection/record['file']).read_bytes()
        assert hashlib.sha256(data).hexdigest() == record['sha256']
        file = 'wear-'+record['file']; (pack/file).write_bytes(data); m['files'][file] = record['sha256']
        entry = dict(slot=slot, file=file, sha256=record['sha256'], tintMaterial=tint,
                     tintRole=role, defaultColor='#'+catalogue['palette'][tint])
        if name in ('sundress', 'racing-suit', 'rose-racing-suit'): entry['covers'] = ['bottom']
        if name.endswith('racing-suit'): entry['companions'] = [name.replace('suit', 'gloves')]
        if name.endswith('racing-helmet'): entry['hidesHair'] = True
        # Gloves are part of the one-piece racing outfit and keep their contrast.
        if slot == 'gloves': entry.pop('tintRole'); entry.pop('tintMaterial')
        m['wearables'][name] = entry
    m['starterSets'] = catalogue['starterSets']
    m['outfitSets'] = catalogue['sets']
    m['starterAvailability'] = 'included-with-every-character'
    m['revision'] = 9
    (pack/'manifest.json').write_text(json.dumps(m, indent=2)+'\n', encoding='utf8', newline='\n')
    print('Pack revision 9: 12 new garments; two free starter sets; reversible helmet hair hiding')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('pack', type=Path); build(p.parse_args().pack)
