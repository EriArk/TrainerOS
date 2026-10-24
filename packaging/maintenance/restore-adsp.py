#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Retry an offline Qualcomm ADSP after rootfs firmware becomes available.

Run once from Armada's device-quirks service, after local-fs.target. Never stop
or restart a running/crashed DSP, change its firmware, or synthesize battery data.
"""
from pathlib import Path, PurePosixPath


def restore(remoteprocs=Path('/sys/class/remoteproc'), firmware=Path('/usr/lib/firmware')):
    for device in sorted(remoteprocs.glob('remoteproc*')):
        if (device / 'name').read_text().strip() != 'adsp':
            continue
        if (device / 'device/driver').resolve().name != 'qcom_q6v5_pas':
            continue
        state = device / 'state'
        if state.read_text().strip() != 'offline':
            continue
        name = PurePosixPath((device / 'firmware').read_text().strip())
        if name.is_absolute() or '..' in name.parts or not name.parts or name.parts[0] != 'qcom':
            raise RuntimeError('Refusing an unexpected ADSP firmware path')
        if not (firmware / name).is_file():
            raise RuntimeError('ADSP firmware is still unavailable on the root filesystem')
        # Recheck before writing; do not interrupt a processor started elsewhere.
        if state.read_text().strip() == 'offline':
            try:
                state.write_text('start\n')
            except OSError:
                if state.read_text().strip() != 'running':
                    raise
            print('Requested offline Qualcomm ADSP startup', flush=True)


if __name__ == '__main__':
    restore()
