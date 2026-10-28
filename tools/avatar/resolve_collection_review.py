"""Resolve four collection looks with the real native resolver, without profile writes."""
import argparse
import json
import os
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__);p.add_argument('resolver',type=Path);p.add_argument('pack',type=Path);p.add_argument('output',type=Path)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
looks={
    'sundress':dict(top='sundress',bottom='base-shorts',shoes='sandals',headwear='none',shirtColor='#eab2a1'),
    'denim':dict(top='everyday-tee',bottom='denim-breeches',shoes='canvas-shoes',headwear='none',shirtColor='#f4f0df',shortsColor='#527899'),
    'racing-classic':dict(top='racing-suit',bottom='base-shorts',shoes='racing-boots',headwear='racing-helmet',shirtColor='#3d6d7e',shoesColor='#3d6d7e',headwearColor='#3d6d7e'),
    'racing-rose':dict(top='rose-racing-suit',bottom='base-shorts',shoes='rose-racing-boots',headwear='rose-racing-helmet',shirtColor='#d891ab',shoesColor='#d891ab',headwearColor='#f4f0df')}
env=os.environ.copy()
if os.name=='nt':env['PATH']='C:/Users/EriArk/msys64/ucrt64/bin;'+env['PATH']
for name,items in looks.items():
    female=name in ('sundress','racing-rose')
    appearance=dict(schema=3,pack='trainer-avatar-1',hair='bob' if female else 'crop',eyes='rogue-feminine' if female else 'innocent-base',
                    skinColor='#614132' if female else '#f0c4a5',hairColor='#975743',eyeColor='#98602d',
                    shirtColor='#ec9e8d',shortsColor='#577c87',socks='none',socksColor='#f4f0df',shoesColor='#f4f0df',headwearColor='#234f43')
    appearance.update(items)
    for suffix in (['','-without-helmet'] if name.startswith('racing') else ['']):
        if suffix:appearance['headwear']='none'
        path=a.output/(name+suffix+'-appearance.json');path.write_text(json.dumps(appearance))
        subprocess.run([str(a.resolver.resolve()),str(a.pack.resolve()),str((a.output/'cache').resolve()),str((a.output/(name+suffix+'.json')).resolve()),str(path.resolve())],env=env,check=True)
