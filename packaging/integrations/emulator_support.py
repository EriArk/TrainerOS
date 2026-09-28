"""Session-installed Adventure helpers; no emulator preference or save writes."""
import ctypes
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

HELPERS = ('overlay_support.py', 'adventure-overlay.py', 'controller-bridge.py', 'grant-input-read.py')
LEGACY_BRIDGE = 'e6dc58795a67b789aabe546b5d20952c10a971c071a496b2c56b81f9c587c210'


def prerequisites():
    # Check everything before the installer enables a session or writes helpers.
    import dbus  # noqa: F401
    for library in ('libSDL2-2.0.so.0', 'libX11.so.6', 'libXRes.so.1', 'libXtst.so.6'):
        ctypes.CDLL(library)
    for binary in ('/usr/bin/python3', '/usr/bin/ffmpeg', '/usr/bin/setfacl', '/usr/bin/systemctl'):
        if not os.access(binary, os.X_OK):
            raise RuntimeError('Missing Adventure prerequisite: ' + binary)
    subprocess.run(['/usr/bin/ffmpeg', '-version'], check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.PIPE, timeout=10)
    subprocess.run(['/usr/bin/systemctl', 'is-active', '--quiet', 'inputplumber.service'],
                   check=True, timeout=10)


def migrate_stock_bridge(account, data, previous=None):
    """Update only our exact old helper; preserve custom bridges and a rollback copy."""
    path = Path(account.pw_dir) / '.local/libexec/traineros/controller-bridge.py'
    if path.is_symlink() or not path.is_file():
        return False
    known = {LEGACY_BRIDGE}
    if previous is not None:
        known.add(hashlib.sha256(previous).hexdigest())
    if path.stat().st_uid != account.pw_uid or hashlib.sha256(path.read_bytes()).hexdigest() not in known:
        return False
    backup = path.with_name('controller-bridge.before-guarded-home.py')
    # Existing backup is never overwritten. Do not follow an unexpected symlink.
    try:
        fd = os.open(backup, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        if backup.is_symlink() or hashlib.sha256(backup.read_bytes()).hexdigest() not in known:
            raise RuntimeError('Unexpected controller helper backup')
    else:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(path.read_bytes()); stream.flush()
            os.fchown(stream.fileno(), account.pw_uid, account.pw_gid); os.fsync(stream.fileno())
    fd, temporary = tempfile.mkstemp(dir=path.parent, prefix='.traineros-bridge-')
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data); stream.flush(); os.fchmod(stream.fileno(), 0o755)
            os.fchown(stream.fileno(), account.pw_uid, account.pw_gid); os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary): os.unlink(temporary)
    return True


def install_support(account, install):
    prerequisites()
    source = Path(__file__).resolve().parent
    contents = {name: (source / name).read_bytes().replace(b'\r\n', b'\n') for name in HELPERS}
    for name, data in contents.items():
        compile(data, name, 'exec')
    bridge = Path('/var/opt/traineros/integrations/controller-bridge.py')
    previous = bridge.read_bytes() if bridge.is_file() else None
    for name, data in contents.items():
        install('/var/opt/traineros/integrations/' + name, data, 0o755)
    install('/etc/systemd/system/inputplumber.service.d/90-traineros-pad-read.conf',
            ('[Service]\nExecStartPost=/usr/bin/python3 /var/opt/traineros/integrations/grant-input-read.py '
             + account.pw_name + '\n').encode(), 0o644)
    # Old integration JSON can keep its exact path while future helper updates
    # use the canonical installation. Never redirect a customized user bridge.
    trampoline = b"#!/usr/bin/python3\nimport runpy\nrunpy.run_path('/var/opt/traineros/integrations/controller-bridge.py', run_name='__main__')\n"
    migrate_stock_bridge(account, trampoline, previous)
    subprocess.run(['/usr/bin/systemctl', 'daemon-reload'], check=True, timeout=15)
    # Apply read-only ACL now without restarting InputPlumber or the session.
    subprocess.run(['/usr/bin/python3', '/var/opt/traineros/integrations/grant-input-read.py', account.pw_name],
                   check=True, timeout=15)
