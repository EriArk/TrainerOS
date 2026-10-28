"""Author separate golf wearables against the retained Trainer skeleton.

python tools/avatar/build_golf_clothes.py --output assets/avatar/collections/golf-v1
No live pack/profile changes. Units metres, Y up, +Z front, shared 54-joint bind.
"""
import argparse,copy,hashlib,json,math
from pathlib import Path
import numpy as np
from scipy.spatial import cKDTree
from scipy.sparse import coo_matrix
from build_pack import GLB,Builder

ROOT=Path(__file__).resolve().parents[2]
COLORS={'ivory':'f4f0df','forest':'234f43','peach':'efa58b','sole':'dedccf','dark':'213b34','metal':'d8b679'}
def rgb(h):
    c=np.array([int(h[i:i+2],16)/255 for i in (0,2,4)])
    return np.where(c<=.04045,c/12.92,((c+.055)/1.055)**2.4).tolist()

def normals(v,f):
    n=np.zeros_like(v);fn=np.cross(v[f[:,1]]-v[f[:,0]],v[f[:,2]]-v[f[:,0]])
    for i in range(3):np.add.at(n,f[:,i],fn)
    return n/np.maximum(np.linalg.norm(n,axis=1)[:,None],1e-12)

class Author:
    def __init__(self):
        self.src=GLB(ROOT/'assets/avatar/approved/base-body/trainer-body.glb');g=self.src.g
        self.body=g['meshes'][0];p=self.body['primitives'][0];a=p['attributes']
        self.p=self.src.acc(a['POSITION']).astype(float);self.n=self.src.acc(a['NORMAL']).astype(float)
        self.f=self.src.acc(p['indices']).reshape(-1,3)
        # UV seams may have distinct normals. Offset a welded surface so the
        # fabric does not inherit cracks from the body's texture boundaries.
        welded,rev=np.unique(np.round(self.p,5),axis=0,return_inverse=True)
        self.n=normals(welded,rev[self.f])[rev]
        self.j=self.src.acc(a['JOINTS_0']);self.w=self.src.acc(a['WEIGHTS_0'])
        self.names=[g['nodes'][i]['name'] for i in g['skins'][0]['joints']]
        self.tree=cKDTree(self.p);self.parts={};self.targets=p.get('targets',[])
        self.delta=[self.src.acc(t['POSITION']) for t in self.targets]
        self.target_names=self.body.get('extras',{}).get('targetNames',[])
    def add(self,item,name,v,f,color,rig='body',correct=True):
        v=np.array(v,dtype=float);f=np.array(f,dtype=np.uint32)
        area=np.linalg.norm(np.cross(v[f[:,1]]-v[f[:,0]],v[f[:,2]]-v[f[:,0]]),axis=1)
        f=f[area>1e-11]
        self.parts.setdefault(item,[]).append(dict(name=name,v=v,f=f,color=color,rig=rig,correct=correct))
    def tube(self,item,name,points,r,color,rig='body',closed=False):
        q=np.array(points);v=[];steps=8
        for k,p in enumerate(q):
            direction=q[(k+1)%len(q)]-q[(k-1)%len(q)] if closed else q[min(k+1,len(q)-1)]-q[max(k-1,0)]
            direction/=max(np.linalg.norm(direction),1e-8)
            axis=np.cross(direction,[0,0,1])
            if np.linalg.norm(axis)<.01:axis=np.cross(direction,[1,0,0])
            axis/=np.linalg.norm(axis);other=np.cross(direction,axis)
            v.extend(p+r*(np.cos(t)*axis+np.sin(t)*other) for t in np.arange(steps)*2*np.pi/steps)
        f=[]
        for k in range(len(q) if closed else len(q)-1):
            for j in range(steps):
                a=k*steps+j;b=k*steps+(j+1)%steps;c=((k+1)%len(q))*steps+j;d=((k+1)%len(q))*steps+(j+1)%steps
                f.extend([[a,c,b],[b,c,d]])
        self.add(item,name,v,f,color,rig)
    def grid(self,item,name,rows,color,rig='body',closed=True,correct=True):
        rows=np.array(rows);ny,nx,_=rows.shape;f=[]
        for i in range(ny-1):
            for j in range(nx if closed else nx-1):
                a=i*nx+j;b=i*nx+(j+1)%nx;c=(i+1)*nx+j;d=(i+1)*nx+(j+1)%nx
                f.extend([[a,b,c],[b,d,c]])
        self.add(item,name,rows.reshape(-1,3),f,color,rig,correct)
    def surface(self,item,name,conditions,color,offset=.006,smooth=14,edge=True):
        # Clip body triangles with interpolated boundary points, then weld and
        # relax the derived garment. The underlying body remains untouched.
        values=np.array([fn(self.p) for fn in conditions]).T
        candidates=self.f[np.all(values[self.f].max(1)>=0,axis=1)]
        vv=[];ff=[];donor_ids=[];donor_factors=[]
        for face in candidates:
            poly=[np.r_[self.p[i]+self.n[i]*offset,values[i],np.eye(3)[k]] for k,i in enumerate(face)]
            for c in range(len(conditions)):
                out=[]
                for i,a in enumerate(poly):
                    b=poly[(i+1)%len(poly)];av=a[3+c];bv=b[3+c]
                    if av>=0:out.append(a)
                    if (av>=0)!=(bv>=0):out.append(a+(b-a)*av/(av-bv))
                poly=out
                if not poly:break
            if len(poly)<3:continue
            at=len(vv);vv.extend(p[:3] for p in poly);ff.extend([at,at+i,at+i+1] for i in range(1,len(poly)-1))
            donor_ids.extend([face]*len(poly));donor_factors.extend(p[-3:] for p in poly)
        v=np.array(vv);f=np.array(ff)
        v,first,rev=np.unique(np.round(v,5),axis=0,return_index=True,return_inverse=True);f=rev[f]
        edges=np.concatenate([f[:,[0,1]],f[:,[1,2]],f[:,[2,0]]]);edges,count=np.unique(np.sort(edges,axis=1),axis=0,return_counts=True)
        boundary=np.unique(edges[count==1]);interior=np.ones(len(v),bool);interior[boundary]=False
        e=np.r_[edges,edges[:,::-1]];adj=coo_matrix((np.ones(len(e)),(e[:,0],e[:,1])),shape=(len(v),len(v))).tocsr();deg=np.maximum(np.asarray(adj.sum(1)).ravel(),1)
        for _ in range(smooth):
            avg=(adj@v)/deg[:,None];v[interior]=v[interior]*.7+avg[interior]*.3
        # Relaxation must not erase the clearance at shoulder convexities.
        _,near=self.tree.query(v);signed=((v-self.p[near])*self.n[near]).sum(1)
        v+=self.n[near]*np.maximum(offset*.8-signed,0)[:,None]
        self.add(item,name,v,f,color)
        self.parts[item][-1]['donor_ids']=np.array(donor_ids)[first]
        self.parts[item][-1]['donor_factors']=np.array(donor_factors)[first]
        # Rounded fabric edge on every opening (neck / arms / hem).
        links={}
        for a,b in edges[count==1]:links.setdefault(a,[]).append(b);links.setdefault(b,[]).append(a)
        unseen=set(links) if edge else set()
        loop_id=0
        while unseen:
            start=next(iter(unseen));loop=[start];prev=-1;curr=start
            while True:
                unseen.discard(curr);choices=[x for x in links[curr] if x!=prev]
                if not choices:break
                nxt=choices[0]
                if nxt==start:break
                if nxt not in unseen:break
                loop.append(nxt);prev,curr=curr,nxt
            if len(loop)>8:
                q=v[loop]
                if np.linalg.norm(np.roll(q,-1,axis=0)-q,axis=1).sum()<.06:continue
                self.tube(item,name+' edge '+str(loop_id),q,.0013,'forest' if color=='ivory' else 'ivory',closed=True);loop_id+=1
        return v,f
    def neck(self,item,main):
        # Open-front, folded polo collar with two pointed front leaves.
        self.surface(item,'rear folded collar',[lambda p:p[:,1]-.653,lambda p:.678-p[:,1],lambda p:.004-p[:,2],lambda p:.062-abs(p[:,0])],main,.018,8,False)
        for side in [-1,1]:
            inner_front=np.array([side*.018,.680,.050]);inner_side=np.array([side*.052,.680,.004])
            outer_front=np.array([side*.030,.626,.080]);outer_side=np.array([side*.069,.653,.020])
            wing=[]
            for t in np.linspace(0,1,8):
                a=inner_front*(1-t)+outer_front*t;b=inner_side*(1-t)+outer_side*t
                wing.append([a*(1-u)+b*u for u in np.linspace(0,1,12)])
            wing=np.array(wing)
            fabric=self.parts[item][0]['v']
            front=fabric[(fabric[:,2]>0)&(fabric[:,1]>.60)&(fabric[:,1]<.71)]
            distance,near=cKDTree(front[:,:2]).query(wing[:,:,:2].reshape(-1,2),k=16)
            depth=front[near,2].max(1)
            wing[:,:,2]=np.maximum(wing[:,:,2],depth.reshape(wing.shape[:2])+.004)
            self.grid(item,'pointed collar leaf',wing if side<0 else wing[::-1],main,closed=False)
            self.tube(item,'collar leaf edge',wing[-1],.0013,'peach')
        # A narrow placket follows the actual torso depth instead of floating.
        y=np.linspace(.601,.662,14);rows=[]
        for yy in y:
            row=[]
            for xx in [-.009,.009]:
                donor=self.p[(abs(self.p[:,0])<.025)&(abs(self.p[:,1]-yy)<.004)&(self.p[:,2]>0)]
                z=float(np.quantile(donor[:,2],.90)) if len(donor) else .047
                row.append([xx,yy,z+.017])
            rows.append(row)
        self.grid(item,'button placket',rows,main,closed=False)
        for k in [4,9]:
            p=(np.array(rows[k][0])+rows[k][1])/2;p[2]+=.001
            self.disc(item,'button '+str(k),p,.0022,'metal')
    def disc(self,item,name,p,r,color,rig='body'):
        pts=[p]+[np.array(p)+[r*np.cos(a),r*np.sin(a),.0008] for a in np.arange(16)*np.pi/8]
        self.add(item,name,pts,[[0,j+1,(j+1)%16+1] for j in range(16)],color,rig)
    def create(self):
        p=self.p
        for female in [False,True]:
            item='polo-sleeveless' if female else 'polo-short-sleeve';color='forest' if female else 'ivory'
            conditions=[lambda p:p[:,1]-.478,lambda p:.672-p[:,1]]
            if female:
                conditions.append(lambda p:.087+.020*np.clip((p[:,1]-.602)/.057,0,1)-abs(p[:,0]))
            else:conditions.append(lambda p:np.maximum(.092-abs(p[:,0]),p[:,1]-.574))
            self.surface(item,'polo fabric',conditions,color,.011,16)
            self.neck(item,'ivory' if female else 'forest')
            # Small stitched golf flag, intentionally readable at chibi scale.
            x=.045;y=.603
            donor=p[(abs(p[:,0]-x)<.006)&(abs(p[:,1]-y)<.006)&(p[:,2]>0)];z=float(donor[:,2].max())+.016
            self.tube(item,'flag stem',[[x,y-.008,z],[x,y+.009,z]],.001,'peach')
            self.add(item,'flag pennant',[[x,y+.009,z],[x+.012,y+.004,z],[x,y,z]],[[0,1,2]],'peach')
        self.surface('golf-shorts','tailored shorts',[lambda p:p[:,1]-.307,lambda p:.485-p[:,1],lambda p:.111-abs(p[:,0])],'forest',.009,28)
        self.surface('golf-shorts','waistband',[lambda p:p[:,1]-.465,lambda p:.485-p[:,1],lambda p:.112-abs(p[:,0])],'forest',.011,10)
        for side in [-1,1]:
            points=[]
            for t in np.linspace(0,1,14):
                x=side*(.061+.027*t);y=.454-.035*t
                near=p[(abs(p[:,0]-x)<.005)&(abs(p[:,1]-y)<.005)&(p[:,2]>0)]
                points.append([x,y,float(near[:,2].max())+.012])
            self.tube('golf-shorts','pocket welt',points,.0015,'ivory')
        # A real open skirt. No extra shorts mesh, no sealed disk underneath.
        for panel in range(10):
            rows=[];theta=np.linspace(panel,panel+1,17)*2*np.pi/10;q=np.linspace(0,1,17)
            for t in np.linspace(0,1,18):
                pleat=np.where(q<.1875,(1-q/.1875),(q-.1875)/.8125)*.013*t**.65
                rx=.104+.025*t+pleat;rz=.087+.026*t+pleat
                rows.append(np.stack([rx*np.sin(theta),np.full(len(theta),.486-.181*t),.008+rz*np.cos(theta)],1))
            self.grid('pleated-skirt','pleat '+str(panel),rows[2:][::-1],'ivory','skirt',False,False)
            self.grid('pleated-skirt','waist band '+str(panel),rows[:3][::-1],'forest','skirt',False,False)
            trim=np.array(rows[-2:]);trim[:,:,0]*=1.008;trim[:,:,2]=.008+(trim[:,:,2]-.008)*1.008
            self.grid('pleated-skirt','peach hem',trim[::-1],'peach','skirt',False,False)
            self.tube('pleated-skirt','hem piping',rows[-1],.0012,'forest','skirt')
        for item,top in [('ankle-socks',.135),('crew-socks',.187)]:
            self.surface(item,'knitted socks',[lambda p:top-p[:,1]],'ivory',.0028,8)
            for yy in [top-.013,top-.023]:
                self.surface(item,'woven stripe',[lambda p:yy-p[:,1],lambda p:p[:,1]-(yy-.004)],'forest' if yy>top-.02 else 'peach',.0034,3,False)
        for item,accent in [('golf-shoes-classic','forest'),('golf-shoes-sport','peach')]:
            for side,suffix in [(1,'L'),(-1,'R')]:
                rig='foot.'+suffix;cx=side*.085;th=np.arange(64)*2*np.pi/64
                # Wide rounded toe box encloses the retained bare foot.
                rows=[]
                for yy,rx,rz,cz in [(-.003,.051,.091,.024),(.005,.052,.092,.024),(.017,.051,.091,.024),(.032,.049,.087,.022),(.047,.046,.080,.020),(.062,.039,.066,.010),(.082,.028,.037,-.018)]:
                    rows.append(np.stack([cx+rx*np.sin(th),np.full(len(th),yy),cz+rz*np.cos(th)],1))
                self.grid(item,'outsole '+suffix,rows[:3],'sole',rig,correct=False)
                self.grid(item,'leather upper '+suffix,rows[2:],'ivory',rig,correct=False)
                self.tube(item,'ankle binding '+suffix,rows[-1],.003,accent,rig,closed=True)
                self.tube(item,'sole stripe '+suffix,rows[2],.0015,accent,rig,closed=True)
                center=[cx,-.003,.024];vv=[center]+rows[0].tolist();ff=[[0,(i+1)%64+1,i+1] for i in range(64)]
                self.add(item,'sole bottom '+suffix,vv,ff,'dark',rig,False)
                for k in range(4):
                    z=.025+k*.011;y=.085-k*.004
                    self.tube(item,'lace '+suffix+str(k),[[cx-.020,y,z],[cx,y+.002,z-.002],[cx+.020,y,z]],.0016,'forest',rig)
                # Contrast heel tab and side piping.
                self.grid(item,'heel tab '+suffix,[[[cx-.012,.026,-.068],[cx+.012,.026,-.068]],[[cx-.009,.074,-.052],[cx+.009,.074,-.052]]],accent,rig,False,False)
        self.hat('golf-cap',False);self.hat('golf-visor',True)
    def hat(self,item,visor):
        rx,rz=(.153,.173) if visor else (.176,.203);y=.884;th=np.arange(96)*2*np.pi/96
        band=[]
        for h in [0,.017]:band.append(np.stack([rx*np.sin(th),np.full(len(th),y+h),rz*np.cos(th)],1))
        self.grid(item,'head band',band,'ivory' if visor else 'forest','head',correct=False)
        self.tube(item,'band piping',band[0],.0018,'peach','head',closed=True)
        if not visor:
            rows=[]
            for t in np.linspace(0,np.pi/2,22):
                rows.append(np.stack([rx*np.cos(t)**.55*np.sin(th),np.full(len(th),y+.017+.115*np.sin(t)),rz*np.cos(t)**.55*np.cos(th)],1))
            self.grid(item,'six panel crown',rows,'forest','head',correct=False)
            for a in np.arange(6)*np.pi/3:
                points=[[rx*np.cos(t)**.55*np.sin(a),y+.018+.115*np.sin(t),rz*np.cos(t)**.55*np.cos(a)] for t in np.linspace(0,np.pi/2,40)]
                self.tube(item,'crown seam',points,.0008,'ivory','head')
            self.disc(item,'front club mark',[0,y+.067,rz*.94],.008,'peach','head')
            badge=self.parts[item][-1]['v'];t=np.arcsin(np.clip((badge[:,1]-y-.017)/.115,0,1))
            badge[:,2]=rz*np.cos(t)**.55+.0015
        rows=[]
        a=np.linspace(-1.16,1.16,56)
        for t in np.linspace(0,1,9):
            reach=.078*np.cos(a)**1.5*t
            rows.append(np.stack([(rx+reach*.38)*np.sin(a),y-.006- .012*t+ .008*np.sin(a)**2,(rz+reach)*np.cos(a)],1))
        self.grid(item,'curved brim',rows[::-1],'forest','head',False,False)
        self.tube(item,'brim binding',rows[-1],.002,'peach','head')
    def binding(self,v,rig,ids=None,factor=None):
        if ids is None:
            distance,ids=self.tree.query(v,k=3);factor=1/np.maximum(distance,.0001)**3;factor/=factor.sum(1)[:,None]
        weights=np.zeros((len(v),54))
        for k in range(3):
            for j in range(4):np.add.at(weights,(np.arange(len(v)),self.j[ids[:,k],j]),self.w[ids[:,k],j]*factor[:,k])
        if rig in self.names:
            weights[:]=0;weights[:,self.names.index(rig)]=1
        elif rig=='skirt':
            # Continuous front/back weighting; broad pleats may separate in a
            # deep sitting pose. This is skinned fabric, not cloth simulation.
            weights[:]=0;t=np.clip((.480-v[:,1])/.065,0,1)*.77
            left=np.clip(.5+v[:,0]/.08,0,1)
            weights[:,1]=1-t;weights[:,46]=t*left;weights[:,50]=t*(1-left)
        joints=np.argsort(weights,axis=1)[:,-4:];values=np.take_along_axis(weights,joints,1);values/=values.sum(1)[:,None]
        return joints.astype('<u2'),values.astype('<f4'),ids,factor
    def export(self,items,path):
        src=self.src;old=src.g
        # Keep original joint node indices/transforms and animations, remove body.
        g=copy.deepcopy(old);g['meshes']=[];g['materials']=[];g.pop('images',None);g.pop('textures',None);g.pop('samplers',None)
        for n in g['nodes']:n.pop('mesh',None);n.pop('skin',None);n.pop('weights',None)
        g['extras']={'trainerWearables':{'rig':'trainer-humanoid-54-v1','items':items,'simulation':False}}
        body_node=next(i for i,n in enumerate(old['nodes']) if 'mesh' in n)
        b=Builder(g)
        g['skins'][0]['inverseBindMatrices']=b.acc(src.acc(old['skins'][0]['inverseBindMatrices']),'MAT4')
        for anim,source_anim in zip(g.get('animations',[]),old.get('animations',[])):
            for sampler,original in zip(anim['samplers'],source_anim['samplers']):
                for field in ['input','output']:sampler[field]=b.acc(src.acc(original[field]),old['accessors'][original[field]]['type'])
            anim['channels']=[c for c in anim['channels'] if c['target']['path']!='weights']
        materials={}
        for color,h in COLORS.items():
            materials[color]=len(g['materials']);g['materials'].append({'name':color,'doubleSided':True,'pbrMetallicRoughness':{'baseColorFactor':rgb(h)+[1],'metallicFactor':.55 if color=='metal' else 0,'roughnessFactor':.48 if color=='metal' else .84}})
        mesh_nodes=[]
        for item in items:
            primitives=[]
            # Merge parts of a material, keeping each wearable a separate node.
            for color in COLORS:
                parts=[p for p in self.parts[item] if p['color']==color]
                if not parts:continue
                vv=[];ff=[];jj=[];ww=[];dd=[[] for _ in self.delta];offset=0
                for part in parts:
                    v=part['v'];f=part['f'];ids=part.get('donor_ids');factor=part.get('donor_factors')
                    if ids is None and item.startswith('polo') and part['rig']=='body':
                        fabric=self.parts[item][0];_,near=cKDTree(fabric['v']).query(v)
                        ids=fabric['donor_ids'][near];factor=fabric['donor_factors'][near]
                    j,w,ids,factor=self.binding(v,part['rig'],ids,factor)
                    vv.append(v);ff.append(f+offset);offset+=len(v);jj.append(j);ww.append(w)
                    for k,d in enumerate(self.delta):dd[k].append((d[ids]*factor[:,:,None]).sum(1) if part['correct'] and part['rig']=='body' else np.zeros_like(v))
                v=np.concatenate(vv);f=np.concatenate(ff);uv=np.stack([v[:,0]*4+.5,v[:,1]*4],1)
                att={'POSITION':b.acc(v.astype('<f4')),'NORMAL':b.acc(normals(v,f).astype('<f4')),'TEXCOORD_0':b.acc(uv.astype('<f4')),'JOINTS_0':b.acc(np.concatenate(jj)),'WEIGHTS_0':b.acc(np.concatenate(ww))}
                primitive={'attributes':att,'indices':b.acc(f.astype('<u4').ravel()),'material':materials[color]}
                primitive['targets']=[{'POSITION':b.acc(np.concatenate(d).astype('<f4'),sparse=True)} for d in dd]
                primitives.append(primitive)
            mesh_id=len(g['meshes']);g['meshes'].append({'name':item,'primitives':primitives,'weights':[0]*len(self.delta),'extras':{'targetNames':self.target_names}})
            node=len(g['nodes']);g['nodes'].append({'name':item,'mesh':mesh_id,'skin':0});g['scenes'][g.get('scene',0)]['nodes'].append(node);mesh_nodes.append(node)
            # Body corrective curves also drive the matching clothing targets.
            for anim,original in zip(g.get('animations',[]),old.get('animations',[])):
                for channel in original['channels']:
                    if channel['target']['node']==body_node and channel['target']['path']=='weights':
                        c=copy.deepcopy(channel);c['target']['node']=node;anim['channels'].append(c)
        b.save(path)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();out=args.output;out.mkdir(parents=True,exist_ok=True)
    author=Author();author.create()
    sets={'mens':['polo-short-sleeve','golf-shorts','ankle-socks','golf-shoes-classic','golf-cap'],'womens':['polo-sleeveless','pleated-skirt','crew-socks','golf-shoes-sport','golf-visor']}
    for item in author.parts:author.export([item],out/(item+'.glb'));print(item,flush=True)
    for key,items in sets.items():author.export(items,out/(key+'-outfit.glb'))
    author.export(list(author.parts),out/'golf-wardrobe.glb')
    manifest={'schema':1,'collection':'golf-v1','status':'art-review','rig':'trainer-humanoid-54-v1','units':'metres','forward':'+Z','simulation':False,'baseBodySha256':hashlib.sha256((ROOT/'assets/avatar/approved/base-body/trainer-body.glb').read_bytes()).hexdigest(),'sets':sets,'palette':COLORS,'items':{item:{'file':item+'.glb','node':item,'sha256':hashlib.sha256((out/(item+'.glb')).read_bytes()).hexdigest()} for item in author.parts},'skirtUnderlayer':'existing body shorts; no added shorts mesh','headwearReviewFits':{'golf-cap':'crop','golf-visor':'bob'},'runtimeInstalled':False}
    (out/'collection.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Separate skinned garments exported:',out)

if __name__=='__main__':main()
