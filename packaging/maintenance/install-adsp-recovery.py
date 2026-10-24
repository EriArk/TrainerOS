#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Install an idempotent rootfs-stage ADSP retry on Armada; no reboot or DSP stop."""
import os
from pathlib import Path
import subprocess

HELPER = Path('/var/opt/traineros/adsp/restore-adsp.py')
DROPIN = Path('/etc/systemd/system/armada-device-quirks.service.d/50-traineros-adsp.conf')


def main():
    if os.geteuid() or not Path('/usr/lib/systemd/system/armada-device-quirks.service').is_file():
        raise SystemExit('Requires root on Armada with its device-quirks service')
    subprocess.run(['systemctl', 'is-enabled', '--quiet', 'armada-device-quirks.service'], check=True)
    source = Path(__file__).with_name('restore-adsp.py').read_bytes()
    dropin = ('[Unit]\nAfter=local-fs.target\n\n[Service]\n'
              f'ExecStartPost=/usr/bin/python3 {HELPER}\n').encode()
    files = [(HELPER, source, 0o755), (DROPIN, dropin, 0o644)]
    for path, data, _ in files:
        if path.is_symlink() or (path.exists() and path.read_bytes() != data):
            raise SystemExit(f'Preserve existing different override: {path}')
    for path, data, mode in files:
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists():
            with path.open('xb') as output:
                output.write(data)
            path.chmod(mode)
    subprocess.run(['systemctl', 'daemon-reload'], check=True)
    # Do not restart device-quirks or the desktop. Only the safe retry runs now.
    subprocess.run(['/usr/bin/python3', str(HELPER)], check=True, timeout=20)
    print('ADSP boot recovery installed; no reboot performed')


if __name__ == '__main__':
    main()
