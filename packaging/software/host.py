#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only host/data discovery. Never infer the OS disk from the ROM library."""
import json
from contextlib import closing
import os
from pathlib import Path
import platform
import shutil
import sqlite3
import subprocess


def command(args):
    return subprocess.check_output(args, text=True, timeout=15)


def schemas(directory):
    root = Path(directory)
    database = root / 'traineros.sqlite3'
    version = 0
    if database.exists():
        if database.is_symlink() or not database.is_file():
            raise ValueError('Invalid personal database location')
        # mode=ro sees committed WAL state, unlike immutable=1; never migrates.
        with closing(sqlite3.connect(database.resolve().as_uri() + '?mode=ro', uri=True, timeout=2)) as db:
            version = db.execute('PRAGMA user_version').fetchone()[0]
    first = root / 'first-run.json'
    setup = 0
    if first.exists():
        if first.is_symlink() or first.stat().st_size > 32768:
            raise ValueError('Invalid first-run state')
        setup = json.loads(first.read_text(encoding='utf-8'))['version']
    if any(type(v) is not int or not 0 <= v <= 10000 for v in (version, setup)):
        raise ValueError('Unknown data schema')
    return {'sqlite': version, 'firstRun': setup}


def storage(mount, blocks):
    filesystems = mount.get('filesystems', [])
    if len(filesystems) != 1:
        raise ValueError('Cannot identify the OS filesystem')
    filesystem = filesystems[0]
    source = filesystem.get('source', '').split('[')[0]
    if not source.startswith('/dev/'):
        raise ValueError('OS storage is not a supported block device')
    disks = []
    def visit(node):
        if node.get('type') == 'disk':
            disks.append(node)
        for child in node.get('children', []): visit(child)
    for node in blocks.get('blockdevices', []): visit(node)
    if len(disks) != 1:
        raise ValueError('OS storage has ambiguous physical backing')
    disk = disks[0]
    if disk.get('rm') not in (True, False, 0, 1, '0', '1') or not str(disk.get('path', '')).startswith('/dev/'):
        raise ValueError('OS storage location is unknown')
    removable = disk.get('rm') in (True, 1, '1')
    return {'source': source, 'filesystem': filesystem.get('fstype', ''),
            'disk': disk.get('path', ''), 'location': 'removable' if removable else 'internal'}


def probe(destination, *, run=command, system=None, machine=None, filesystem=Path('/')):
    """Injectable reads are for host fixtures, never exposed as CLI overrides."""
    system, machine = system or platform.system(), machine or platform.machine()
    errors = []
    if system != 'Linux' or machine not in ('aarch64', 'arm64'):
        errors.append('This installer requires Linux ARM64 Armada.')
    release = {}
    os_release = filesystem / 'etc/os-release'
    if os_release.is_file():
        for line in os_release.read_text().splitlines():
            if '=' in line:
                key, value = line.split('=', 1); release[key] = value.strip('"')
    for name in ('usr/libexec/armada/device-env', 'usr/libexec/armada/session-control', 'usr/bin/gamescope'):
        if not os.access(filesystem / name, os.X_OK): errors.append('Missing Armada capability: ' + name)
    disk = {}
    if system == 'Linux':
        try:
            mount = json.loads(run(['findmnt', '--json', '--target', '/usr', '--output', 'SOURCE,FSTYPE']))
            source = mount['filesystems'][0]['source'].split('[')[0]
            disk = storage(mount, json.loads(run(['lsblk', '--json', '--inverse', '--paths', '--output', 'PATH,TYPE,RM', source])))
            # Some MMC drivers mark SD as non-removable; the card type is the
            # physical distinction between SD and soldered eMMC in that case.
            card = filesystem / 'sys/class/block' / Path(disk['disk']).name / 'device/type'
            if card.is_file():
                kind = card.read_text().strip()
                if kind == 'SD': disk['location'] = 'removable'
                elif kind == 'MMC': disk['location'] = 'internal'
        except (OSError, ValueError, KeyError, IndexError, subprocess.SubprocessError) as e:
            errors.append('Could not identify OS storage: ' + str(e))
    path = Path(destination).absolute()
    while not path.exists(): path = path.parent
    return {'supported': not errors, 'system': release.get('PRETTY_NAME', system),
            'architecture': machine, 'osStorage': disk, 'freeBytes': shutil.disk_usage(path).free,
            'errors': errors, 'target': 'linux-aarch64' if not errors else ''}


def compatible(release, data):
    for name, version in data.items():
        limits = release['data'][name]
        if not limits['min'] <= version <= limits['max']:
            raise ValueError('Release is incompatible with current ' + name + ' data; personal files are unchanged')
