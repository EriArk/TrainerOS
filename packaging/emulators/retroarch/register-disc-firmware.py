#!/usr/bin/env python3
"""Register existing Sega CD firmware in a TrainerOS installation profile.

No firmware is downloaded or replaced. Existing unrelated profile fields stay
intact; keep the backup to undo this bounded installation repair.
"""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', type=Path, required=True)
    parser.add_argument('--system-directory', type=Path, required=True)
    args = parser.parse_args()
    for proc in Path('/proc').glob('[0-9]*'):
        try:
            if (proc / 'exe').readlink().name.lower() == 'retroarch':
                raise SystemExit('Close RetroArch before changing its profile.')
        except OSError:
            pass
    original = args.profile.read_bytes()
    profile = json.loads(original)
    if profile.get('version') != 1:
        raise SystemExit('Unsupported integration profile.')
    hashes = {}
    root = args.system_directory.expanduser().resolve(strict=True)
    for name in ('bios_CD_U.bin', 'bios_CD_E.bin', 'bios_CD_J.bin'):
        path = (root / name).resolve(strict=True)
        if path.parent != root or not path.is_file() or path.stat().st_size != 131072:
            raise SystemExit('Invalid Sega CD firmware: ' + name)
        hashes[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    firmware = profile.setdefault('discFirmware', {})
    if firmware.get('segacd') == hashes:
        print('Sega CD firmware already registered.')
        return
    if 'segacd' in firmware:
        raise SystemExit('Existing Sega CD hashes differ; review before replacing.')
    backup = args.profile.with_name(args.profile.name + '.before-launch80')
    if backup.exists() and backup.read_bytes() != original:
        raise SystemExit('Different profile backup exists; review first.')
    if not backup.exists():
        backup.write_bytes(original)
    firmware['segacd'] = hashes
    temporary = args.profile.with_name(args.profile.name + '.launch80-new')
    with temporary.open('x', encoding='utf-8') as output:
        json.dump(profile, output, indent=2)
        output.write('\n')
    temporary.chmod(args.profile.stat().st_mode)
    temporary.replace(args.profile)
    print('Registered three existing Sega CD firmware hashes; backup retained.')


if __name__ == '__main__':
    main()
