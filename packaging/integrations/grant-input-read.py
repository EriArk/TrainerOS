#!/usr/bin/python3
"""Root installation hook: read-only access to the built-in Flip pad only.

Run after InputPlumber has hidden its source. No writes, grabs or general input
group membership are granted. Install this file and its parent root-owned.
"""
import fcntl
import os
from pathlib import Path
import pwd
import stat
import subprocess
import sys
import time


def grant(account):
    uid = pwd.getpwnam(account).pw_uid
    if os.geteuid() != 0 or uid < 1000:
        raise RuntimeError('Root and a regular target account are required')
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        for item in Path('/sys/class/input').glob('event*'):
            try:
                if (item/'device/name').read_text().strip() != 'Retroid Pocket Gamepad': continue
                path = Path('/dev/input')/item.name
                fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC | os.O_NOFOLLOW)
                with os.fdopen(fd, 'rb', buffering=0) as source:
                    info = os.fstat(source.fileno())
                    expected = (item/'dev').read_text().strip()
                    if not stat.S_ISCHR(info.st_mode) or info.st_uid != 0:
                        raise RuntimeError('Unexpected input node')
                    if expected != f'{os.major(info.st_rdev)}:{os.minor(info.st_rdev)}':
                        raise RuntimeError('Input identity changed')
                    # InputPlumber hides the source with mode 000. Wait for it,
                    # so a later hide does not mask the newly granted ACL.
                    if info.st_mode & 0o777: continue
                    name = bytearray(128)
                    fcntl.ioctl(source.fileno(), 0x80804506, name, True)
                    if bytes(name).split(b'\0')[0] != b'Retroid Pocket Gamepad':
                        raise RuntimeError('Unexpected physical controller')
                    subprocess.run(['/usr/bin/setfacl', '-m', f'u:{uid}:r--', '--',
                                    f'/proc/self/fd/{source.fileno()}'],
                                   pass_fds=(source.fileno(),), check=True, timeout=2)
                    return True
            except FileNotFoundError:
                continue
        time.sleep(.1)
    return False  # An absent device must not prevent InputPlumber/desktop boot.


if __name__ == '__main__':
    if len(sys.argv) != 2: raise SystemExit(2)
    print('TrainerOS pad read access ready' if grant(sys.argv[1]) else 'TrainerOS pad not available')
