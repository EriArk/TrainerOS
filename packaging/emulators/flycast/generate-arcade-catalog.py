#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Export dependency metadata from the pinned native table, never ROM/EEPROM bytes.

This is a bounded initializer reader, not a C++ evaluator or an arcade decoder.
An upstream change requires a reviewed pin/hash and a new generated catalog.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

SOURCE = 'd0e47e572b1e7b355e88bda8308c89d0c5156cbf'
TABLE_SHA = '824c8e244c060fca037dd16ad99ef7adc5f0a788de3f0e92853a2dae25f2cac9'


def array(text, name):
    text = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                  lambda m: m[0] if m[0].startswith('"') else '', text, flags=re.S)
    start = re.search(r'\b' + name + r'\[\]\s*=\s*\{', text)
    if not start:raise ValueError('Missing native array')
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|[{},]|[^\s{},]+', text[start.end()-1:])
    index = 0
    def group():
        nonlocal index
        assert tokens[index] == '{';index += 1
        result = [];field = []
        while tokens[index] != '}':
            if tokens[index] == '{':field.append(group())
            elif tokens[index] == ',':
                if field:result.append(field[0] if len(field) == 1 else ' '.join(field))
                field = [];index += 1
            else:field.append(tokens[index]);index += 1
        if field:result.append(field[0] if len(field) == 1 else ' '.join(field))
        index += 1
        return result
    return group()


def string(value):
    if value in ('NULL', 'nullptr', '0'):return None
    result = json.loads(value)
    if not isinstance(result, str):raise ValueError('Expected native string')
    return result


def number(value):
    # The reviewed table contains integer literals, not arbitrary expressions.
    if not re.fullmatch(r'(?:0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*', value):
        raise ValueError('Unexpected native integer: ' + value)
    return int(value.rstrip('uUlL'), 0) if value != '0' else 0


def blob(row, bios=False):
    shift = 1 if bios else 0
    name = string(row[shift])
    if not name:return None
    kind = row[shift+4] if len(row)>shift+4 else 'Normal'
    if kind not in ('Normal','SwapWordBytes','InterleavedWord','Copy','Key','Eeprom'):
        raise ValueError('Unknown native blob type')
    return {'name':name,'length':number(row[shift+2]),
            'crc':number(row[shift+3]) if len(row)>shift+3 else 0,
            'kind':kind, **({'region':number(row[0])} if bios else {})}


def generate(source):
    raw=source.read_bytes().replace(b'\r\n',b'\n')
    if hashlib.sha256(raw).hexdigest()!=TABLE_SHA:raise ValueError('Unreviewed native table')
    text=raw.decode();games={};bios={}
    for row in array(text,'BIOS'):
        name=string(row[0])
        if not name:continue
        bios[name]=[v for entry in row[1] if (v:=blob(entry,True))]
    for row in array(text,'Games'):
        name=string(row[0])
        if not name:continue
        firmware=string(row[5]) or 'naomi'
        platform='atomiswave' if row[6]=='AW' else 'naomi2' if firmware=='naomi2' else 'naomi'
        if name in games or firmware not in bios:raise ValueError('Invalid native catalog reference')
        games[name]={'platform':platform,'parent':string(row[1]),'bios':firmware,
                     'blobs':[v for entry in row[8] if (v:=blob(entry))],
                     'disc':string(row[9]) if len(row)>9 else None}
    if len(games)<200 or set(v['platform'] for v in games.values())!={'naomi','naomi2','atomiswave'}:
        raise ValueError('Incomplete native table export')
    return {'protocol':1,'source':SOURCE,'tableSha256':TABLE_SHA,
            'notice':'Native ROM metadata: Flycast GPL-2.0-or-later; MAME ROM information LGPL-2.1-or-later. See arcade-catalog.NOTICE.',
            'games':games,'bios':bios}


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('native_table',type=Path);p.add_argument('output',type=Path)
    args=p.parse_args();catalog=generate(args.native_table)
    args.output.write_text(json.dumps(catalog,sort_keys=True,separators=(',',':'))+'\n',encoding='utf-8')
    print('Exported',len(catalog['games']),'native games and',len(catalog['bios']),'BIOS definitions')
