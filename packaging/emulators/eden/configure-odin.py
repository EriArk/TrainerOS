#!/usr/bin/env python3
"""Configure the reviewed Eden 0.2.1 ARM64 AppImage on Armada Odin/Turnip.

Explicit maintenance step, not a setting for arbitrary PCs. No game/save/NAND
files are changed. Fresh controller mappings are seeded by TrainerOS discovery.
"""
import hashlib
import os
from pathlib import Path
import re
import shutil
import tempfile


def save(path, data):
    if path.is_symlink():
        raise SystemExit('Review symlinked configuration before changing it')
    if path.exists():
        original = path.read_bytes()
        if original == data: return
        backup = path.with_name(path.name + '.before-traineros-' + hashlib.sha256(original).hexdigest()[:12])
        if not backup.exists(): shutil.copy2(path, backup)
        assert backup.read_bytes() == original
    fd, temporary = tempfile.mkstemp(dir=path.parent, prefix='.traineros-')
    try:
        with os.fdopen(fd, 'wb') as output:
            output.write(data); output.flush(); os.fsync(output.fileno())
        os.chmod(temporary, 0o600)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary): os.unlink(temporary)
    print('Configured', path.name)


def main():
    home = Path.home()
    package = home/'Applications/eden.AppImage'
    expected = 'b64f926cbf74fd870a39b144971084323d895d51919b91e906328e7f81bea087'
    if hashlib.sha256(package.read_bytes()).hexdigest() != expected:
        raise SystemExit('Runtime changed: review the new package before applying this preset')
    if not Path('/usr/share/vulkan/icd.d/freedreno_icd.aarch64.json').is_file():
        raise SystemExit('This preset requires the reviewed Armada Turnip driver')
    for process in Path('/proc').glob('[0-9]*'):
        try:
            if (process/'exe').readlink().name in ('eden', 'eden-cli', 'eden.AppImage'):
                raise SystemExit('Close Eden before configuring it')
        except (FileNotFoundError, PermissionError): pass
    config = home/'.config/eden/qt-config.ini'
    text = config.read_text(encoding='utf-8')
    for key, value in (('cpu_backend', '0'), ('confirmStop', '2')):
        for name, setting in ((key, value), (key+'\\default', 'false')):
            text, count = re.subn(r'(?m)^'+re.escape(name)+r'=[^\n]*$', lambda _: name+'='+setting, text)
            if count != 1: raise SystemExit('Unrecognized configuration: '+name)
    save(config, text.encode('utf-8'))
    env = Path(__file__).with_name('odin.env').read_bytes()
    save(package.with_name(package.name+'.env'), env)


if __name__ == '__main__': main()
