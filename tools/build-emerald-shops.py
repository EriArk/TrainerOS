#!/usr/bin/env python3
"""Extract factual merchant data from the pinned, private Emerald source cache."""
from pathlib import Path
import argparse, hashlib, json, re
ap=argparse.ArgumentParser();ap.add_argument('--source-dir',type=Path,required=True);root=ap.parse_args().source_dir
inputs={}
def source(path):
    p=root/path.replace('/','_');inputs[p.name]=hashlib.sha256(p.read_bytes()).hexdigest();return p.read_text(encoding='utf-8')
const=source('include/constants/items.h')
enum=re.sub(r'//[^\n]*','',re.search(r'enum\s*\{(.*?)\};',const,re.S)[1])
ids={name:i for i,name in enumerate(re.findall(r'^\s*(ITEM_[A-Z0-9_]+)\s*,',enum,re.M))}
tms=source('include/constants/tms_hms.h')
for kind,count in [('TM',50),('HM',8)]:
    moves=re.findall(r'F\((\w+)\)',re.search(r'#define FOREACH_'+kind+r'\(F\)(.*?)(?=\n\n)',tms,re.S)[1]);assert len(moves)==count
    for n,move in enumerate(moves,1):ids['ITEM_'+kind+'_'+move]=ids[f'ITEM_{kind}{n:02}']
items={}
for key,body in re.findall(r'\[(ITEM_\w+)\]\s*=\s*\{(.*?)\n    \}',source('src/data/items.h'),re.S):
    if key not in ids:continue
    pocket=re.search(r'\.pocket = (POCKET_\w+)',body);name=re.search(r'\.name = _\("([^"]+)"\)',body);price=re.search(r'\.price = (\d+)',body)
    if not pocket or not name:continue
    title=name[1].title()
    if key.startswith(('ITEM_TM_','ITEM_HM_')):title=name[1]+' · '+key[8:].replace('_',' ').title()
    items[str(ids[key])]={'name':title,'price':int(price[1]) if price else 0,'pocket':{'POCKET_ITEMS':1,'POCKET_KEY_ITEMS':2,'POCKET_POKE_BALLS':3,'POCKET_TM_HM':4,'POCKET_BERRIES':5}[pocket[1]]}
flagtext=source('include/constants/flags.h')
flags={k:int(v,0) for k,v in re.findall(r'#define (FLAG_\w+)\s+(0x[0-9a-fA-F]+|\d+)\b',flagtext)}
for k,v in re.findall(r'#define (FLAG_\w+)\s+\(SYSTEM_FLAGS \+ (0x[0-9A-Fa-f]+)\)',flagtext):flags[k]=0x860+int(v,0)
decorids={k:int(v) for k,v in re.findall(r'#define (DECOR_\w+)\s+(\d+)\b',source('include/constants/decorations.h'))}
decor={};cats=['DESK','CHAIR','PLANT','ORNAMENT','MAT','POSTER','DOLL','CUSHION']
for key,body in re.findall(r'\[(DECOR_\w+)\]\s*=\s*\{(.*?)\n    \}',source('src/data/decoration/header.h'),re.S):
    decor[str(decorids[key])]={'name':re.search(r'\.name = _\("([^"]+)"\)',body)[1].title(),'price':int(re.search(r'\.price = (\d+)',body)[1]),'category':cats.index(re.search(r'\.category = DECORCAT_(\w+)',body)[1])}
def lists(place):
    s=source('data/maps/'+place+'/scripts.inc')
    return s,[(name,[ids[x] if x.startswith('ITEM_') else decorids[x] for x in re.findall(r'(?:ITEM|DECOR)_\w+',body)],'decoration' if 'DECOR_' in body else 'item') for name,body in re.findall(r'\n(\w+):\n((?:\s*\.2byte (?:ITEM|DECOR)_\w+\n)+)',s)]
shops=[]
def merchant(id,name,location,flag,stock,kind='item',**extra):
    m={'id':id,'name':name,'location':location,'visitedFlag':flags[flag],'stock':stock,'kind':kind,**extra};shops.append(m);return m
for city in ['OldaleTown','PetalburgCity','RustboroCity','SlateportCity','MauvilleCity','VerdanturfTown','FallarborTown','LavaridgeTown','FortreeCity','MossdeepCity','SootopolisCity']:
    s,ls=lists(city+'_Mart');name=re.sub(r'(Town|City)$',r' \1',city);flag='FLAG_VISITED_'+re.sub(r'(Town|City)$',r'_\1',city).upper()
    m=merchant(city.lower()+'-mart',name+' Poké Mart',name,flag,ls[0][1])
    if len(ls)>1:m.update(expandedFlag=flags[re.search(r'goto_if_set (FLAG_\w+)',s)[1]],expandedStock=ls[1][1])
group='Lilycove Department Store'
for floor,names in [(2,['Travel supplies','Medicine']),(3,['Vitamins','Battle items']),(4,['Attack TMs','Defense TMs']),(5,['Dolls','Cushions','Posters','Mats'])]:
    s,ls=lists('LilycoveCity_DepartmentStore_'+str(floor)+'F');assert len(ls)==len(names)
    for n,((_,stock,kind),name) in enumerate(zip(ls,names)):
        merchant(f'lilycove-{floor}f-{n}',f'{floor}F · {name}','Lilycove City','FLAG_VISITED_LILYCOVE_CITY',stock,kind,group=group)
s,ls=lists('LilycoveCity_DepartmentStoreRooftop')
merchant('lilycove-rooftop-drinks','Rooftop · Drinks','Lilycove City','FLAG_VISITED_LILYCOVE_CITY',[ids['ITEM_FRESH_WATER'],ids['ITEM_SODA_POP'],ids['ITEM_LEMONADE']],group=group,roof=True,vending=True,limit=1)
merchant('lilycove-rooftop-sale','Rooftop · Clear-out sale','Lilycove City','FLAG_VISITED_LILYCOVE_CITY',ls[0][1],'decoration',group=group,roof=True,news=3)
for place,flag,name in [('LavaridgeTown_HerbShop','FLAG_VISITED_LAVARIDGE_TOWN','Herb Shop'),('FortreeCity_DecorationShop','FLAG_VISITED_FORTREE_CITY','Fortree Decoration Shop'),('Route104_PrettyPetalFlowerShop','FLAG_LANDMARK_FLOWER_SHOP','Pretty Petal Flower Shop'),('BattleFrontier_Mart','FLAG_LANDMARK_BATTLE_FRONTIER','Battle Frontier Poké Mart'),('EverGrandeCity_PokemonLeague_1F','FLAG_LANDMARK_POKEMON_LEAGUE','Pokémon League Poké Mart'),('TrainerHill_Entrance','FLAG_LANDMARK_TRAINER_HILL','Trainer Hill Poké Mart')]:
    s,ls=lists(place)
    for n,(_,stock,kind) in enumerate(ls[:1] if place=='TrainerHill_Entrance' else ls):
        m=merchant(place.lower()+'-'+str(n),['Desks','Chairs'][n] if place=='FortreeCity_DecorationShop' else name, name,flag,stock,kind)
        if place=='FortreeCity_DecorationShop':m['group']=name
        if place=='Route104_PrettyPetalFlowerShop':m['requiredFlags']=[flags['FLAG_MET_PRETTY_PETAL_SHOP_OWNER'],flags['FLAG_BADGE03_GET']]
        if place=='TrainerHill_Entrance':m.update(expandedFlag=flags['FLAG_SYS_GAME_CLEAR'],expandedStock=ls[1][1])
s,ls=lists('SlateportCity')
for n,((_,stock,kind),name) in enumerate(zip(ls,['Energy Guru','Dolls','Decorations','Power TMs'])):
    m=merchant('slateport-market-'+str(n),name,'Slateport City','FLAG_VISITED_SLATEPORT_CITY',stock,kind,group='Slateport Market')
    if n==0:m['discountNews']=1
    if n==2:m['requiredFlags']=[flags['FLAG_RECEIVED_SECRET_POWER']]
    if n==3:m['hiddenFlag']=flags['FLAG_HIDE_SLATEPORT_CITY_TM_SALESMAN']
source('data/maps/MtChimney/scripts.inc');source('data/maps/Route109_SeashoreHouse/scripts.inc')
merchant('mt-chimney-cookies','Lava Cookie stall','Mt. Chimney','FLAG_DEFEATED_EVIL_TEAM_MT_CHIMNEY',[ids['ITEM_LAVA_COOKIE']],hiddenFlag=flags['FLAG_HIDE_MT_CHIMNEY_LAVA_COOKIE_LADY'],limit=1)
merchant('seashore-drinks','Seashore House','Route 109','FLAG_LANDMARK_SEASHORE_HOUSE',[ids['ITEM_SODA_POP']],requiredFlags=[flags['FLAG_RECEIVED_6_SODA_POP']],limit=1)
for p in ['include/global.h','include/global.tv.h','include/constants/tv.h','include/constants/vars.h','src/tv.c','src/item.c','src/shop.c','src/decoration_inventory.c']:source(p)
result={'version':2,'source':'pret/pokeemerald','revision':'5eff78649e7170a877b961ef0b3da13b81a16038','items':items,'decorations':decor,'merchants':shops}
assert len(shops)==36 and len(decor)==121 and ids['ITEM_POKE_BALL']==4
for m in shops:
    for id in m['stock']+m.get('expandedStock',[]):assert str(id) in (decor if m['kind']=='decoration' else items)
Path('data/emerald-shops.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
Path('data/emerald-shops-source.json').write_text(json.dumps({'source':'https://github.com/pret/pokeemerald','revision':result['revision'],'inputs':dict(sorted(inputs.items())),'scope':'Factual names, identifiers, prices, pockets, stock and flag conditions only. No dialogue, game implementation or art.'},indent=2)+'\n',encoding='utf-8',newline='\n')
print(len(shops),'counters;',len(items),'item facts;',len(decor),'decoration facts')
