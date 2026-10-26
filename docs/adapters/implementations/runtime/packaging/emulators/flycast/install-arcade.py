#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Create a new arcade-capable package from the owned pinned Dojo bundle.

No selection changes, firmware downloads or personal EEPROM/save migration.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import importlib.util

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--base',type=Path,required=True);p.add_argument('--destination',type=Path,required=True)
p.add_argument('--firmware-root',type=Path,action='append',default=[])
args=p.parse_args();base=args.base.resolve();dest=args.destination
if not dest.is_absolute() or dest.exists() or dest.is_symlink():raise SystemExit('Choose a new absolute package directory')
manifest=json.loads((base/'manifest.json').read_text())
if manifest['source']!='d0e47e572b1e7b355e88bda8308c89d0c5156cbf':raise SystemExit('Unreviewed native runtime')
for name,expected in manifest['files'].items():
    path=base/name
    if not path.resolve().is_relative_to(base) or hashlib.sha256(path.read_bytes()).hexdigest()!=expected:
        raise SystemExit('Existing bundle changed')
recipe=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('arcade',recipe/'arcade.py')
arcade=importlib.util.module_from_spec(spec);spec.loader.exec_module(arcade)
catalog=json.loads((recipe/'arcade-catalog.json').read_text())
shutil.copytree(base,dest,symlinks=True)
for name in ('arcade.py','arcade-catalog.json'):shutil.copyfile(recipe/name,dest/name)
(dest/'licenses').mkdir(exist_ok=True)
for name in ('arcade-catalog.NOTICE','LICENSE.arcade-metadata'):shutil.copyfile(recipe/name,dest/'licenses'/name)
for name in catalog['bios']:
    for suffix in ('.zip','.7z'):
        filename=name+suffix;target=dest/'firmware'/filename
        candidates=[folder/filename for folder in args.firmware_root if (folder/filename).is_file()]
        if target.exists():candidates.insert(0,target)
        definitions=catalog['bios'][name]
        names={b['name'] for b in definitions if b['kind']!='Copy'}
        crcs={b['crc'] for b in definitions if b['crc'] and b['kind']!='Copy'}
        hashes={arcade.archive_identity([entry for entry in arcade.archive(path)
                if entry['name'] in names or entry['crc'] in crcs]) for path in candidates}
        if len(hashes)>1:raise SystemExit('Conflicting owned firmware copies: '+filename)
        if candidates and not target.exists():shutil.copyfile(candidates[0],target)
subprocess.run([sys.executable,str(recipe/'bundle.py'),str(dest),manifest.get('onlineRelay') or '--nearby-only',manifest['renderer']],check=True)
print('New arcade package staged; installed selection and personal progress unchanged')
