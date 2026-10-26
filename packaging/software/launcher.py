#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Runtime lease survives the GUI and adopted games; no automatic rollback."""
import argparse
import ctypes
import logging
from logging.handlers import RotatingFileHandler
import os
from pathlib import Path
import select
import subprocess
import sys
import time
import installation as install
import releases as sw


def run(root, arguments=()):
    if sys.platform != 'linux': raise RuntimeError('The managed launcher requires Linux')
    root = install.private(root)
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(36, 1, 0, 0, 0) != 0:  # PR_SET_CHILD_SUBREAPER
        raise OSError(ctypes.get_errno(), 'Cannot retain games after shell exit')
    upstream = os.environ.get('TRAINEROS_READY_FD')
    forwarded = False
    with sw.cache_lock(root / 'runtime'):
        install.ensure_no_orphans(root)
        while True:
            current = install.receipt(root)
            release = install.version(root, current['active'])
            config = install.configuration(root)
            install.host.compatible(release, install.host.schemas(config['dataDirectory']))
            payload = root / 'versions' / current['active'] / 'payload'
            reader, writer = os.pipe()
            env = dict(os.environ, TRAINEROS_SOFTWARE_ROOT=str(root), TRAINEROS_READY_FD=str(writer),
                       TRAINEROS_SOFTWARE_MANAGED='1')
            # Optional bundled libraries are part of the signed, verified tree.
            if (payload / 'lib').is_dir(): env['LD_LIBRARY_PATH'] = str(payload / 'lib')
            if (payload / 'plugins').is_dir(): env['QT_PLUGIN_PATH'] = str(payload / 'plugins')
            sw.atomic(root / 'runtime-session.json', {'boot': install.boot_id(), 'pid': os.getpid()})
            try:
                child = subprocess.Popen([str(payload / 'bin/traineros'), '--data-dir', config['dataDirectory'], *arguments],
                                         cwd=payload, env=env, pass_fds=(writer,))
            except (OSError, ValueError):
                os.close(reader)
                (root / 'runtime-session.json').unlink(missing_ok=True)
                raise
            finally: os.close(writer)
            try:
                readable, _, _ = select.select([reader], [], [], 45)
                ready = bool(readable) and os.read(reader, 1) == b'R'
                with sw.cache_lock(root / 'transaction'):
                    current['health'] = 'first-frame-confirmed' if ready else 'first-frame-unconfirmed'
                    sw.atomic(root / 'receipt.json', current)
                if ready and upstream and not forwarded:
                    try: os.write(int(upstream), b'R'); os.close(int(upstream))
                    except OSError: pass
                    forwarded = True
            finally: os.close(reader)
            result = child.wait()
            # Closing/crashing the GUI does not imply its game has exited. No
            # signals are sent to descendants; the lease remains held until all end.
            while True:
                try: os.waitpid(-1, 0)
                except InterruptedError: continue
                except ChildProcessError: break
            if result not in (0, 75):
                (root / 'request.json').unlink(missing_ok=True)
                install.record_event(root, 'launch', phase='failed', exit=result)
                # A crash may leave a service-launched game outside our process
                # tree. Retain the boot fence until recovery after a new boot.
                return result
            with sw.cache_lock(root / 'transaction'):
                if result == 0:
                    (root / 'request.json').unlink(missing_ok=True)
                if result == 0 or not install.read(root / 'request.json'):
                    (root / 'runtime-session.json').unlink(missing_ok=True)
                    return result
                try: install.apply_request(root)
                except (OSError, ValueError, RuntimeError, KeyError, subprocess.SubprocessError) as error:
                    install.record_event(root, 'maintenance', phase='failed', error=str(error))
                    logging.error('Maintenance failed; retaining owned active receipt: %s', error)
            # Explicit maintenance requested a shell restart. Re-read the atomic
            # receipt whether activation succeeded or failed; never reboot the OS.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', default=str(install.DEFAULT_ROOT))
    args, extra = parser.parse_known_args()
    root = install.private(args.root)
    handler = RotatingFileHandler(root / 'launcher.log', maxBytes=262144, backupCount=2)
    logging.basicConfig(level=logging.INFO, handlers=[handler])
    try: return run(root, extra)
    except (OSError, ValueError, RuntimeError, KeyError, subprocess.SubprocessError) as error:
        logging.error('%s', error); print(str(error), file=sys.stderr); return 70


if __name__ == '__main__': raise SystemExit(main())
