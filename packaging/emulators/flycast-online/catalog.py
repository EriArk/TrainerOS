#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build serial aliases from a supplied libretro Redump DAT (never ROM names).

DCNet's published service families plus the additions in Flycast 2.5-2.7.
This catalog describes native Online candidates, not live service certification.
"""
import hashlib
import json
from pathlib import Path
import re
import sys

SOURCE_SHA256 = '010a5b393fc6f0aee22fc9a1f6849bb7b7fa9a08ab75ac4602a810a1c3d23fa3'

FAMILIES = (
    '4x4 Evolution', '4x4 Evo', 'ChuChu Rocket!', 'Dee Dee Planet', 'Internet Game Pack',
    'Monaco Grand Prix Online', 'Monaco Grand Prix Online - Racing Simulation 2',
    'Racing Simulation 2 - Monaco Grand Prix On-Line',
    'NBA 2K1', 'NBA 2K2', 'NCAA College Football 2K2 - Road to the Rose Bowl',
    'NFL 2K1', 'NFL 2K2', 'Ooga Booga', 'PBA Tour Bowling 2001', 'Planet Ring',
    'POD - Speedzone', 'POD 2 - Multiplayer Online', 'Starlancer', 'StarLancer',
    'Toy Racer', 'World Series Baseball 2K2', 'Worms World Party', 'Daytona USA',
    'Daytona USA 2001', 'Sega Tetris', 'Golf Shiyou yo 2 - Aratanaru Chousen',
    'Aero Dancing i', 'Aero Dancing i - Jikai Saku made Matemasen', 'Hundred Swords',
    'Speed Devils Online Racing', 'Speed Devils - Online Racing', 'F355 Challenge', 'F355 Challenge - Passione Rossa',
    'Phantasy Star Online', 'Phantasy Star Online Ver. 2', 'Quake III Arena', 'Quake III - Arena',
    'Kidou Senshi Gundam - Renpou vs. Zeon & DX', 'Outtrigger', 'Power Smash - Sega Professional Tennis',
    'Rune Jade', 'Next Tetris, The - On-Line Edition',
    'Dennou Senki - Virtual-On - Oratorio Tangram',
)


def serial(value):
    value = re.sub(r'[-\s]', '', value.upper())
    # Redump packaging serials omit the MK prefix used in Sega's boot sector.
    return value[2:] if re.fullmatch(r'MK\d+', value) else value


def generate(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError('Source DAT changed; review and update its pinned revision and digest together')
    entries = {}
    for block in data.decode('utf-8-sig').split('\ngame (')[1:]:
        name = re.search(r'^\s*name "([^"]+)"', block).group(1)
        title = name.split(' (', 1)[0]
        if title not in FAMILIES or re.search(r'\((Demo|Sample|Beta|Promo)', name):
            continue
        match = re.search(r'^\s*serial "([^"]+)"', block, re.M)
        if not match:
            continue
        key = serial(match.group(1))
        if not re.fullmatch(r'[A-Z0-9]{4,16}', key):
            continue
        value = {'title': title, 'bba': title == 'Toy Racer'}
        if key in entries and entries[key] != value:
            raise ValueError('Ambiguous product code: ' + key)
        entries[key] = value
    if len(entries) < 30:
        raise ValueError('Incomplete source catalog')
    return {'protocol': 1, 'sourceSha256': hashlib.sha256(data).hexdigest(),
            'source': 'https://github.com/libretro/libretro-database/blob/9a899f5b13ed1c355491e342203be29bc573323b/metadat/redump/Sega%20-%20Dreamcast.dat',
            'services': 'https://github.com/TheArcadeStriker/flycast-wiki/wiki/DCNet',
            'entries': dict(sorted(entries.items()))}


if __name__ == '__main__':
    Path(sys.argv[2]).write_text(json.dumps(generate(Path(sys.argv[1]).read_bytes()), indent=2) + '\n', encoding='utf-8')
