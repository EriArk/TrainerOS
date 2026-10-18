#!/usr/bin/env python3
"""Move melonDS's writable firmware working copy out of a read-only BIOS mount.

Run with melonDS closed. Original firmware, BIOS files and saves stay untouched.
The destination must be a private writable directory visible inside the runtime.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('--directory', type=Path, required=True)
    args = parser.parse_args()
    for proc in Path('/proc').glob('[0-9]*'):
        try:
            if (proc / 'exe').readlink().name.lower() == 'melonds':
                raise SystemExit('Close melonDS first.')
        except OSError:
            pass
    original = args.config.read_bytes()
    text = original.decode('utf-8')
    matches = list(re.finditer(r'^FirmwarePath = ("[^"\r\n]+")\r?$', text, re.M))
    if len(matches) != 1:
        raise SystemExit('Expected one DS firmware path; review multi-instance configs separately.')
    source = Path(json.loads(matches[0][1])).resolve(strict=True)
    directory = args.directory.expanduser().resolve()
    target = directory / 'firmware.bin'
    if source == target:
        print('Private firmware already configured.')
        return
    data = source.read_bytes()
    if len(data) not in (131072, 262144, 524288):
        raise SystemExit('Unexpected firmware size; original retained.')
    directory.mkdir(parents=True, exist_ok=True)
    if target.exists():
        if target.read_bytes() != data:
            raise SystemExit('Destination differs; will not overwrite firmware settings.')
    else:
        with target.open('xb') as output:
            output.write(data)
        target.chmod(0o600)
    if hashlib.sha256(target.read_bytes()).digest() != hashlib.sha256(data).digest():
        raise SystemExit('Firmware readback mismatch.')
    backup = args.config.with_name(args.config.name + '.before-launch80-firmware')
    if backup.exists():
        if backup.read_bytes() != original:
            raise SystemExit('Different config backup exists; review first.')
    else:
        backup.write_bytes(original)
    match = matches[0]
    text = text[:match.start(1)] + json.dumps(str(target)) + text[match.end(1):]
    temporary = args.config.with_name(args.config.name + '.launch80-new')
    with temporary.open('x', encoding='utf-8', newline='') as output:
        output.write(text)
    temporary.chmod(args.config.stat().st_mode)
    os.replace(temporary, args.config)
    print('Configured verified private firmware copy; original and backup retained.')


if __name__ == '__main__':
    main()
