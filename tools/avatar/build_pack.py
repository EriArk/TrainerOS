"""Build the shared avatar runtime pack from the retained authoring inputs.

Run with the constructor public/assets directory and an external output directory.
Preserves body vertices, skin and all corrective animation channels. Sparse morphs
are lossless; only texture resolution changes. Requires numpy, scipy, Pillow.
"""
import argparse, copy, hashlib, io, json, struct
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
from scipy.spatial import cKDTree

DT={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}
NC={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}
class GLB:
    def __init__(self,path):
        b=Path(path).read_bytes(); n=struct.unpack_from('<I',b,12)[0]
        self.g=json.loads(b[20:20+n]);self.b=b[28+n:]
    def acc(self,i):
        a=self.g['accessors'][i]; nc=NC[a['type']];dt=np.dtype(DT[a['componentType']])
        out=np.zeros((a['count'],nc),dtype=dt)
        if 'bufferView' in a:
            v=self.g['bufferViews'][a['bufferView']]
            out=np.ndarray(out.shape,dtype=dt,buffer=self.b,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',dt.itemsize*nc),dt.itemsize)).copy()
        if 'sparse' in a:
            s=a['sparse'];v=self.g['bufferViews'][s['indices']['bufferView']];w=self.g['bufferViews'][s['values']['bufferView']]
            ids=np.frombuffer(self.b,DT[s['indices']['componentType']],s['count'],v.get('byteOffset',0))
            out[ids]=np.frombuffer(self.b,dt,s['count']*nc,w.get('byteOffset',0)).reshape(-1,nc)
        return out
    def image(self,i):
        v=self.g['bufferViews'][self.g['images'][i]['bufferView']];o=v.get('byteOffset',0)
        return Image.open(io.BytesIO(self.b[o:o+v['byteLength']])).convert('RGB')
class Builder:
    def __init__(self,g):self.g=g;self.b=bytearray();g['bufferViews']=[];g['accessors']=[]
    def view(self,raw):
        self.b.extend(b'\0'*((-len(self.b))%4));i=len(self.g['bufferViews'])
        self.g['bufferViews'].append({'buffer':0,'byteOffset':len(self.b),'byteLength':len(raw)});self.b.extend(raw);return i
    def acc(self,arr,typ=None,sparse=False):
        arr=np.ascontiguousarray(arr);arr=arr.reshape(-1,1) if arr.ndim==1 else arr
        component={np.dtype(v):k for k,v in DT.items()}[arr.dtype]
        a={'componentType':component,'count':len(arr),'type':typ or {v:k for k,v in NC.items()}[arr.shape[1]]}
        ids=np.flatnonzero(np.any(arr!=0,axis=1)).astype('<u4')
        if sparse and len(ids)<len(arr)*.7:
            if len(ids):a['sparse']={'count':len(ids),'indices':{'bufferView':self.view(ids.tobytes()),'componentType':5125},'values':{'bufferView':self.view(arr[ids].tobytes())}}
        else:a['bufferView']=self.view(arr.tobytes())
        if a['type']=='VEC3':a.update(min=arr.min(0).tolist(),max=arr.max(0).tolist())
        i=len(self.g['accessors']);self.g['accessors'].append(a);return i
    def save(self,path):
        self.b.extend(b'\0'*((-len(self.b))%4));self.g['buffers']=[{'byteLength':len(self.b)}]
        raw=json.dumps(self.g,separators=(',',':')).encode();raw+=b' '*((-len(raw))%4)
        Path(path).write_bytes(struct.pack('<III',0x46546c67,2,28+len(raw)+len(self.b))+struct.pack('<II',len(raw),0x4e4f534a)+raw+struct.pack('<II',len(self.b),0x004e4942)+self.b)

def png(im):
    f=io.BytesIO();im.save(f,format='PNG');return f.getvalue()
def build(source,out,shirt=None):
    out.mkdir(parents=True,exist_ok=True)
    src=GLB(source/'body-round.glb');g=copy.deepcopy(src.g);b=Builder(g)
    # Keep accessor indices stable, compress the large predominantly-zero correctives.
    morphs={i for m in src.g['meshes'] for p in m['primitives'] for t in p.get('targets',[]) for i in t.values()}
    for i,a in enumerate(src.g['accessors']):
        assert b.acc(src.acc(i),a['type'],i in morphs)==i
    primitive=g['meshes'][0]['primitives'][0];att=primitive['attributes']
    P=src.acc(att['POSITION']);J=src.acc(att['JOINTS_0']);W=src.acc(att['WEIGHTS_0']);UV=src.acc(att['TEXCOORD_0']);F=src.acc(primitive['indices']).reshape(-1,3)
    names=[g['nodes'][i]['name'] for i in g['skins'][0]['joints']]
    torso=np.isin(J,[names.index(n) for n in ['root','pelvis','spine','chest']])*W
    inside=(P[:,1]>.448)&(P[:,1]<.59)&(abs(P[:,0])<.105)&(abs(P[:,2])<.105)&(torso.sum(1)>.98)
    primitive['indices']=b.acc(F[~inside[F].all(1)].astype('<u4').ravel())
    g.setdefault('extras',{})['uncoveredBodyIndices']=src.g['meshes'][0]['primitives'][0]['indices']
    # One shared body. The other approved eye surface is a small positional patch.
    almond=GLB(source/'body-almond.glb');ap=almond.g['meshes'][0]['primitives'][0]
    assert np.array_equal(F,almond.acc(ap['indices']).reshape(-1,3))
    patches=[]
    for semantic in ['POSITION','NORMAL']:
        a=g['accessors'][att[semantic]];v=g['bufferViews'][a['bufferView']]
        file='almond-'+semantic.lower()+'.bin';(out/file).write_bytes(almond.acc(ap['attributes'][semantic]).astype('<f4').tobytes())
        patches.append({'file':file,'offset':v['byteOffset'],'length':v['byteLength']})
    atlas_files=[f'{kind}-{style}.png' for kind in ['face','mask'] for style in ['round','almond']]
    previous=json.loads((out/'manifest.json').read_text()) if (out/'manifest.json').exists() else {}
    body_hashes={name:hashlib.sha256((source/name).read_bytes()).hexdigest() for name in ['body-round.glb','body-almond.glb']}
    cached=previous.get('atlasVersion')==2 and all(previous.get('sourceSha256',{}).get(k)==v for k,v in body_hashes.items()) and all((out/f).exists() for f in atlas_files)
    base_id=src.g['textures'][src.g['materials'][0]['pbrMetallicRoughness']['baseColorTexture']['index']]['source']
    round_image=src.image(base_id).resize((2048,2048),Image.Resampling.LANCZOS)
    if not cached:
        # Rasterized rest-position atlas fixes color selection independently of pose.
        size=2048;pos=np.zeros((size,size,3),np.float32);valid=np.zeros((size,size),bool)
        for face in F:
            q=UV[face]*(size-1);lo=np.maximum(np.floor(q.min(0)).astype(int),0);hi=np.minimum(np.ceil(q.max(0)).astype(int),size-1)
            if np.any(hi<lo):continue
            xx,yy=np.meshgrid(np.arange(lo[0],hi[0]+1),np.arange(lo[1],hi[1]+1));v0=q[1]-q[0];v1=q[2]-q[0];den=v0[0]*v1[1]-v1[0]*v0[1]
            if abs(den)<1e-8:continue
            x=xx-q[0,0];y=yy-q[0,1];u=(x*v1[1]-v1[0]*y)/den;v=(v0[0]*y-x*v0[1])/den;ok=(u>=-.001)&(v>=-.001)&(u+v<=1.001)
            region=pos[lo[1]:hi[1]+1,lo[0]:hi[0]+1];region[ok]=((1-u-v)[...,None]*P[face[0]]+u[...,None]*P[face[1]]+v[...,None]*P[face[2]])[ok]
            valid[lo[1]:hi[1]+1,lo[0]:hi[0]+1]|=ok
        pbr=src.g['materials'][0]['pbrMetallicRoughness'];base_id=src.g['textures'][pbr['baseColorTexture']['index']]['source'];round_image=src.image(base_id).resize((size,size),Image.Resampling.LANCZOS)
        def smooth(a,b,x):
            t=np.clip((x-a)/(b-a),0,1);return t*t*(3-2*t)
        for style,model in [('round',src),('almond',almond)]:
            image=model.image(base_id).resize((size,size),Image.Resampling.LANCZOS);c=np.array(image,dtype=float)/255
            mouth=(pos[:,:,2]>.06)&(abs(pos[:,:,0])<.044)&(abs(pos[:,:,1]-.741)<.019)&valid
            c[mouth]=np.array(round_image)[mouth]/255
            image=Image.fromarray(np.uint8(np.clip(c,0,1)*255));image.save(out/f'face-{style}.png')
            mx=c.max(2);sat=(mx-c.min(2))/np.maximum(mx,.001);x,y,z=pos.transpose(2,0,1)
            shorts=(y<.53)*(y>.25)*smooth(.015,.055,c[:,:,2]-c[:,:,0])*smooth(.01,.045,c[:,:,1]-c[:,:,0])
            eye=(z>.06)*(y>.766)*(y<.831)*(abs(x)>.028)*(abs(x)<.109)
            cx,cy,rx,ry=(.0524,.7929,.0168,.0160) if style=='almond' else (.0562,.7926,.0194,.022)
            iris=eye*(1-smooth(.97,1.04,np.sqrt(((abs(x)-cx)/rx)**2+((y-cy)/ry)**2)))*smooth(.24,.33,mx)*(1-smooth(.70,.83,mx))*smooth(.18,.32,sat)*smooth(.01,.05,c[:,:,0]-c[:,:,1])
            skin=smooth(.025,.07,c[:,:,0]-c[:,:,2])*smooth(.48,.65,mx)*(1-shorts)*(1-iris)*(1-eye*(1-smooth(.18,.32,sat)))
            mask=np.stack([skin,shorts,iris],2)*valid[:,:,None]
            # Texture filtering samples beyond island borders. Carry palette masks
            # into the padding too, otherwise dark skin reveals pale UV seams.
            from scipy.ndimage import distance_transform_edt
            distance,nearest=distance_transform_edt(~valid,return_indices=True)
            padding=(~valid)&(distance<=12)
            mask[padding]=mask[nearest[0][padding],nearest[1][padding]]
            Image.fromarray(np.uint8(mask*255)).save(out/f'mask-{style}.png')
    # Body textures: base color is replaced by the host after applying the chosen palette.
    normal_id=src.g['textures'][src.g['materials'][0]['normalTexture']['index']]['source']
    g['images']=[{'bufferView':b.view(png(round_image)),'mimeType':'image/png'},{'bufferView':b.view(png(src.image(normal_id).resize((1024,1024),Image.Resampling.LANCZOS))),'mimeType':'image/png'}]
    g['textures']=[{'source':0},{'source':1}]
    g['materials']=[{'name':'Body','doubleSided':True,'normalTexture':{'index':1},'pbrMetallicRoughness':{'baseColorTexture':{'index':0},'metallicFactor':0,'roughnessFactor':.82}}]
    body_node=next(i for i,n in enumerate(g['nodes']) if n.get('mesh')==0)
    world=np.eye(4) # All supplemental vertices below are converted to the approved world/bind space.
    def matrices(nodes):
        parent={c:i for i,n in enumerate(nodes) for c in n.get('children',[])};cache={}
        def get(i):
            if i in cache:return cache[i]
            n=nodes[i]
            if 'matrix' in n:M=np.array(n['matrix']).reshape(4,4).T
            else:
                from scipy.spatial.transform import Rotation
                M=np.eye(4);M[:3,:3]=Rotation.from_quat(n.get('rotation',[0,0,0,1])).as_matrix()@np.diag(n.get('scale',[1,1,1]));M[:3,3]=n.get('translation',[0,0,0])
            if i in parent:M=get(parent[i])@M
            cache[i]=M;return M
        return get
    bind_world=matrices(g['nodes'])(body_node)
    assert np.max(abs(bind_world-np.eye(4)))<1e-5, 'Unexpected body bind transform'
    torso_ids=np.flatnonzero((abs(P[:,0])<.13)&(P[:,1]>.40)&(P[:,1]<.71)&(torso.sum(1)>.5))
    donor_faces=F[np.isin(F,torso_ids).all(1)]
    tree=cKDTree(P[donor_faces].mean(1));material_roles={};hair_nodes={}
    def surface_binding(points):
        _,candidates=tree.query(points,k=32);tri=P[donor_faces[candidates]];p=points[:,None,:]
        va=tri[:,:,1]-tri[:,:,0];vb=tri[:,:,2]-tri[:,:,0];vp=p-tri[:,:,0]
        aa=(va*va).sum(2);ab=(va*vb).sum(2);bb=(vb*vb).sum(2);ap=(va*vp).sum(2);bp=(vb*vp).sum(2);den=np.maximum(aa*bb-ab*ab,1e-20)
        u=(bb*ap-ab*bp)/den;v=(aa*bp-ab*ap)/den
        coords=np.stack([1-u-v,u,v],2);projection=(tri*coords[:,:,:,None]).sum(2)
        distance=((projection-p)**2).sum(2);distance[(coords<0).any(2)]=np.inf
        for a,z in [(0,1),(1,2),(2,0)]:
            edge=tri[:,:,z]-tri[:,:,a];t=np.clip(((p-tri[:,:,a])*edge).sum(2)/np.maximum((edge*edge).sum(2),1e-20),0,1)
            candidate=tri[:,:,a]+edge*t[:,:,None];d=((candidate-p)**2).sum(2);replace=d<distance
            w=np.zeros_like(coords);w[:,:,a]=1-t;w[:,:,z]=t;coords[replace]=w[replace];distance[replace]=d[replace]
        best=distance.argmin(1);rows=np.arange(len(points));return donor_faces[candidates[rows,best]],coords[rows,best]
    for part in ['shirt','shoes','hair-swept','hair-ponytail','hair-crop','hair-bob']:
        s=GLB(shirt if part=='shirt' and shirt else source/(part+'.glb'));get_world=matrices(s.g['nodes']);parts=[]
        for ni,n in enumerate(s.g['nodes']):
            if 'mesh' not in n:continue
            M=get_world(ni)
            for p in s.g['meshes'][n['mesh']]['primitives']:
                a=p['attributes'];pp=s.acc(a['POSITION'])@M[:3,:3].T+M[:3,3];nn=s.acc(a['NORMAL'])@np.linalg.inv(M[:3,:3]);nn/=np.linalg.norm(nn,axis=1)[:,None]
                attrs={'POSITION':b.acc(pp.astype('<f4')),'NORMAL':b.acc(nn.astype('<f4'))}
                if 'TEXCOORD_0' in a:attrs['TEXCOORD_0']=b.acc(s.acc(a['TEXCOORD_0']))
                weights=np.zeros((len(pp),54))
                if part=='shirt':
                    sf=s.acc(p['indices']).reshape(-1,3)
                    # Work across UV seams, otherwise duplicate boundary vertices drift.
                    _,weld=np.unique(np.round(pp,6),axis=0,return_inverse=True)
                    edges=np.concatenate([sf[:,[0,1]],sf[:,[1,2]],sf[:,[2,0]]])
                    edges=weld[edges];edges=np.unique(np.sort(edges,axis=1),axis=0)
                    edges=np.concatenate([edges,edges[:,::-1]])
                    from scipy.sparse import coo_matrix
                    count=int(weld.max())+1
                    adj=coo_matrix((np.ones(len(edges)),(edges[:,0],edges[:,1])),shape=(count,count)).tocsr()
                    degree=np.asarray(adj.sum(1)).ravel();multiplicity=np.bincount(weld)
                    def relax(values,amount,iterations):
                        values=values.copy()
                        for _ in range(iterations):
                            mean=np.zeros((count,values.shape[1]));np.add.at(mean,weld,values);mean/=multiplicity[:,None]
                            neighbor=(adj@mean)/np.maximum(degree[:,None],1)
                            values=values*(1-amount)+(neighbor[weld])*amount
                        return values
                    ids,blend=surface_binding(pp)
                    surface=(P[ids]*blend[:,:,None]).sum(1)
                    normal=(src.acc(att['NORMAL'])[ids]*blend[:,:,None]).sum(1)
                    normal/=np.maximum(np.linalg.norm(normal,axis=1)[:,None],1e-8)
                    # Fit shoulder support only; the front/back panels remain loose.
                    shoulder=np.clip((pp[:,1]-.64)/.035,0,1)
                    pp=pp*(1-shoulder[:,None])+(surface+normal*.009)*shoulder[:,None]
                    clearance=((pp-surface)*normal).sum(1)
                    pp+=normal*(np.maximum(0,.008-clearance)*(pp[:,1]>.59))[:,None]
                    pp=relax(pp,.35,10)
                    ids,blend=surface_binding(pp)
                    attrs['POSITION']=b.acc(pp.astype('<f4'))
                    face_normal=np.cross(pp[sf[:,1]]-pp[sf[:,0]],pp[sf[:,2]]-pp[sf[:,0]])
                    smooth_normals=np.zeros((count,3))
                    for corner in range(3):np.add.at(smooth_normals,weld[sf[:,corner]],face_normal)
                    nn=smooth_normals[weld];nn/=np.maximum(np.linalg.norm(nn,axis=1)[:,None],1e-8)
                    attrs['NORMAL']=b.acc(nn.astype('<f4'))
                    for k in range(3):
                        for c in range(4):np.add.at(weights,(np.arange(len(pp)),J[ids[:,k],c]),W[ids[:,k],c]*blend[:,k])
                    weights=relax(weights,.45,16)
                else:
                    bones=np.full(len(pp),names.index('head')) if part.startswith('hair') else np.where(pp[:,0]>0,names.index('foot.L'),names.index('foot.R'))
                    weights[np.arange(len(pp)),bones]=1
                joints=np.argsort(weights,axis=1)[:,-4:];ww=np.take_along_axis(weights,joints,axis=1);ww/=ww.sum(1)[:,None]
                attrs.update(JOINTS_0=b.acc(joints.astype('<u2')),WEIGHTS_0=b.acc(ww.astype('<f4')))
                mat=copy.deepcopy(s.g['materials'][p['material']]);name=mat.get('name','')
                role='shirtColor' if part=='shirt' else ('hairColor' if part.startswith('hair') and name!='HairClip' else '')
                # These fitted garments are flat-material assets; no source texture dependencies.
                mat['pbrMetallicRoughness']={k:v for k,v in mat.get('pbrMetallicRoughness',{}).items() if not k.endswith('Texture')};mat.pop('normalTexture',None)
                mat['pbrMetallicRoughness'].update(metallicFactor=0,roughnessFactor=.75)
                mi=len(g['materials']);g['materials'].append(mat)
                if role:material_roles[str(mi)]=role
                pr={'attributes':attrs,'indices':b.acc(s.acc(p['indices'])),'material':mi}
                if part=='shirt':
                    # Hide only body triangles fully covered by this garment.
                    sf=s.acc(p['indices']).reshape(-1,3);cloth=pp[sf]
                    candidates=cKDTree(cloth.mean(1));body_ids=np.flatnonzero((P[:,1]>.433)&(P[:,1]<.64)&(abs(P[:,0])<.14)&(torso.sum(1)>.98))
                    _,near=candidates.query(P[body_ids],k=24);tri=cloth[near]
                    origin=P[body_ids,None,:];direction=src.acc(att['NORMAL'])[body_ids,None,:]
                    e1=tri[:,:,1]-tri[:,:,0];e2=tri[:,:,2]-tri[:,:,0];h=np.cross(direction,e2);det=(e1*h).sum(2)
                    inv=np.divide(1,det,out=np.zeros_like(det),where=abs(det)>1e-10);delta=origin-tri[:,:,0]
                    u=(delta*h).sum(2)*inv;q=np.cross(delta,e1);v=(direction*q).sum(2)*inv;t=(e2*q).sum(2)*inv
                    hit=(abs(det)>1e-10)&(u>.015)&(v>.015)&(u+v<.985)&(t>0)&(t<.06)
                    covered=np.zeros(len(P),bool);covered[body_ids]=hit.any(1)
                    primitive['indices']=b.acc(F[~covered[F].all(1)].astype('<u4').ravel())
                    pr['targets']=[]
                    for target in primitive['targets']:
                        delta=src.acc(target['POSITION']);transferred=(delta[ids]*blend[:,:,None]).sum(1)
                        # A loose hem must not inherit the shorts' deep-fold sculpt.
                        transferred*=np.clip((pp[:,1]-.48)/.12,.15,1)[:,None]
                        transferred=relax(transferred,.4,12)
                        pr['targets'].append({'POSITION':b.acc(transferred.astype('<f4'),sparse=True)})
                parts.append(pr)
        mesh={'name':part,'primitives':parts}
        if part=='shirt':mesh.update(weights=g['meshes'][0].get('weights',[0]*46),extras=g['meshes'][0]['extras'])
        mi=len(g['meshes']);g['meshes'].append(mesh);node=len(g['nodes']);g['nodes'].append({'name':part,'mesh':mi,'skin':0})
        g['scenes'][g.get('scene',0)]['nodes'].append(node)
        if part.startswith('hair'):hair_nodes[part.removeprefix('hair-')]=node
        if part=='shirt':
            for anim in g['animations']:
                for channel in list(anim['channels']):
                    if channel['target']=={'node':body_node,'path':'weights'}:
                        ch=copy.deepcopy(channel);ch['target']['node']=node;anim['channels'].append(ch)
    b.save(out/'template.glb')
    # Verify sparse repacking is lossless for all body data except the reversible coverage index.
    result=GLB(out/'template.glb')
    for i in range(len(src.g['accessors'])):assert np.array_equal(src.acc(i),result.acc(i)),i
    manifest={'schema':1,'pack':'trainer-avatar-1','revision':1,'template':'template.glb','bodyImage':0,'almondPatches':patches,'hairNodes':hair_nodes,'materialRoles':material_roles,'rig':'trainer-humanoid-54-v1','units':'metres','forward':'+Z','eyeHeight':.80,'actions':{'wave':'Wave','sit':'Sit','squat':'Squat','step':'Step','raise_arms':'Arms_Up'},'licenseStatus':'local-review-only','sourceSha256':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in source.glob('*.glb')}}
    manifest['atlasVersion']=2
    manifest['files']={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in out.iterdir() if f.is_file() and f.name!='manifest.json'}
    if shirt:manifest['sourceSha256']['shirt.glb']=hashlib.sha256(shirt.read_bytes()).hexdigest()
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    # Sleeveless game clothing follows the torso, with no runtime cloth or
    # copied arm/leg sculpt corrections. Preserve the approved body bind pose.
    from rigid_clothing import bind
    bind(out)
    print(json.dumps({'templateBytes':(out/'template.glb').stat().st_size,'totalBytes':sum(f.stat().st_size for f in out.iterdir() if f.is_file()),'bodyAccessorsExact':True,'correctives':46,'joints':54,'hairNodes':hair_nodes}),flush=True)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('--shirt',type=Path);a=p.parse_args();build(a.source,a.output,a.shirt)
