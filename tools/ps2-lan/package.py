#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Package built PS2 helpers; no game, BIOS, emulator binary or personal data."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build',type=Path,required=True)
parser.add_argument('--transport',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();source=Path(__file__).resolve().parent
files={name:source/name for name in ('launcher.py','disc.py','session_config.py','profiles.json')}
files.update({'ps2_endpoint':args.build/'ps2_endpoint','libchdr.so':args.build/'_deps/chdr-build/libchdr.so','stream_transport':args.transport/'stream_transport'})
for path in files.values():
    if not path.is_file() or path.stat().st_size>64*1024*1024:raise ValueError('Missing or oversized package file')
args.output.mkdir(mode=0o755)
hashes={}
for name,path in files.items():
    target=args.output/name;shutil.copyfile(path,target);target.chmod(0o755 if name in ('ps2_endpoint','stream_transport') else 0o644)
    hashes[name]=hashlib.sha256(target.read_bytes()).hexdigest()
for kind,root in (('chdr',args.build/'_deps/chdr-src'),('datachannel',args.transport/'_deps/datachannel-src')):
    notices=[p for p in root.rglob('*') if p.is_file() and not p.is_symlink() and '.git' not in p.parts and p.name.lower().startswith(('license','copying'))]
    if not notices:raise ValueError('Missing dependency notices')
    for path in notices:
        target=args.output/'notices'/kind/path.relative_to(root);target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
shutil.copyfile(source.parents[1]/'LICENSE',args.output/'notices/TrainerOS.LICENSE')
(args.output/'manifest.json').write_text(json.dumps({'protocol':1,'settings':'eclx1-group-own-cards-v2',
    'chdrSource':'47d794151b2b64f6aba60304e6480f4f5a2bc24b','files':hashes},indent=2)+'\n')
print('PS2_PACKAGE_READY')
