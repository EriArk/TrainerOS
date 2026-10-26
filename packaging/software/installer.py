#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Armada desktop installer/recovery entry; downloads from the pinned bootstrap."""
import argparse
import hashlib
import importlib.util
import time
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import shlex
import installation as install
import releases as sw


def atomic_file(path, data, mode):
    path = Path(path); install.private(path.parent)
    if path.is_symlink(): raise ValueError('Refusing a redirected launcher')
    fd, temporary = tempfile.mkstemp(dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data); stream.flush(); os.fsync(stream.fileno())
        os.chmod(temporary, mode); os.replace(temporary, path)
    finally:
        if os.path.exists(temporary): os.unlink(temporary)


def shortcut_contents(root, home=None):
    root, home = Path(root), Path(home or Path.home())
    helper = root / 'bootstrap/packaging/software'
    def quoted(value): return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"').replace('%', '%%') + '"'
    launch = ('#!/bin/sh\nexec /usr/bin/python3 ' + shlex.quote(str(helper / 'launcher.py')) +
              ' --root ' + shlex.quote(str(root)) + ' "$@"\n').encode()
    def desktop(name, entry, extra=''):
        return ('[Desktop Entry]\nType=Application\nName=' + name + '\nExec=/usr/bin/python3 ' + quoted(helper / entry) +
                ' --root ' + quoted(root) + extra + '\nIcon=applications-games\nTerminal=false\nCategories=Game;\n').encode()
    return {home / '.local/bin/traineros': (launch, 0o755),
            home / '.local/share/applications/org.traineros.TrainerOS.desktop': (desktop('TrainerOS', 'launcher.py'), 0o644),
            home / '.local/share/applications/org.traineros.Recovery.desktop': (desktop('TrainerOS Setup and Recovery', 'installer.py', ' --gui'), 0o644)}


def shortcuts(root, home=None, preflight=False):
    previous = {r['path']: r['sha256'] for r in install.read(Path(root) / 'shortcuts.json', [])}
    contents = shortcut_contents(root, home)
    for path in contents:
        if path.is_symlink() or path.exists() and (str(path) not in previous or sw.sha256(path) != previous[str(path)]):
            raise ValueError('Existing unmanaged launcher requires explicit migration: ' + str(path))
    if preflight: return
    records = [{'path': str(p), 'sha256': hashlib.sha256(data).hexdigest()} for p, (data, _) in contents.items()]
    # Write ownership before creating files: interruption remains reinstallable.
    sw.atomic(Path(root) / 'shortcuts.json', records)
    for path, (data, mode) in contents.items(): atomic_file(path, data, mode)


def bootstrap(root, source):
    root, source = Path(root), Path(source)
    destination = root / 'bootstrap'
    wanted = [*source.joinpath('software').glob('*.py'), *source.joinpath('session').glob('*'),
              *source.joinpath('integrations').glob('*.py'), source / 'crates/catalogue.py']
    files = [p for p in wanted if p.is_file() and not p.is_symlink()]
    records = {p.relative_to(source).as_posix(): sw.sha256(p) for p in files}
    if destination.exists():
        # Bootstrap updates need their own reviewed installer; OTA cannot silently
        # replace the trust/launcher code while it is executing.
        old = install.read(root / 'bootstrap-files.json')
        if old != records: raise ValueError('Installed bootstrap differs; use the matching recovery installer')
        for name, digest in records.items():
            path = destination / 'packaging' / name
            if path.is_symlink() or not path.is_file() or sw.sha256(path) != digest: raise ValueError('Bootstrap was modified')
        return
    with tempfile.TemporaryDirectory(dir=root, prefix='.bootstrap-') as tmp:
        stage = Path(tmp) / 'tree'; stage.mkdir()
        for path in files:
            target = stage / 'packaging' / path.relative_to(source)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target); target.chmod(0o755 if path.suffix in ('.py', '.sh') else 0o644)
        sw.atomic(root / 'bootstrap-files.json', records)
        os.replace(stage, destination)


def integrate(root, remove=False):
    import pwd
    root = Path(root)
    # pkexec authenticates this one reviewed installation operation. No new broad
    # permanent root command or package-manager permission is granted.
    helper = root / 'bootstrap/packaging/session/install.py'
    for name, digest in install.read(root / 'bootstrap-files.json').items():
        path = root / 'bootstrap/packaging' / name
        if path.is_symlink() or sw.sha256(path) != digest: raise ValueError('Bootstrap integrity check failed')
    subprocess.run(['pkexec', '/usr/bin/python3', '-I', str(helper), '--user', pwd.getpwuid(os.getuid()).pw_name,
                    '--uninstall' if remove else '--managed'], check=True)
    sw.atomic(root / 'integration.json', {'systemInstalled': not remove})


def dialog(*args):
    return subprocess.run(['kdialog', '--title', 'TrainerOS Setup', *args], capture_output=True, text=True)


class Cancelled(RuntimeError):
    pass


class Progress:
    def __enter__(self):
        self.dbus = shutil.which('qdbus6') or shutil.which('qdbus-qt6') or shutil.which('qdbus')
        if not self.dbus: raise ValueError('The Armada KDE qdbus utility is required for progress and cancellation')
        result = dialog('--progressbar', 'Preparing TrainerOS...', '100')
        self.ref = result.stdout.strip().split(); self.last = 0; self.phase = ''
        if result.returncode or len(self.ref) != 2: raise ValueError('Could not open installation progress')
        self.call('setAutoClose', 'false'); self.call('showCancelButton', 'true')
        return self
    def call(self, *args):
        return subprocess.run([self.dbus, *self.ref, *args], capture_output=True, text=True, timeout=5)
    def update(self, phase, done=0, total=0):
        sw.event(phase, done, total)
        # Avoid a DBus process for each download buffer. Losing the progress
        # window is treated like Cancel, including KDE versions that destroy it.
        now = time.monotonic()
        if phase == self.phase and now - self.last < .25: return
        self.last = now; self.phase = phase
        state = self.call('wasCancelled')
        if state.returncode or state.stdout.strip() == 'true': raise Cancelled('Download cancelled. Reopen Setup to resume.')
        self.call('setLabelText', {'download': 'Downloading TrainerOS', 'extract': 'Preparing verified files',
                                 'verify-archive': 'Verifying download', 'prepared': 'Download verified'}.get(phase, phase))
        self.call('Set', '', 'maximum', '100' if total else '0')
        self.call('Set', '', 'value', str(int(done * 100 / total) if total else 0))
    def __exit__(self, *unused):
        try: self.call('close')
        except (OSError, subprocess.SubprocessError): pass


def prerequisites(source):
    for binary in ('/usr/bin/python3', '/usr/bin/pkexec', '/usr/sbin/visudo', '/usr/bin/ldd'):
        if not os.access(binary, os.X_OK): raise ValueError('Missing Armada installer dependency: ' + binary)
    spec = importlib.util.spec_from_file_location('emulator_support', Path(source) / 'integrations/emulator_support.py')
    support = importlib.util.module_from_spec(spec); spec.loader.exec_module(support)
    support.prerequisites()


def payload_dependencies(prepared):
    payload = Path(prepared) / 'payload'
    env = dict(os.environ)
    if (payload / 'lib').is_dir(): env['LD_LIBRARY_PATH'] = str(payload / 'lib')
    result = subprocess.run(['/usr/bin/ldd', str(payload / 'bin/traineros')], env=env, capture_output=True, text=True, timeout=30)
    if result.returncode or 'not found' in result.stdout:
        raise ValueError('The signed TrainerOS build needs unavailable system libraries. Use a matching Armada build; installation was not activated.')


def gui(root, config_path):
    if not shutil.which('kdialog'): raise ValueError('Armada KDE kdialog is required to run the graphical installer')
    report = install.host.probe(root)
    if not report['supported']: raise ValueError('\n'.join(report['errors']))
    source = Path(__file__).resolve().parents[1]
    if not (root / 'config.json').exists():
        config = sw.decode(Path(config_path).read_bytes())
        if set(config) != {'schema', 'feeds', 'publicKey'} or config['schema'] != 1: raise ValueError('Invalid publisher bootstrap')
        key = Path(config_path).parent / sw.relative(config['publicKey'])
        # The normal Qt application data path; never the Software versions root.
        data = Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share'))) / 'TrainerOS/TrainerOS'
        install.initialize(root, config['feeds'], key, data)
    bootstrap(root, source)
    current = install.receipt(root)
    action = 'install'
    if current['active']:
        choice = dialog('--menu', 'TrainerOS installation and recovery', 'audit', 'Check installation', 'repair', 'Repair installed files',
                        'rollback', 'Return to the previous compatible version', 'cache', 'Clear downloaded update files', 'uninstall', 'Uninstall TrainerOS')
        if choice.returncode: return 0
        action = choice.stdout.strip()
    disk = report['osStorage']
    if action == 'audit':
        result = install.audit(root)
        dialog('--msgbox', 'Software files: ' + result['status'] + '\nSession integration: ' + result.get('sessionIntegration', 'not-recorded') + '\n' + '\n'.join(result['problems'])); return 0
    if action != 'install':
        if dialog('--yesno', 'Continue with ' + action + '? Personal profiles, games and saves will be kept.').returncode: return 0
        with sw.cache_lock(root / 'runtime'), sw.cache_lock(root / 'transaction'):
            install.ensure_no_orphans(root)
            if action == 'uninstall':
                install.uninstall(root, preflight=True)
                if install.read(root / 'integration.json', {}).get('systemInstalled'): integrate(root, remove=True)
                result = install.uninstall(root)
            elif action == 'cache': result = install.clean_cache(root)
            elif action == 'rollback': result = install.rollback(root)
            else:
                prerequisites(source)
                with Progress() as progress:
                    progress.update('Repairing installed files')
                    result = install.repair(root, progress=progress.update, before_install=lambda: progress.call('showCancelButton', 'false')); shortcuts(root); integrate(root)
        dialog('--msgbox', result['status']); return 0
    prerequisites(source)
    shortcuts(root, preflight=True)
    choice = dialog('--radiolist', 'Choose an update channel', 'beta', 'Beta', 'on', 'preview', 'Preview — experimental', 'off')
    if choice.returncode: return 0
    channel = choice.stdout.strip()
    with Progress() as progress:
        progress.update('Checking update channel')
        candidate = install.resolve(root, channel)
    if candidate['status'] != 'available': raise ValueError('No compatible release is available for this installation')
    description = (f"Install TrainerOS {candidate['version']}?\nDownload: {candidate['downloadBytes'] / 1048576:.1f} MiB\n"
                   f"OS: {report['system']}\nOS storage: {disk['location']} ({disk['disk']})\n"
                   f"Free installation space: {report['freeBytes'] / 1073741824:.1f} GiB\n"
                   'Your OS, games, profiles and saves will be kept. A system authorization prompt installs the session choice.')
    if dialog('--yesno', description).returncode: return 0
    with Progress() as progress:
        result = install.resolve(root, channel, prepare=True, expected=candidate['sequence'], progress=progress.update)
        if result['status'] != 'prepared': raise ValueError('Release changed. Check again before installation')
        with sw.cache_lock(root / 'runtime'), sw.cache_lock(root / 'transaction'):
            install.ensure_no_orphans(root)
            progress.update('Checking application dependencies')
            payload_dependencies(result['prepared'])
            progress.update('Installing verified files')
            progress.call('showCancelButton', 'false')
            install.install_prepared(root, result['prepared']); shortcuts(root)
            # Journal intent before pkexec; a partial privileged install must be
            # removed by its owner before uninstalling the application.
            sw.atomic(root / 'integration.json', {'systemInstalled': True})
            integrate(root)
    if not dialog('--yesno', 'TrainerOS is installed. Open first-time setup now?').returncode:
        subprocess.Popen(['/usr/bin/python3', str(root / 'bootstrap/packaging/software/launcher.py'), '--root', str(root)], start_new_session=True)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=install.DEFAULT_ROOT)
    parser.add_argument('--config', type=Path, default=Path(__file__).resolve().parents[2] / 'bootstrap.json')
    parser.add_argument('--gui', action='store_true')
    args = parser.parse_args()
    try:
        if sys.platform != 'linux': raise ValueError('The installer requires Armada Linux')
        if os.getuid() == 0: raise ValueError('Run Setup as your desktop user; only session integration requests system authorization')
        if not args.gui: parser.error('Use --gui from the installer launcher')
        return gui(args.root, args.config)
    except Cancelled as error:
        dialog('--msgbox', str(error)); return 0
    except (OSError, ValueError, RuntimeError, KeyError, ImportError, subprocess.SubprocessError) as error:
        if shutil.which('kdialog'): dialog('--error', str(error) + '\nYou can reopen Setup to retry or repair. Personal data is preserved.')
        else: print(str(error), file=sys.stderr)
        return 1


if __name__ == '__main__': raise SystemExit(main())
