#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage the BBA transport alongside the existing pinned Dolphin bundle."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--transport', type=Path, required=True, help='Built stream_transport CMake directory')
parser.add_argument('--endpoint', type=Path, required=True, help='Built bba_endpoint executable')
parser.add_argument('--output', type=Path, required=True, help='New dolphin-lan staging directory')
args = parser.parse_args()
binary = args.transport / 'stream_transport'
dependency = args.transport / '_deps/datachannel-src'
notices = [p for p in dependency.rglob('*') if p.is_file() and not p.is_symlink()
           and '.git' not in p.parts and p.name.lower().startswith(('license', 'copying'))]
if not binary.is_file() or not 0 < binary.stat().st_size <= 64 * 1024 * 1024 or not notices:
    raise ValueError('Build the transport and retain dependency notices first')
with binary.open('rb') as file:
    header = file.read(20)
if header[:6] != b'\x7fELF\x02\x01':
    raise ValueError('Expected a 64-bit little-endian Linux executable')
architecture = {183: 'aarch64', 62: 'x86_64'}[int.from_bytes(header[18:20], 'little')]
if args.endpoint.read_bytes()[:20]!=binary.read_bytes()[:20]:raise ValueError('Endpoint architecture mismatch')
args.output.mkdir(mode=0o755)  # Never overwrite an installed/previous package.
shutil.copyfile(binary, args.output / 'stream_transport')
(args.output / 'stream_transport').chmod(0o755)
shutil.copyfile(args.endpoint,args.output/'bba_endpoint')
(args.output/'bba_endpoint').chmod(0o755)
for path in notices:
    target = args.output / 'notices/datachannel' / path.relative_to(dependency)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(path, target)
shutil.copyfile(Path(__file__).resolve().parents[3] / 'LICENSE', args.output / 'notices/TrainerOS.LICENSE')
manifest = {'protocol': 1, 'settings': 'dolphin-bba-group-isolated-v2', 'architecture': architecture,
            'transportSource': '6b1e2e620f1e37f0eafeee702eaea0043cb305fd',
            'endpointSha256': hashlib.sha256(args.endpoint.read_bytes()).hexdigest(),
            'sha256': hashlib.sha256(binary.read_bytes()).hexdigest()}
(args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('DOLPHIN_LAN_PACKAGE_READY')
