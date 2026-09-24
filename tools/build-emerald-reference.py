#!/usr/bin/env python3
"""Generate factual Emerald ID/name/stat tables from a pinned pret source checkout.
No game implementation, prose, artwork or ROM bytes are included in the output.
The optional cache uses flattened source filenames (see --source-dir).
"""
import argparse, hashlib, json, re
from pathlib import Path
REVISION = '5eff78649e7170a877b961ef0b3da13b81a16038'
FILES = ['include/constants/species.h','include/constants/moves.h','include/constants/items.h',
         'include/constants/abilities.h','include/constants/pokemon.h',
         'src/data/pokemon/species_info.h','src/data/battle_moves.h','charmap.txt']
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--source-dir',type=Path,required=True);args=ap.parse_args()
    raw={}
    for f in FILES:
        p=args.source_dir/f
        if not p.exists():p=args.source_dir/f.replace('/','_')
        raw[f]=p.read_bytes()
    texts={k:v.decode('utf-8') for k,v in raw.items()}
    def constants(file,prefix):
        return {name:int(value) for name,value in re.findall(r'^#define '+prefix+r'([A-Z0-9_]+)\s+(\d+)\b',texts[file],re.M)}
    def pretty(name):return name.replace('_',' ').title()
    species=constants(FILES[0],'SPECIES_');moves=constants(FILES[1],'MOVE_')
    # The pinned item IDs are an implicit, zero-based enum, not #defines.
    item_enum=re.search(r'enum\s*\{(.*?)\};',texts[FILES[2]],re.S)[1]
    item_enum=re.sub(r'//[^\n]*','',item_enum)
    item_names=re.findall(r'^\s*ITEM_([A-Z0-9_]+)\s*,',item_enum,re.M)
    items={name:index for index,name in enumerate(item_names)}
    assert len(items)==377 and items['CHARCOAL']==215
    natures=constants(FILES[4],'NATURE_')
    dex=json.loads(Path('data/pokedex.json').read_text(encoding='utf-8'))['entries']
    norm=lambda s:re.sub('[^a-z0-9]','',s.lower())
    byname={norm(e['id']):e for e in dex}
    records={}
    for name,body in re.findall(r'\[SPECIES_([A-Z0-9_]+)\]\s*=\s*\{\s*\n(.*?)\n\s*\},?',texts[FILES[5]],re.S):
        if name not in species or species[name] not in list(range(1,252))+list(range(277,412)):continue
        entry=byname[norm(name)]
        fields=['baseHP','baseAttack','baseDefense','baseSpeed','baseSpAttack','baseSpDefense']
        bases=[int(re.search(r'\.'+f+r'\s*=\s*(\d+)',body)[1]) for f in fields]
        types=re.findall(r'TYPE_([A-Z]+)',re.search(r'\.types\s*=\s*\{([^}]+)',body)[1])
        ab=re.findall(r'ABILITY_([A-Z0-9_]+)',re.search(r'\.abilities\s*=\s*\{([^}]+)',body)[1])
        growth=re.search(r'\.growthRate\s*=\s*GROWTH_([A-Z_]+)',body)[1]
        records[str(species[name])]={'id':entry['id'],'number':entry['number'],'name':entry['name'],
            'base':bases,'types':list(dict.fromkeys(map(pretty,types))),'abilities':[pretty(a) if a!='NONE' else '' for a in ab],
            'growth':growth,'form':entry['forms'][0]['id']}
    assert len(records)==386, len(records)
    move_table={}
    for name,body in re.findall(r'\[MOVE_([A-Z0-9_]+)\]\s*=\s*\{\s*\n(.*?)\n\s*\},?',texts[FILES[6]],re.S):
        if name not in moves:continue
        move_table[str(moves[name])]={'name':pretty(name),'pp':int(re.search(r'\.pp\s*=\s*(\d+)',body)[1])}
    assert len(move_table)==355
    chars={int(code,16):char for char,code in re.findall(r"^'(.)'\s*=\s*([0-9A-F]{2})\s*$",texts['charmap.txt'].split('@ Hiragana')[0],re.M)}
    result={'sourceRevision':REVISION,'species':records,'moves':move_table,
        'items':{str(v):pretty(k) for k,v in items.items() if not re.fullmatch(r'[0-9A-F]{3}',k) and not k.startswith('UNUSED')},
        'natures':{str(v):pretty(k) for k,v in natures.items()},'characters':chars}
    Path('data/emerald-reference.json').write_text(json.dumps(result,ensure_ascii=False,separators=(',',':'))+'\n',encoding='utf-8')
    Path('data/emerald-reference-source.json').write_text(json.dumps({'source':'https://github.com/pret/pokeemerald','revision':REVISION,
        'inputs':{k:hashlib.sha256(v).hexdigest() for k,v in raw.items()},'scope':'Factual IDs, names, numeric rules and text encoding only; no game code or assets.'},indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
