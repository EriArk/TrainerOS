"""Four Trainer outfits for art review; never mutates the live avatar pack.

Uses the retained skeleton/correctives and a separate hem-free fitting guide.
Python 3.12 + numpy/scipy. Run from any directory.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path

import numpy as np
from scipy.interpolate import PchipInterpolator
from scipy.spatial import cKDTree

import build_golf_clothes as base
from build_pack import GLB, Builder
from check_garment_fit import depth_profile

ROOT = Path(__file__).resolve().parents[2]
PALETTE = {
    'ivory': 'f4f0df', 'forest': '467e7b', 'peach': 'edb595',
    'sole': 'e0d9cb', 'dark': '293c4c', 'metal': 'd7b975',
    'dress': 'eab2a1', 'dress-light': 'f7d9c6', 'denim': '527899',
    'denim-light': '88a3b8', 'stitch': 'ddc69b',
    'race': '3d6d7e', 'orange': 'f1a36d',
    'rose': 'd891ab', 'plum': '70546f', 'visor': '8bbfc6',
}
SETS = {
    'starter-sundress': ['sundress'],
    'starter-denim': ['everyday-tee', 'denim-breeches', 'canvas-shoes'],
    'racing-classic': ['racing-suit', 'racing-helmet', 'racing-boots', 'racing-gloves'],
    'racing-rose': ['rose-racing-suit', 'rose-racing-helmet', 'rose-racing-boots', 'rose-racing-gloves'],
}


class Collection(base.Author):
    def surface(self,*args,**kwargs):
        # Relax positions, skin weights and corrective donors together. Keeping
        # old donors after moving vertices produces spikes at raised shoulders.
        kwargs['coherent']=True
        return super().surface(*args,**kwargs)

    def binding(self,v,rig,ids=None,factor=None):
        j,w,ids,factor=super().binding(v,'body' if rig=='torso' else rig,ids,factor)
        if rig.startswith('boot.'):
            suffix=rig.split('.')[1]
            shin=np.clip((v[:,1]-.050)/.075,0,1);shin=shin*shin*(3-2*shin)
            j[:]=0;w[:]=0
            j[:,0]=self.names.index('foot.'+suffix);j[:,1]=self.names.index('shin.'+suffix)
            w[:,0]=1-shin;w[:,1]=shin
        if rig=='torso':
            # Sleeveless fabric follows the trunk; nearby arm donors must not
            # pull strips of the bodice upward when the character raises arms.
            w*=np.isin(j,[1,2,3])
            empty=w.sum(1)<1e-8;j[empty,0]=np.where(v[empty,1]<.53,1,3);w[empty,0]=1
            w/=w.sum(1)[:,None]
        return j,w,ids,factor

    def front_depth(self,item,xy):
        fabric=self.parts[item][0]
        return np.array([depth_profile(fabric['v'],fabric['f'],x,[y])[0,1]
                         for x,y in xy])

    def front_line(self, item, name, xy, radius, color):
        xy = np.array(xy)
        z = self.front_depth(item,xy) + .0022
        self.tube(item, name, np.c_[xy, z], radius, color)
        if item=='sundress':self.parts[item][-1]['rig']='torso'
        else:self.parts[item][-1]['follow_fabric']=True

    def front_panel(self, item, name, rows, color):
        xy = np.array([np.linspace(row[0],row[-1],17) for row in rows])
        z = self.front_depth(item,xy.reshape(-1,2)) + .0026
        self.grid(item, name, np.c_[xy.reshape(-1, 2), z].reshape(*xy.shape[:2], 3),
                  color, closed=False)
        self.parts[item][-1]['follow_fabric']=True

    def create(self,only_set=None):
        torso = (np.isin(self.j, [1, 2, 3, 4, 24]) * self.w).sum(1)
        torso_cut = lambda p: np.maximum(torso-.88,
                                        np.minimum(.060-abs(p[:,0]),p[:,1]-.640))
        # A purpose-built continuous dress pattern. Body data supplies skinning
        # and correctives, not the silhouette of the cloth.
        theta=np.arange(144)*2*np.pi/144
        heights=np.linspace(.614,.282,90)
        levels=[.282,.380,.430,.475,.500,.540,.580,.614]
        width=PchipInterpolator(levels,[.156,.130,.113,.096,.091,.087,.087,.083])
        front=PchipInterpolator(levels,[.138,.113,.103,.104,.103,.102,.096,.090])
        back=PchipInterpolator(levels,[-.128,-.103,-.087,-.073,-.064,-.060,-.060,-.060])
        rows=[]
        for y in heights:
            fold=.0105*np.cos(14*theta)*np.clip((.450-y)/.168,0,1)**.9
            fr=float(front(y));bk=float(back(y))
            rows.append(np.stack([(float(width(y))+fold)*np.sin(theta),
                                  np.full(len(theta),y),
                                  (fr+bk)/2+((fr-bk)/2+fold)*np.cos(theta)],1))
        self.grid('sundress','continuous dress fabric',rows[::-1],'dress','whole-dress',correct=False)
        bodice=self.parts['sundress'][0];v=bodice['v']
        eligible=np.flatnonzero((torso>.80)|((self.p[:,1]<.530)&(abs(self.p[:,0])<.105)))
        distance,near=cKDTree(self.p[eligible]).query(v,k=3)
        ids=eligible[near];factor=1/np.maximum(distance,.0001)**3
        factor/=factor.sum(1)[:,None]
        bj,bw,_,_=self.binding(v,'torso',ids,factor)
        sj,sw,_,_=self.binding(v,'skirt',ids,factor)
        trunk=np.zeros((len(v),54));lower=np.zeros_like(trunk)
        for k in range(4):
            np.add.at(trunk,(np.arange(len(v)),bj[:,k]),bw[:,k])
            np.add.at(lower,(np.arange(len(v)),sj[:,k]),sw[:,k])
        fade=np.clip((.490-v[:,1])/.110,0,1);fade=fade*fade*(3-2*fade)
        weights=trunk*(1-fade[:,None])+lower*fade[:,None]
        joints=np.argsort(weights,axis=1)[:,-4:].astype('<u2')
        weights=np.take_along_axis(weights,joints,axis=1)
        weights=(weights/weights.sum(1)[:,None]).astype('<f4')
        # The open, loose bodice must not inherit the body's shoulder bulges
        # when arms rise. Retain trunk corrections only below the chest.
        corrections=[(d[ids]*factor[:,:,None]).sum(1)*(1-fade[:,None])
                     if name.startswith('Hip') else np.zeros_like(v)
                     for name,d in zip(self.target_names,self.delta)]
        bodice.update(skin_joints=joints,skin_weights=weights,corrective_deltas=corrections)
        # Broad cloth straps cut from the actual shoulder surface, not floating tubes.
        self.surface('sundress', 'shoulder straps',
                     [lambda p: p[:, 1] - .600, lambda p: .681 - p[:, 1],
                      lambda p: .012 - abs(abs(p[:, 0]) - .052), torso_cut],
                     'dress', .009, 12, False)
        self.parts['sundress'][-1]['rig']='torso'
        strap=self.parts['sundress'][-1]
        j,w,_,_=self.binding(strap['v'],'torso',strap['donor_ids'],strap['donor_factors'])
        strap.update(skin_joints=j,skin_weights=w,correct=False)
        strap.pop('transfer',None)
        self.grid('sundress', 'wide woven hem', np.array(rows[-4:])[::-1] * [1.006, 1, 1.006],
                  'dress-light', 'skirt', correct=False)
        for side in [-1, 1]:
            self.add('sundress', 'small waist bow',
                     [[0, .481, .106], [side * .022, .491, .107],
                      [side * .021, .470, .108]], [[0, 1, 2]], 'ivory', 'pelvis', False)
        for y in [.590, .558, .526]:
            self.front_line('sundress', 'bodice button', [[0, y], [0, y+.001]], .002, 'ivory')
        if only_set=='starter-sundress':return
        # T-shirt with round rib neckline; intentionally no polo collar.
        self.surface('everyday-tee', 'cotton tee',
                     [lambda p: p[:, 1] - .472, lambda p: .662 - p[:, 1],
                      lambda p: np.maximum(.092-abs(p[:, 0]), p[:, 1]-.571)],
                     'ivory', .012, 16, False)
        for yy in [.601, .609]:
            self.front_line('everyday-tee', 'chest stripe',
                            [[x, yy] for x in np.linspace(-.056, .056, 35)], .002, 'forest')
        # Neck ring follows the clipped fabric boundary.
        v, f = self.parts['everyday-tee'][0]['v'], self.parts['everyday-tee'][0]['f']
        edges = np.concatenate([f[:, [0,1]], f[:, [1,2]], f[:, [2,0]]])
        edges, counts = np.unique(np.sort(edges, axis=1), axis=0, return_counts=True)
        rim = np.unique(edges[counts==1]); neck = v[rim][v[rim,1] > .658]
        neck = neck[np.argsort(np.arctan2(neck[:,0], neck[:,2]-.004))]
        self.tube('everyday-tee', 'round rib neck', neck, .0026, 'forest', closed=True)
        self.pants('denim-breeches', .177, 'denim', .011)
        self.surface('denim-breeches', 'waistband',
                     [lambda p: p[:,1]-.464, lambda p: .485-p[:,1], lambda p: .112-abs(p[:,0])],
                     'denim', .014, 10, False)
        for side in [-1, 1]:
            self.front_line('denim-breeches', 'scoop pocket stitch',
                            [[side*(.034+.046*t), .458-.025*np.sin(t*np.pi/2)] for t in np.linspace(0,1,28)],
                            .0009, 'stitch')
        self.front_line('denim-breeches', 'fly stitch', [[.004,y] for y in np.linspace(.419,.469,18)], .0008, 'stitch')
        # Folded cuffs have their own designed edge, below the original baked hem.
        self.surface('denim-breeches','rolled denim cuffs',
                     [lambda p:p[:,1]-.177,lambda p:.196-p[:,1]],
                     'denim-light',.014,10,False)
        v=self.parts['denim-breeches'][-1]['v']
        center=np.sign(v[:,0])*np.interp(v[:,1],[.07,.18,.30,.405],[.084,.073,.061,.061])
        v[:,0]=center+(v[:,0]-center)*1.14
        v[:,2]=.009+(v[:,2]-.009)*1.12
        self.shoes('canvas-shoes', 'forest', False)
        for rose in [False, True]:
            prefix='rose-' if rose else ''
            item=prefix+'racing-suit'; color='rose' if rose else 'race'; contrast='plum' if rose else 'dark'
            self.surface(item,'one-piece upper',
                         [lambda p:np.maximum(p[:,1]-.469,abs(p[:,0])-.119),lambda p:.676-p[:,1],
                          lambda p:np.maximum(.108-abs(p[:,0]),p[:,1]-.433)],
                         color,.014 if rose else .012,18,False)
            self.pants(item,.117,color,.010 if rose else .013)
            self.surface(item,'soft stand collar',
                         [lambda p:p[:,1]-.664,lambda p:.695-p[:,1],lambda p:.058-abs(p[:,0])],
                         contrast,.018,8,False)
            # Separate front insert and contoured side bands distinguish the rose cut.
            self.front_panel(item,'ivory chest yoke',
                             [[[x,y] for x in [-.071,.071]] for y in np.linspace(.612,.634,12)],'ivory')
            for side in [-1,1]:
                self.front_panel(item,'curved waist insert',
                                 [[[side*x,y] for x in ([.047+.021*((y-.50)/.09)**2,.064+.020*((y-.50)/.09)**2] if rose else [.064,.082])]
                                  for y in np.linspace(.486,.595,32)],contrast)
                self.front_line(item,'contrast piping',
                                [[side*(.041+.021*((y-.50)/.09)**2 if rose else .059),y] for y in np.linspace(.490,.593,35)],
                                .0015,'ivory' if rose else 'orange')
            self.front_line(item,'zipper',[[0,y] for y in np.linspace(.49,.662,64)],.0012,'dark')
            self.front_line(item,'zip pull',[[.002,.650],[.002,.658]],.0020,'metal')
            # Broad adjustable waist belt sits over the seam between upper/lower.
            self.surface(item,'waist belt',[lambda p:p[:,1]-.474,lambda p:.487-p[:,1],lambda p:.112-abs(p[:,0])],
                         contrast,.017,8,False)
            self.front_panel(item,'belt buckle',[[[-.010,y],[.010,y]] for y in np.linspace(.476,.486,4)],'metal')
            # A small original chequered badge; no real racing brand.
            for r in range(2):
                for c in range(3):
                    if (r+c)%2==0:
                        x=.033+c*.006;y=.645+r*.006
                        self.front_panel(item,'check badge',[[[x,y],[x+.005,y]],[[x,y+.005],[x+.005,y+.005]]],'ivory')
            # Ribbed cuffs follow forearms, keeping the hands separate.
            self.surface(item,'wrist cuffs',
                         [lambda p:p[:,1]-.428,lambda p:.447-p[:,1],lambda p:abs(p[:,0])-.135],
                         contrast,.007,10,False)
            glove=prefix+'racing-gloves'
            self.surface(glove,'soft driving gloves',
                         [lambda p:.435-p[:,1],lambda p:abs(p[:,0])-.143],
                         contrast,.0035,6,False)
            self.shoes(prefix+'racing-boots',color,True)
            self.helmet(prefix+'racing-helmet',rose)

    def pants(self,item,hem,color,offset):
        self.surface(item,'smooth leg fabric',
                     [lambda p:p[:,1]-hem,lambda p:.485-p[:,1],lambda p:.112-abs(p[:,0])],
                     color,offset,28,False,fit_bands=[(.300,.405)])
        part=self.parts[item][-1];v=part['v'];blend=np.clip((.405-v[:,1])/.13,0,1)
        center=np.sign(v[:,0])*np.interp(v[:,1],[.07,.18,.30,.405],[.084,.073,.061,.061])
        v[:,0]=center+(v[:,0]-center)*(1+.14*blend)
        v[:,2]=.009+(v[:,2]-.009)*(1+.12*blend)

    def shoes(self,item,accent,boot):
        for side,suffix in [(1,'L'),(-1,'R')]:
            rig=('boot.' if boot else 'foot.')+suffix;cx=side*.085;th=np.arange(64)*2*np.pi/64
            levels=[(-.003,.051,.091,.024),(.012,.052,.092,.024),(.020,.051,.090,.024),
                    (.041,.046,.082,.019),(.062,.039,.066,.010),(.082,.029,.039,-.017)]
            levels=[(y,rx+.006 if y<.05 else rx,rz,cz) for y,rx,rz,cz in levels]
            if boot:
                # A boot last, not a wide shaft grafted onto a narrow sneaker
                # ankle. The rear surface flows continuously from heel to calf.
                yy=[.020,.041,.062,.082,.109,.145]
                width=PchipInterpolator(yy,[.057,.052,.048,.048,.052,.055])
                front=PchipInterpolator(yy,[.114,.102,.078,.050,.038,.035])
                rear=PchipInterpolator(yy,[-.068,-.071,-.074,-.077,-.081,-.085])
                levels=levels[:2]+[(float(y),float(width(y)),float((front(y)-rear(y))/2),
                                   float((front(y)+rear(y))/2)) for y in np.linspace(.020,.145,45)]
            rows=[np.stack([(side*np.interp(y,[.020,.109],[.085,.078]) if boot else cx)+rx*np.sin(th),np.full(64,y),cz+rz*np.cos(th)],1) for y,rx,rz,cz in levels]
            self.grid(item,'rubber sole '+suffix,rows[:3],'sole',rig,correct=False)
            self.grid(item,'upper '+suffix,rows[2:],accent if boot else 'ivory',rig,correct=False)
            self.tube(item,'ankle binding '+suffix,rows[-1],.0025,'dark' if boot else accent,rig,closed=True)
            self.tube(item,'sole stripe '+suffix,rows[1],.0014,accent,rig,closed=True)
            self.add(item,'sole underside '+suffix,[[cx,-.003,.024]]+rows[0].tolist(),
                     [[0,(i+1)%64+1,i+1] for i in range(64)],'dark',rig,False)
            for k in range(3 if boot else 4):
                if boot:
                    y=.052+k*.012;rx=float(width(y));cz=float((front(y)+rear(y))/2);rz=float((front(y)-rear(y))/2)
                    xx=np.linspace(-.020,.020,15);center=side*np.interp(y,[.020,.109],[.085,.078])
                    self.tube(item,'closure '+suffix,
                              np.c_[center+xx,np.full(len(xx),y),cz+rz*np.sqrt(1-(xx/rx)**2)+.002],
                              .002,'ivory',rig)
                    continue
                z=.024+k*.012;y=.085-k*.004
                self.tube(item,'closure '+suffix,[[cx-.020,y,z],[cx,y+.002,z],[cx+.020,y,z]],
                          .003 if boot else .0015,'ivory' if boot else accent,rig)

    def helmet(self,item,rose):
        color='ivory' if rose else 'race';accent='rose' if rose else 'orange'
        # A complete protective shell with a real front eye opening.
        th=np.linspace(-np.pi,np.pi,145)
        heights=np.unique(np.r_[np.linspace(.701,.995,52),.779,.879])
        def ring(y,angles):
            radius=np.sqrt(max(.00001,1-((y-.844)/.153)**2))
            chin=max(.132-.166*radius,0)*np.maximum(np.cos(angles),0)**2 if y<.779 else 0
            return np.stack([.151*radius*np.sin(angles),np.full(len(angles),y),
                             .006+(.166*radius+chin)*np.cos(angles)],1)
        for lo,hi in zip(heights[:-1],heights[1:]):
            for j in range(len(th)-1):
                if .779<=lo and hi<=.879 and abs((th[j]+th[j+1])/2)<1.07:continue
                self.grid(item,'helmet shell',[ring(lo,th[j:j+2]),ring(hi,th[j:j+2])],color,'head',False,False)
        # Visor spans the opening, with a dark gasket and separately editable material.
        a=np.linspace(-1.08,1.08,70);rows=[]
        for y in np.linspace(.777,.881,20):
            row=ring(y,a);row[:,0]*=1.018;row[:,2]=.006+(row[:,2]-.006)*1.024
            rows.append(row)
        self.grid(item,'curved visor',rows,'visor','head',False,False)
        for edge in [rows[0],rows[-1],np.array(rows)[:,0],np.array(rows)[:,-1]]:
            self.tube(item,'visor seal',edge,.0022,'dark','head')
        # Longitudinal stripe follows the crown and back, rather than a palette-only variant.
        stripe=[]
        for phi in np.linspace(-2.43,2.43,151):
            width=.012 if rose else .018
            row=[]
            for x in np.linspace(-width,width,7):
                radial=np.sqrt(1-(x/.1512)**2)
                row.append([x,.844+.1532*np.cos(phi)*radial,.006+.1662*np.sin(phi)*radial])
            stripe.append(row)
        for a,b in zip(stripe[:-1],stripe[1:]):
            mid=(np.array(a)+np.array(b))/2
            if mid[0,2]>.01 and .775<mid[0,1]<.884:continue
            self.grid(item,'crown racing stripe',[a,b],accent,'head',False,False)
        # Two small chin vents and contrasting lower rim.
        lower=ring(.735,np.linspace(-2.9,2.9,100))
        self.tube(item,'chin colour line',lower,.003,accent,'head')
        for side in [-1,1]:
            for yy in [.752,.763]:
                q=ring(yy,np.linspace(side*.28,side*.59,18));q[:,2]+=.002
                self.tube(item,'chin vent',q,.0018,'dark','head')

    def export(self,items,path):
        super().export(items,path)
        model=GLB(path)
        for m in model.g['materials']:
            if m['name']=='visor':
                m['alphaMode']='BLEND'
                m['pbrMetallicRoughness'].update(baseColorFactor=base.rgb(PALETTE['visor'])+[.26],
                                               roughnessFactor=.19,metallicFactor=.12)
        # Preserve buffers byte-for-byte when changing only material metadata.
        b=object.__new__(Builder);b.g=model.g;b.b=bytearray(model.b);b.save(path)


def combine(folder,items,path):
    """Assemble exact standalone meshes without repeating expensive fitting."""
    first=GLB(folder/(items[0]+'.glb'));old=first.g
    g=copy.deepcopy(old);g['meshes']=[];g['materials']=[]
    for node in g['nodes']:
        if 'mesh' in node:node['name']='Retained source anchor'
        node.pop('mesh',None);node.pop('skin',None);node.pop('weights',None)
    b=Builder(g)
    g['skins'][0]['inverseBindMatrices']=b.acc(first.acc(old['skins'][0]['inverseBindMatrices']),'MAT4')
    for anim,original in zip(g['animations'],old['animations']):
        for sampler,source in zip(anim['samplers'],original['samplers']):
            for field in ['input','output']:
                sampler[field]=b.acc(first.acc(source[field]),old['accessors'][source[field]]['type'])
        anim['channels']=[c for c in anim['channels'] if c['target']['path']!='weights']
    material_ids={}
    for item in items:
        model=GLB(folder/(item+'.glb'));mesh=copy.deepcopy(model.g['meshes'][0])
        for primitive in mesh['primitives']:
            mat=model.g['materials'][primitive['material']]
            if mat['name'] not in material_ids:
                material_ids[mat['name']]=len(g['materials']);g['materials'].append(copy.deepcopy(mat))
            primitive['material']=material_ids[mat['name']]
            for semantic,index in list(primitive['attributes'].items()):
                primitive['attributes'][semantic]=b.acc(model.acc(index),model.g['accessors'][index]['type'])
            primitive['indices']=b.acc(model.acc(primitive['indices']).ravel())
            for target in primitive.get('targets',[]):
                for semantic,index in list(target.items()):
                    target[semantic]=b.acc(model.acc(index),model.g['accessors'][index]['type'],sparse=True)
        node=len(g['nodes']);g['nodes'].append({'name':item,'mesh':len(g['meshes']),'skin':0})
        g['meshes'].append(mesh);g['scenes'][g.get('scene',0)]['nodes'].append(node)
        for anim,original in zip(g['animations'],old['animations']):
            for channel in original['channels']:
                if channel['target']['path']=='weights':
                    c=copy.deepcopy(channel);c['target']['node']=node;anim['channels'].append(c)
    g['extras']={'trainerWearables':{'rig':'trainer-humanoid-54-v1','items':items,'simulation':False}}
    b.save(path)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'assets/avatar/collections/starter-racing-v1')
    parser.add_argument('--only-set',choices=list(SETS))
    args=parser.parse_args()
    out=args.output;out.mkdir(parents=True,exist_ok=True)
    base.COLORS.clear();base.COLORS.update(PALETTE)
    author=Collection();author.create(args.only_set)
    for item in author.parts:
        if args.only_set and item not in SETS[args.only_set]:continue
        author.export([item],out/(item+'.glb'));print(item,flush=True)
    for key,items in SETS.items():
        if not args.only_set or key==args.only_set:combine(out,items,out/(key+'-outfit.glb'))
    all_items=[item for items in SETS.values() for item in items]
    combine(out,all_items,out/'starter-racing-wardrobe.glb')
    manifest={'schema':1,'collection':'starter-racing-v1','artRevision':1,'status':'art-review',
              'rig':'trainer-humanoid-54-v1','units':'metres','forward':'+Z','simulation':False,
              'baseBodySha256':hashlib.sha256((ROOT/'assets/avatar/approved/base-body/trainer-body.glb').read_bytes()).hexdigest(),
              'sets':SETS,'palette':PALETTE,
              'items':{i:{'file':i+'.glb','node':i,'sha256':hashlib.sha256((out/(i+'.glb')).read_bytes()).hexdigest()} for i in all_items},
              'starterSets':['starter-sundress','starter-denim'],
              'starterIntent':'Both free after character creation; no automatic grant implemented in this authoring collection.',
              'existingAccessories':{'starter-sundress':['sandals']},
              'helmetHairPolicy':'Hide hair only while equipped; restore selected hairstyle on removal. Review renderer exercises this; runtime integration pending.',
              'dressSlotPolicy':'Full outfit covers top and bottom; selection/coverage contract pending runtime integration.',
              'runtimeInstalled':False}
    (out/'collection.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Four outfits exported to',out)


if __name__=='__main__':
    main()
