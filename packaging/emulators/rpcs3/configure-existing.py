#!/usr/bin/env python3
"""Apply the reviewed Armada launch defaults to an existing RPCS3 installation.

Run as the handheld user while RPCS3 is closed. Discovery seeds fresh installs;
this explicit maintenance step preserves existing firmware, pads and other settings.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import tempfile


def replace_ini(text, key, value):
    match = re.search(r'(?m)^\[main_window\]\s*$', text)
    if not match:
        return text.rstrip() + '\n\n[main_window]\n' + key + '=' + value + '\n'
    end = re.search(r'(?m)^\[', text[match.end():])
    stop = match.end() + end.start() if end else len(text)
    section = text[match.end():stop]
    pattern = r'(?m)^' + re.escape(key) + r'=[^\n]*$'
    section = (re.sub(pattern, key + '=' + value, section) if re.search(pattern, section)
               else section.rstrip() + '\n' + key + '=' + value + '\n')
    return text[:match.end()] + section + text[stop:]


def save(path, text):
    original = path.read_bytes()
    data = text.encode('utf-8')
    if original == data:
        return
    backup = path.with_name(path.name + '.before-traineros-' + hashlib.sha256(original).hexdigest()[:12])
    if not backup.exists():
        shutil.copy2(path, backup)
    assert backup.read_bytes() == original, 'Backup mismatch'
    fd, name = tempfile.mkstemp(dir=path.parent, prefix='.traineros-')
    try:
        with os.fdopen(fd, 'wb') as output:
            output.write(data); output.flush(); os.fsync(output.fileno())
        os.chmod(name, path.stat().st_mode & 0o777)
        os.replace(name, path)
    finally:
        if os.path.exists(name): os.unlink(name)
    print('Updated', path.name, 'backup:', backup.name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config-dir', type=Path, default=Path.home()/'.config/rpcs3')
    args = parser.parse_args()
    for process in Path('/proc').glob('[0-9]*'):
        try:
            if (process/'exe').readlink().name in ('rpcs3', 'rpcs3.AppImage'):
                raise SystemExit('Close RPCS3 before configuring it')
        except (FileNotFoundError, PermissionError):
            pass
    config = args.config_dir/'config.yml'
    gui = args.config_dir/'GuiConfigs/CurrentSettings.ini'
    if config.is_symlink() or gui.is_symlink():
        raise SystemExit('Symlinked preferences require an explicit operator review')
    text = config.read_text(encoding='utf-8')
    # Change only the known interpreter preset that stalled on Odin.
    text = text.replace('  Shader Mode: Async Recompiler with Shader Interpreter\n',
                        '  Shader Mode: Async Recompiler (multi-threaded)\n')
    save(config, text)
    text = gui.read_text(encoding='utf-8')
    for key in ('infoBoxEnabledWelcome', 'confirmationBoxExitGame'):
        text = replace_ini(text, key, 'false')
    save(gui, text)


if __name__ == '__main__':
    main()
