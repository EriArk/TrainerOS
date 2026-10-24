#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify an exact reviewed patch stack, or advance a known older stack.

Overlapping patches cannot be verified independently in reverse. Reconstruct
each prefix from Git's pinned originals in a private directory instead. Refuse
unrecognized modifications before writing any source file.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile


def stack(source, names, check):
    recipe = Path(__file__).resolve().parent
    paths = set()
    for name in names:
        for line in (recipe / name).read_text().splitlines():
            if line.startswith('+++ b/'):
                paths.add(line[6:])
    def snapshot(root):
        return {name: (root / name).read_bytes() if (root / name).exists() else None for name in paths}
    actual = snapshot(source)
    with tempfile.TemporaryDirectory(prefix='traineros-patches-') as directory:
        target = Path(directory)
        for name in paths:
            original = subprocess.run(['git', '-C', str(source), 'show', 'HEAD:' + name], capture_output=True)
            if original.returncode == 0:
                path = target / name;path.parent.mkdir(parents=True, exist_ok=True);path.write_bytes(original.stdout)
            else:
                # Only a file introduced by a reviewed patch may be absent.
                assert any(('+++ b/' + name) in (recipe / p).read_text() and
                           '--- /dev/null' in (recipe / p).read_text() for p in names), name
        known = actual == snapshot(target)
        for name in names:
            subprocess.run(['git', 'apply', str(recipe / name)], cwd=target, check=True)
            known = known or actual == snapshot(target)
        expected = snapshot(target)
        assert known, 'Unrecognized source changes; preserve them and review before rebuilding'
        if check:
            assert actual == expected, 'Reviewed patches are missing'
        elif actual != expected:
            for name, data in expected.items():
                path = source / name;path.parent.mkdir(parents=True, exist_ok=True)
                staging = path.with_name(path.name + '.traineros-patch')
                assert not staging.exists()
                staging.write_bytes(data);staging.replace(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    stack(args.source, ['pdp-stat.patch', 'relay-retry.patch', 'coordination-stream.patch',
                       'coordination-timeout.patch', 'pdp-threading.patch'], args.check)
    stack(args.source / 'ext/aemu_postoffice', ['socket-timeout.patch'], args.check)
    print('REVIEWED_PATCH_STACK_VERIFIED')


if __name__ == '__main__':
    main()
