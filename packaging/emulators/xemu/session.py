#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare local xemu settings. No peer paths, account or save transfer.

The native HDD stays owned by xemu (including its exclusive QEMU image lock).
Only a private EEPROM copy receives a session MAC; encrypted HDD/online keys,
serial, region and the original EEPROM are never replaced.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import tomllib


def atom(v):
    if isinstance(v, str):
        return json.dumps(v, ensure_ascii=False)
    if type(v) is bool:
        return 'true' if v else 'false'
    if type(v) in (int, float):
        return repr(v)
    if isinstance(v, list):
        return '[' + ', '.join(map(atom, v)) + ']'
    if isinstance(v, dict):
        return '{' + ', '.join(atom(k) + '=' + atom(x) for k, x in v.items()) + '}'
    raise ValueError('Unsupported native setting type')


def toml(value, prefix=()):
    lines = ['[' + '.'.join(map(atom, prefix)) + ']'] if prefix else []
    lines += [atom(k) + ' = ' + atom(v) for k, v in value.items() if not isinstance(v, dict)]
    for k, v in value.items():
        if isinstance(v, dict):
            lines += ['', toml(v, prefix + (k,))]
    return '\n'.join(lines) + '\n'


def checksum(data):
    # Native factory-section checksum: end-around carry of little-endian words.
    total = sum(struct.unpack('<11I', data))
    return (~((total & 0xffffffff) + (total >> 32))) & 0xffffffff


def private_eeprom(data, mac):
    if len(data) != 256 or len(mac) != 6 or mac[0] & 1:
        raise ValueError('Invalid own Xbox EEPROM or network address')
    if struct.unpack_from('<I', data, 0x30)[0] != checksum(data[0x34:0x60]):
        raise ValueError('Own Xbox EEPROM checksum is invalid')
    result = bytearray(data)
    result[0x40:0x46] = mac
    struct.pack_into('<I', result, 0x30, checksum(result[0x34:0x60]))
    return bytes(result)


def prepare(config, directory, mac):
    config, directory = Path(config), Path(directory)
    if not config.is_file() or config.stat().st_size > 1024 * 1024:
        raise ValueError('Open Xbox normally once to prepare your native settings')
    value = tomllib.loads(config.read_text(encoding='utf-8'))
    files = value.get('sys', {}).get('files', {})
    for key in ('bootrom_path', 'flashrom_path', 'hdd_path', 'eeprom_path'):
        p = Path(files.get(key, ''))
        if not p.is_absolute() or not p.is_file():
            raise ValueError('Open Xbox normally once to prepare your own firmware, HDD and EEPROM')
    eeprom = Path(files['eeprom_path'])
    if eeprom.stat().st_size != 256:
        raise ValueError('Invalid own Xbox EEPROM')
    data = private_eeprom(eeprom.read_bytes(), bytes.fromhex(mac.replace(':', '')))
    if not directory.is_dir() or directory.is_symlink():
        raise ValueError('Invalid private session folder')
    value.setdefault('net', {})['enable'] = False  # CLI attaches the native nvnet NIC instead.
    files['eeprom_path'] = str(directory / 'eeprom.bin')
    # Keep graphics, controls, firmware and personal HDD exactly as configured.
    for name, content in [('eeprom.bin', data), ('xemu.toml', toml(value).encode())]:
        with (directory / name).open('xb') as f:
            os.chmod(f.name, 0o600)
            f.write(content)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config')
    parser.add_argument('directory')
    parser.add_argument('mac')
    a = parser.parse_args()
    try:
        prepare(a.config, a.directory, a.mac)
    except (ValueError, OSError, KeyError) as e:
        print(str(e))
        raise SystemExit(1)
