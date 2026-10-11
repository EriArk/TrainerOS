#!/usr/bin/env python3
"""Fingerprint an already built Linux bundle; does not install or publish it."""
import hashlib
import json
from pathlib import Path
import sys
from urllib.parse import urlsplit

root = Path(sys.argv[1]).resolve(strict=True)
online = sys.argv[2] if sys.argv[2] != '--nearby-only' else ''
endpoint = urlsplit(online)
renderer = sys.argv[3] if len(sys.argv) > 3 else 'vulkan'
if online and (endpoint.scheme != 'wss' or not endpoint.hostname or endpoint.username or endpoint.query or endpoint.fragment):
    raise SystemExit('Supply the operator-managed wss endpoint, not a player address')
if renderer not in ('vulkan', 'opengl'):
    raise SystemExit('Unknown renderer')
files = {}
# Cheats and unrelated desktop metadata need not be copied into a runtime bundle.
for directory in ('bin', 'lib', 'share/RMG'):
    for path in sorted((root / directory).rglob('*')):
        if not path.is_file() or 'Cheats' in path.parts:
            continue
        if not path.resolve().is_relative_to(root):
            raise SystemExit(f'External bundle symlink: {path}')
        files[path.relative_to(root).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
manifest = dict(protocol=1, source='4777ed3f1fb7f1e4a0e70438a27dc4a5e9fb1927',
                serverSource='bf0f28746971fcb6341fac55c0d2d19f646a6e09',
                onlineServer=online, renderer=renderer, files=files)
(root / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
print(f'Fingerprinted {len(files)} runtime files; device/service acceptance remains separate.')
