#!/usr/bin/python3 -I
# SPDX-License-Identifier: GPL-3.0-or-later
"""Install a reviewed capture compositor beside Armada; never replace its RPM.

Run as root with the built binary and its reviewed SHA256. Session restart is
deliberately separate: finish any live game through TrainerOS first.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import tempfile
import time

BASELINE = 'd71bff6e6a1a493469ace879c29041de221612e1a83902c03be3f3434d29338a'
TARGET = Path('/var/opt/traineros/system/gamescope')


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('sha256')
    args = parser.parse_args()
    if os.geteuid() != 0:
        parser.error('Root installation required')
    if digest(Path('/usr/bin/gamescope')) != BASELINE:
        parser.error('Unreviewed Armada compositor; review the new baseline first')
    # Copy the exact bytes checked below, avoiding a second read of user staging.
    data = args.binary.read_bytes()
    if len(args.sha256) != 64 or hashlib.sha256(data).hexdigest() != args.sha256:
        parser.error('Artifact digest mismatch')
    notices = {name: (Path(__file__).resolve().parent / name).read_bytes()
               for name in ('LICENSE.gamescope', 'LICENSE.punktfunk')}
    TARGET.parent.mkdir(parents=True, exist_ok=True)
    if TARGET.parent.is_symlink() or TARGET.is_symlink():
        parser.error('Unexpected installation symlink')
    stage = Path(tempfile.mkdtemp(prefix='.gamescope-', dir=TARGET.parent))
    try:
        (stage / 'gamescope').write_bytes(data)
        (stage / 'gamescope').chmod(0o755)
        sums = f'{BASELINE}  /usr/bin/gamescope\n{args.sha256}  {TARGET}/gamescope\n'
        (stage / 'SHA256SUMS').write_text(sums)
        (stage / 'SHA256SUMS').chmod(0o644)
        for name, content in notices.items():
            (stage / name).write_bytes(content)
            (stage / name).chmod(0o644)
        stage.chmod(0o755)
        if TARGET.exists():
            TARGET.rename(TARGET.with_name('gamescope-before-' + str(time.time_ns())))
        stage.rename(TARGET)
    finally:
        if stage.exists():
            shutil.rmtree(stage)
    print('Installed beside system Gamescope:', args.sha256)


if __name__ == '__main__':
    main()
