#!/usr/bin/env python3
from pathlib import Path
import re,json,argparse,hashlib
ap=argparse.ArgumentParser(description='Extract factual Emerald shop inventories, prices, IDs and unlock flags from a pinned flattened source cache.');ap.add_argument('--source-dir',type=Path,required=True);r=ap.parse_args().source_dir
const=(r/'include_constants_items.h').read_text(encoding='utf-8'); enum=re.sub(r'//[^\n]*','',re.search(r'enum\s*\{(.*?)\};',const,re.S)[1]); ids={name:i for i,name in enumerate(re.findall(r'^\s*(ITEM_[A-Z0-9_]+)\s*,',enum,re.M))}
items={}
for key,body in re.findall(r'\[(ITEM_\w+)\]\s*=\s*\{(.*?)\n    \}',(r/'src_data_items.h').read_text(encoding='utf-8'),re.S):
 if key not in ids:continue
 pocket=re.search(r'\.pocket = (POCKET_\w+)',body);name=re.search(r'\.name = _\("([^"]+)"\)',body);price=re.search(r'\.price = (\d+)',body)
 if not pocket or not name:continue
 items[str(ids[key])]={'name':name[1].title().replace('Poké','Poké'),'price':int(price[1]) if price else 0,'pocket':{'POCKET_ITEMS':1,'POCKET_KEY_ITEMS':2,'POCKET_POKE_BALLS':3,'POCKET_TM_HM':4,'POCKET_BERRIES':5}[pocket[1]]}
flags={k:int(v,0) for k,v in re.findall(r'#define (FLAG_\w+)\s+(0x[0-9a-fA-F]+|\d+)\b',(r/'include_constants_flags.h').read_text(encoding='utf-8'))}
for k,v in re.findall(r'#define (FLAG_\w+)\s+\(SYSTEM_FLAGS \+ (0x[0-9A-Fa-f]+)\)',(r/'include_constants_flags.h').read_text(encoding='utf-8')):flags[k]=0x860+int(v,0)
order=['OldaleTown','PetalburgCity','RustboroCity','SlateportCity','MauvilleCity','VerdanturfTown','FallarborTown','LavaridgeTown','FortreeCity','MossdeepCity','SootopolisCity']
shops=[]
for city in order:
 s=(r/('data_maps_'+city+'_Mart_scripts.inc')).read_text(encoding='utf-8');lists=re.findall(r'\n(\w+):\n((?:\s*\.2byte ITEM_\w+\n)+)\s*pokemartlistend',s)
 def stock(body):return [ids[x] for x in re.findall(r'ITEM_\w+',body)]
 name=re.sub(r'(Town|City)$',r' \1',city); flagname=re.sub(r'(Town|City)$',r'_\1',city).upper()
 shop={'id':city.lower()+'-mart','name':name+' Poké Mart','location':name,'visitedFlag':flags['FLAG_VISITED_'+flagname],'stock':stock(lists[0][1])}
 if len(lists)>1:
  flag=re.search(r'goto_if_set (FLAG_\w+)',s)[1];shop.update(expandedFlag=flags[flag],expandedStock=stock(lists[1][1]))
 shops.append(shop)
result={'version':1,'source':'pret/pokeemerald','revision':'5eff78649e7170a877b961ef0b3da13b81a16038','items':items,'merchants':shops}
Path('data/emerald-shops.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
assert len(shops)==11 and ids['ITEM_POKE_BALL']==4
inputs={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(r.iterdir()) if p.name in ['include_constants_items.h','include_constants_flags.h','src_data_items.h'] or p.name.startswith('data_maps_') and p.name.endswith('_Mart_scripts.inc') and 'BattleFrontier' not in p.name}
Path('data/emerald-shops-source.json').write_text(json.dumps({'source':'https://github.com/pret/pokeemerald','revision':result['revision'],'inputs':inputs,'scope':'Factual names, identifiers, prices, pockets, stock and flag conditions only. No dialogue, game implementation or art.'},indent=2)+'\n',encoding='utf-8')
print(len(shops),'marts extracted')
