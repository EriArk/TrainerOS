#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build LAN availability from the owner's CSV and pinned upstream disc serials.

Development-only dependency: PyYAML. Never matches library filenames or titles
at runtime. The CSV defines candidate availability, not gameplay certification.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import yaml

REVISION='90126bdfea'
DATABASE_SHA256='61c77b789ccde856096f024e4a3eef6891e15ca59c127e0dd13e762a7d338a7e'
SOURCE=f'https://github.com/ARMSX2/ARMSX2/blob/{REVISION}/bin/resources/GameIndex.yaml'
# Explicit alternate catalogue names, reviewed against that revision. No fuzzy
# matching: remixes, sequels, demos and similarly named games remain distinct.
ALIASES={
    3:['Armored Core - Nexus [Disc 1]','Armored Core - Nexus [Disc 2]'],
    10:['Conflict - Global Terror','Conflict - Global Storm'],
    36:['SOCOM - US Navy Seals - Combined Assault'],
    44:['TOCA Race Driver 2 - Ultimate Racing Simulator',
        'DTM Race Driver 2 - The Ultimate Racing Simulator','V8 Supercars 2'],
    45:['TOCA Race Driver 3 - The Ultimate Racing Simulator',
        'DTM Race Driver 3 - The Ultimate Racing Simulator','V8 Supercars Australia 3'],
    56:['SEGA AGES 2500シリーズ Vol.31 電脳戦機バーチャロン'],
}
REGIONS={9:'PAL',20:'PAL',42:'NTSC-U',56:'NTSC-J'}
REISSUES=re.compile(r' \[(?:Greatest Hits|PlayStation ?2 the Best|PlayStation 2 BigHit Series|BEST PRICE)\]$',re.I)


def normalize(name):
    # Keep non-Latin letters: stripping them would mistake Japanese demo names
    # for retail releases. Only the known budget reissue suffixes are removed.
    return ''.join(c for c in REISSUES.sub('',name).casefold() if c.isalnum())


def build(csv_path,database_path):
    payload=database_path.read_bytes()
    if hashlib.sha256(payload).hexdigest()!=DATABASE_SHA256:
        raise ValueError('Review a changed upstream game database before rebuilding')
    database=yaml.safe_load(payload)
    with csv_path.open(encoding='utf-8-sig',newline='') as stream:
        rows=list(csv.reader(stream))[1:]
    if [int(row[0]) for row in rows]!=list(range(1,57)):
        raise ValueError('Review a changed owner LAN list before rebuilding')
    games=[];used=set()
    for row in rows:
        number=int(row[0]);names={normalize(n) for n in [row[1],*ALIASES.get(number,[])]}
        releases=[]
        for serial,game in sorted(database.items()):
            if not re.fullmatch(r'(?:SC|SL)[A-Z]{2}-\d{5}',serial):continue
            if serial.startswith(('SCED-','SLED-')):continue
            if normalize(game.get('name','')) not in names:continue
            region=game.get('region','')
            if number in REGIONS and not region.startswith(REGIONS[number]):continue
            if serial in used:raise ValueError('Ambiguous LAN catalogue serial')
            used.add(serial)
            releases.append({'serial':serial,'region':region})
        if not releases:raise ValueError(f'No reviewed serials for LAN row {number}')
        games.append({'catalogueRow':number,'title':row[1],'mode':'lan',
                      'availability':'owner-test','releases':releases})
    return {'version':2,'source':SOURCE,'sourceSha256':DATABASE_SHA256,
            'catalogue':'docs/data/ps2/ps2_lan_full.csv',
            'catalogueSha256':hashlib.sha256(csv_path.read_bytes()).hexdigest(),
            'games':games}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database',type=Path,required=True)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args();root=Path(__file__).resolve().parents[2]
    result=build(root/'docs/data/ps2/ps2_lan_full.csv',args.database)
    destination=Path(__file__).with_name('profiles.json')
    if args.check:
        if json.loads(destination.read_text(encoding='utf-8'))!=result:raise SystemExit('Stale LAN catalogue')
    else:destination.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('PS2 LAN catalogue:',len(result['games']),'games,',sum(len(g['releases']) for g in result['games']),'serials')
