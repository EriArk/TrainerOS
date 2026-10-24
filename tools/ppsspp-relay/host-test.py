#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Linux relay lifecycle check. Requires an unused port 27313 and iproute2 ss."""
from pathlib import Path
import signal
import socket
import subprocess
import sys
import time
import os


def main():
    binary = Path(sys.argv[1]).resolve(strict=True)
    # The intermediate parent lets the test observe PR_SET_PDEATHSIG cleanup.
    parent = subprocess.Popen([
        sys.executable, '-c',
        'import subprocess,sys,time; '
        'p=subprocess.Popen([sys.argv[1]],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); '
        'print(p.pid,flush=True); time.sleep(15)', str(binary),
    ], stdout=subprocess.PIPE, text=True)
    child = None
    try:
        child = int(parent.stdout.readline())
        matches = []
        for _ in range(30):
            lines = subprocess.check_output(['ss', '-lntp'], text=True).splitlines()
            matches = [line for line in lines if f'pid={child},' in line]
            if matches:
                break
            time.sleep(.1)
        assert matches and all('127.0.0.1:27313' in line for line in matches), matches
        with socket.create_connection(('127.0.0.1', 27313), timeout=2):
            pass
        parent.kill()
        parent.wait(timeout=3)
        for _ in range(30):
            try:
                alive = Path('/proc', str(child), 'stat').read_text().split(') ', 1)[1][0] != 'Z'
            except FileNotFoundError:
                alive = False
            if not alive:
                break
            time.sleep(.1)
        assert not alive, 'Relay survived parent death'
        print('LOOPBACK_AND_PARENT_DEATH_PASS')
    finally:
        if parent.poll() is None:
            parent.kill()
            parent.wait(timeout=3)
        if child is not None:
            try:
                # Only signal the child if its executable still matches.
                if Path('/proc', str(child), 'exe').readlink() == binary:
                    os.kill(child, signal.SIGTERM)
            except FileNotFoundError:
                pass


if __name__ == '__main__':
    main()
