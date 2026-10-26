#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare ARMSX2 -gamecfg; no global configuration or memory-card writes.

The caller must select the exact game's existing INI (or None if absent), own
the private session directory and retain it until the emulator exits. This is
not game admission, an emulator launcher or a replacement for Lobby lifecycle.
"""
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat


def group_connection(data: bytes) -> dict:
    def unique(pairs):
        result={}
        for k,v in pairs:
            if k in result:raise ValueError('Duplicate LAN field')
            result[k]=v
        return result
    value=json.loads(data,object_pairs_hook=unique)
    if set(value)!={'protocol','mode','room','seat','players','sockets'} or value['protocol']!=2 or value['mode']!='ps2':
        raise ValueError('Invalid LAN group')
    seat,count=value['seat'],value['players']
    if type(seat) is not int or type(count) is not int or not 2<=count<=4 or not 1<=seat<=count:
        raise ValueError('Invalid LAN seats')
    if not isinstance(value['room'],str) or not re.fullmatch(r'[a-f0-9]{12}',value['room']):raise ValueError('Invalid LAN room')
    sockets=value['sockets']
    expected={str(n) for n in range(2,count+1)} if seat==1 else {'1'}
    if not isinstance(sockets,dict) or set(sockets)!=expected or len(set(sockets.values()))!=len(sockets):raise ValueError('Invalid LAN edges')
    for path in sockets.values():
        if not isinstance(path,str):raise ValueError('Invalid LAN socket')
        p=PurePosixPath(path)
        if not p.is_absolute() or p.as_posix()!=path or '..' in p.parts or p.name!='ps2.sock' or len(path.encode())>100 or '\0' in path:
            raise ValueError('Invalid LAN socket')
    return value


def connection(data: bytes) -> dict[str, str]:
    """Read only the private configuration emitted by stream_transport ps2."""
    if len(data) > 4096:
        raise ValueError('Invalid PS2 connection size')
    if data.lstrip().startswith(b'{'):
        group=group_connection(data)
        return {'mode':'ps2','address':'127.0.0.1','host':'true' if group['seat']==1 else 'false',
                'port':'19072' if group['seat']==1 else '19073','peer':str(group['seat']),'room':group['room'],
                'sockets':group['sockets']}
    values = {}
    for line in data.decode('ascii').splitlines():
        key, separator, value = line.partition('=')
        if not separator or key in values:
            raise ValueError('Invalid PS2 connection fields')
        values[key] = value
    if set(values) != {'mode', 'address', 'host', 'port', 'peer', 'room', 'socket'}:
        raise ValueError('Invalid PS2 connection fields')
    if values['mode'] != 'ps2' or values['address'] != '127.0.0.1':
        raise ValueError('Invalid PS2 connection mode')
    if values['host'] not in ('true', 'false') or values['peer'] != ('1' if values['host'] == 'true' else '2'):
        raise ValueError('Invalid PS2 connection role')
    if not re.fullmatch(r'[0-9]{4,5}', values['port']) or not 1024 <= int(values['port']) <= 65535:
        raise ValueError('Invalid PS2 connection port')
    if values['host'] == 'true' and values['port'] != '19072':
        raise ValueError('Invalid PS2 host port')
    if not re.fullmatch(r'[a-f0-9]{12}', values['room']):
        raise ValueError('Invalid PS2 room material')
    path=PurePosixPath(values['socket'])
    if not path.is_absolute() or path.name!='ps2.sock' or '..' in path.parts:
        raise ValueError('Invalid private PS2 socket')
    if values['host']=='false' and values['port']!='19073':
        raise ValueError('Invalid PS2 guest port')
    return values


def overlay(original: bytes, material: bytes) -> bytes:
    values = connection(material)
    if len(original) > 1024 * 1024 or b'\0' in original:
        raise ValueError('Invalid PS2 game settings')
    text = original.decode('utf-8-sig')
    settings = {
        'EthEnable': 'true', 'EthApi': 'Local Link',
        'LocalLinkHost': values['host'], 'LocalLinkAddress': values['address'],
        'LocalLinkPort': values['port'], 'LocalLinkPeerId': values['peer'],
        'LocalLinkRoomCode': values['room'],
    }
    # Do not parse/re-serialize unrelated Qt INI values: escapes, paths, controller
    # bindings and per-title overrides must retain their original representation.
    result = []
    in_network = False
    seen_network = False
    for line in text.splitlines(keepends=True):
        stripped = line.strip()
        if stripped.startswith('[') and stripped.endswith(']'):
            if in_network:
                result.extend(f'{key} = {value}\n' for key, value in settings.items())
            in_network = stripped == '[DEV9/Eth]'
            if in_network and seen_network:
                raise ValueError('Duplicate PS2 Ethernet section')
            seen_network |= in_network
        key, equals, _ = stripped.partition('=')
        if in_network and equals and key.strip().casefold() in {k.casefold() for k in settings}:
            continue
        # Separate a final unterminated original line from appended settings.
        result.append(line if line.endswith(('\n', '\r')) else line + '\n')
    if not seen_network:
        result.append('\n[DEV9/Eth]\n')
    if in_network or not seen_network:
        result.extend(f'{key} = {value}\n' for key, value in settings.items())
    return ''.join(result).encode('utf-8')


def launch_settings(original: bytes) -> bytes:
    """Disable library enumeration only in the private launch settings copy.

    Even ARMSX2 -batch scans configured library folders; an unavailable network
    mount can stall before the selected local game boots. Retain every other key.
    """
    if len(original)>1024*1024 or b'\0' in original:raise ValueError('Invalid PS2 base settings')
    result=[];game_list=False
    for line in original.decode('utf-8-sig').splitlines(keepends=True):
        stripped=line.strip()
        if stripped.startswith('[') and stripped.endswith(']'):game_list=stripped=='[GameList]'
        key,equals,_=stripped.partition('=')
        if game_list and equals and key.strip() in ('Paths','RecursivePaths'):continue
        result.append(line)
    return ''.join(result).encode('utf-8')


def prepare(source: Path | None, destination: Path, material: bytes) -> None:
    """Exclusive private output; never overwrite an existing or personal INI."""
    parent = destination.parent
    if not destination.is_absolute() or not parent.is_dir() or parent.is_symlink():
        raise ValueError('A private PS2 session directory is required')
    if os.name != 'nt':
        info = parent.stat()
        if info.st_uid != os.getuid() or stat.S_IMODE(info.st_mode) & 0o077:
            raise ValueError('PS2 session directory is not private')
    original = b''
    if source is not None:
        if not source.is_absolute() or not source.is_file() or source.stat().st_size > 1024 * 1024:
            raise ValueError('Invalid PS2 game settings file')
        with source.open('rb') as stream:
            original = stream.read(1024 * 1024 + 1)
    payload = overlay(original, material)
    descriptor = os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(descriptor, 'wb') as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
    except BaseException:
        destination.unlink(missing_ok=True)
        raise
