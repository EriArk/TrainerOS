#!/usr/bin/env python3
"""Package the Linux reader: package.py BINARY PPSSPP_SOURCE OUTPUT_DIRECTORY."""
import hashlib
import json
import shutil
import struct
import subprocess
import sys
from pathlib import Path

binary, source, output = map(Path,sys.argv[1:])
recipe=Path(__file__).resolve().parent
revision='fa50bb1976065c4f8b1b47af227d367fe9771555'
if subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()!=revision:
    raise SystemExit('Unreviewed PPSSPP source')
data=binary.read_bytes()
if len(data)<64 or data[:6]!=b'\x7fELF\x02\x01':
    raise SystemExit('Expected little-endian ELF64')
arch={183:'aarch64',62:'x86_64'}.get(struct.unpack_from('<H',data,18)[0])
if not arch:
    raise SystemExit('Unsupported architecture')
output.mkdir(parents=True,exist_ok=False)
shutil.copy2(binary,output/'psp-content-probe')
(output/'psp-content-probe').chmod(0o755)
shutil.copy2(source/'LICENSE.TXT',output/'PPSSPP-LICENSE.txt')
shutil.copytree(recipe,output/'source/tools/ppsspp-content',ignore=shutil.ignore_patterns('__pycache__'))
header=Path('src/integrations/adventure/standalone/PspContentIdentity.h')
(output/'source'/header.parent).mkdir(parents=True)
shutil.copy2(recipe.parent.parent/header,output/'source'/header)
shutil.copy2(recipe.parent.parent/'LICENSE',output/'TrainerOS-LICENSE.txt')
shutil.copy2(recipe.parent.parent/'LICENSE',output/'source/LICENSE')
(output/'manifest.json').write_text(json.dumps({'recipe':'psp-content-v1','source':revision,'architecture':arch,
    'files':{'psp-content-probe':hashlib.sha256(data).hexdigest()}},indent=2)+'\n')
print(output)
