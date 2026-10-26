#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Finish a new native package using explicitly supplied owned firmware."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

p = argparse.ArgumentParser()
p.add_argument('package', type=Path)
p.add_argument('firmware', type=Path)
p.add_argument('--renderer', choices=('vulkan', 'opengl'), default='vulkan')
a = p.parse_args()
assert (a.package / 'bin/flycast').is_file()
assert not (a.package / 'manifest.json').exists()
assert not (a.package / 'firmware').exists()
for name, size in (('dc_boot.bin', 2 * 1024 * 1024), ('dc_flash.bin', 128 * 1024)):
    assert (a.firmware / name).stat().st_size == size
(a.package / 'firmware').mkdir(mode=0o700)
for name in ('dc_boot.bin', 'dc_flash.bin'):
    shutil.copyfile(a.firmware / name, a.package / 'firmware' / name)
    (a.package / 'firmware' / name).chmod(0o600)
shutil.copyfile(Path(__file__).with_name('catalog.json'), a.package / 'catalog.json')
files = {name: hashlib.sha256((a.package / name).read_bytes()).hexdigest()
         for name in ('bin/flycast', 'catalog.json', 'firmware/dc_boot.bin', 'firmware/dc_flash.bin')}
(a.package / 'manifest.json').write_text(json.dumps({
    'protocol': 1, 'source': '5aa091fde632fb332c8d8c34e280d62dc951954c',
    'settings': 'dcnet-owned-vmu-v1', 'renderer': a.renderer, 'files': files}, indent=2) + '\n')
