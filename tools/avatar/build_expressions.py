"""Add the shared transient facial rig to an unchanged revision-4 input pack.

python tools/avatar/build_expressions.py REVISION_4_PACK OUTPUT
No profile migration, raster repainting or game-owned character geometry.
"""
import argparse, copy, hashlib, json, shutil
from pathlib import Path
import numpy as np
from scipy.interpolate import LinearNDInterpolator
from build_pack import GLB, Builder


def smooth(lo, hi, x):
    t = np.clip((x-lo)/(hi-lo), 0, 1)
    return t*t*(3-2*t)


def linear(c):
    return np.where(c <= .04045, c/12.92, ((c+.055)/1.055)**2.4)


def normals(pos, faces):
    out = np.zeros_like(pos)
    n = np.cross(pos[faces[:, 1]]-pos[faces[:, 0]], pos[faces[:, 2]]-pos[faces[:, 0]])
    for k in range(3): np.add.at(out, faces[:, k], n)
    length = np.linalg.norm(out, axis=1)
    out[length > 1e-10] /= length[length > 1e-10, None]
    out[length <= 1e-10] = [0, 0, 1]
    return out


def build(source: Path, out: Path):
    assert source.resolve() != out.resolve(), 'Use a separate output directory'
    manifest = json.loads((source/'manifest.json').read_text())
    assert manifest['revision'] == 4, 'Requires the retained approved revision 4'
    model = GLB(source/'template.glb')
    g = copy.deepcopy(model.g)
    # Append only: the original accessor IDs and legacy patch byte offsets survive.
    b = Builder.__new__(Builder); b.g = g; b.b = bytearray(model.b)
    body = g['meshes'][manifest['preparedBodyMesh']]
    original = model.g['meshes'][manifest['legacyBodyMesh']]['primitives'][0]
    pos = model.acc(body['primitives'][0]['attributes']['POSITION'])
    uv = model.acc(original['attributes']['TEXCOORD_0'])
    texture = np.asarray(model.image(manifest['bodyImage']), dtype=float)/255
    xy = np.clip(np.rint(uv*(np.array(texture.shape[1::-1])-1)), 0, texture.shape[0]-1).astype(int)
    colors = linear(texture[xy[:, 1], xy[:, 0]])

    eye_faces=model.acc(model.g['meshes'][manifest['preparedBodyMesh']]['primitives'][1]['indices']).reshape(-1,3)

    # Replace the baked smile with skin-colored geometry. Boundary vertices retain
    # the sampled original color; the shared skin palette still recolors this part.
    x, y, z = pos.T
    distance = np.sqrt((x/.034)**2+((y-.740)/.014)**2)
    region = (distance < 1.65) & (z > .075)
    samples = region & (distance > .85) & (colors[:, 0] > .45)
    basis = lambda x,y: np.stack([x*0+1, x, y-.740, x*x, x*(y-.740), (y-.740)**2], axis=-1)
    fitted = basis(x,y) @ np.linalg.lstsq(basis(x[samples],y[samples]), colors[samples], rcond=None)[0]
    blend = (1-smooth(.85,1.45,distance))*(z>.075)
    patch_colors = np.c_[np.clip(colors*(1-blend[:,None])+fitted*blend[:,None],0,1),np.ones(len(pos))].astype('<f4')
    patch_faces = []
    for prim in body['primitives']:
        faces = model.acc(prim['indices']).reshape(-1,3)
        selected = region[faces].all(1) & (distance[faces].min(1)<1.45)
        patch_faces.extend(faces[selected])
        prim['indices'] = b.acc(faces[~selected].astype('<u4').ravel())
    patch = copy.deepcopy(body['primitives'][0])
    patch['material'] = 8
    patch['attributes']['COLOR_0'] = b.acc(patch_colors,'VEC4')
    patch['indices'] = b.acc(np.asarray(patch_faces,dtype='<u4').ravel())
    body['primitives'].append(patch)
    manifest['vertexPalette'].append({'accessor':patch['attributes']['COLOR_0'],'role':'skinColor','base':[.87137,.55201,.37626]})

    # The legacy face keeps its own original surface. Remove exactly the eye
    # triangles that become moving islands, rather than covering a second skin
    # surface with them. This avoids z-fighting and changes to approved face shape.
    legacy=g['meshes'][manifest['legacyBodyMesh']]
    removed={tuple(sorted(face)) for face in [*eye_faces,*patch_faces]}
    faces=model.acc(original['indices']).reshape(-1,3)
    remaining=np.array([face for face in faces if tuple(sorted(face)) not in removed],dtype='<u4')
    base=copy.deepcopy(original);base['indices']=b.acc(remaining.ravel())
    # Qt/Assimp merges primitive attributes within a mesh: all surfaces need
    # COLOR_0 once the mouth skin patch has it, including textured white surfaces.
    base['attributes']['COLOR_0']=body['primitives'][0]['attributes']['COLOR_0']
    skin=copy.deepcopy(patch)
    for key in ['POSITION','NORMAL']:skin['attributes'][key]=original['attributes'][key]
    legacy['primitives']=[base,skin]
    manifest['eyeBodyMeshes']={style:manifest['legacyBodyMesh'] for style in ['round','almond']}
    mouth_faces={tuple(sorted(face)) for face in patch_faces}
    eye_faces=np.array([face for face in eye_faces if tuple(sorted(face)) not in mouth_faces],dtype='<u4')

    front = (abs(x)<.065)&(y>.700)&(y<.775)&(z>.075)
    surface = LinearNDInterpolator(pos[front,:2], pos[front,2])
    def project(points, offset=.0012):
        points = np.asarray(points,dtype='<f4')
        depth = surface(points[:,:2])
        assert np.isfinite(depth).all(), 'Facial component left the supported surface'
        points[:,2] = depth+offset
        return points

    # Minimal chibi mouths: a warm contour, a small upper tooth strip and tongue.
    # All poses use the same topology, allowing smooth convex interpolation.
    mouth_specs = {
        'rest': (.023,.738,.006,.00065,0),
        'flat': (.018,.740,0,.0006,0),
        'smile': (.026,.736,.010,.0008,0),
        'frown': (.018,.744,-.007,.00065,0),
        'smirk': (.021,.740,.003,.00065,.004),
        'open': (.010,.738,0,.010,0),
        'laugh': (.023,.736,.006,.010,0),
        'tense': (.021,.740,0,.0035,0),
    }
    mouth_names = ['mouth_'+name for name in mouth_specs if name!='rest']
    def mouth_point(u,v,spec):
        width,cy,curve,height,tilt=spec
        return [width*u,cy+curve*u*u+height*v+tilt*u,0]
    ring = np.linspace(0,2*np.pi,65)[:-1]
    disc = np.c_[np.cos(ring),np.sin(ring)]
    # Insets are clipped to the disk analytically, and collapse inside the line
    # for closed poses. Their depth is behind the dark contour in closed poses.
    teeth = np.array([[u,v] for u,v in [(-.72,.55),(-.45,.88),(0,1),(.45,.88),(.72,.55),(.55,.28),(0,.37),(-.55,.28)]])
    tongue = np.c_[.55*np.cos(ring),-.63+.22*np.sin(ring)]
    def fill_polygon(boundary):
        # Rings follow the curved face instead of cutting through it as a flat fan.
        center=boundary.mean(0);count=len(boundary);rings=8
        points=np.array([center,*[center+(p-center)*r/rings for r in range(1,rings+1) for p in boundary]])
        triangles=[[0,1+i,1+(i+1)%count] for i in range(count)]
        for r in range(rings-1):
            for i in range(count):
                a=1+r*count+i;bb=1+r*count+(i+1)%count;c=1+(r+1)*count+i;d=1+(r+1)*count+(i+1)%count
                triangles.extend([[a,c,d],[a,d,bb]])
        return points,np.array(triangles,dtype='<u4')
    mouth_primitives=[]
    head=next(i for i,n in enumerate(g['skins'][0]['joints']) if g['nodes'][n]['name']=='head')
    for label,boundary,color in [('Contour',disc,[.095,.028,.022,1]),('Teeth',teeth,[.94,.91,.85,1]),('Tongue',tongue,[.55,.14,.18,1])]:
        points,triangles=fill_polygon(boundary)
        material=len(g['materials']);g['materials'].append({'name':'Mouth '+label,'doubleSided':True,'pbrMetallicRoughness':{'baseColorFactor':color,'metallicFactor':0,'roughnessFactor':1}})
        def shape(spec):
            opened = spec in [mouth_specs['laugh'],mouth_specs['tense']] if label=='Teeth' else spec==mouth_specs['laugh']
            offset=.0015 if label=='Contour' else (.0023 if opened else .0008)
            visible_points=points if label=='Contour' or opened else points*.0001
            return project([mouth_point(u,v,spec) for u,v in visible_points],offset)
        base=shape(mouth_specs['rest']);n=normals(base,triangles)
        joints=np.zeros((len(base),4),dtype='<u2');joints[:,0]=head
        weights=np.zeros((len(base),4),dtype='<f4');weights[:,0]=1
        prim={'attributes':{'POSITION':b.acc(base),'NORMAL':b.acc(n),'JOINTS_0':b.acc(joints),'WEIGHTS_0':b.acc(weights)},'indices':b.acc(triangles.ravel()),'material':material,'targets':[]}
        for name,spec in mouth_specs.items():
            if name=='rest':continue
            target=shape(spec)
            prim['targets'].append({'POSITION':b.acc(target-base),'NORMAL':b.acc(normals(target,triangles)-n)})
        mouth_primitives.append(prim)
    mesh_id=len(g['meshes']);g['meshes'].append({'name':'Trainer mouth','primitives':mouth_primitives,'weights':[0]*len(mouth_names),'extras':{'targetNames':mouth_names}})
    node_id=len(g['nodes']);g['nodes'].append({'name':'Face_Mouth','mesh':mesh_id,'skin':0});g['scenes'][g.get('scene',0)]['nodes'].append(node_id)

    # Retain legacy painted eyes as fitted islands stitched to the original face.
    # The twelve modular sets retain the approved clean face, unchanged.
    for style in ['round','almond']:
        ids,indices=np.unique(eye_faces.ravel(),return_inverse=True)
        p=copy.deepcopy(original);p.pop('targets',None)
        values={key:model.acc(value)[ids] for key,value in original['attributes'].items()}
        if style=='almond':
            for key in ['POSITION','NORMAL']:values[key]=np.frombuffer((source/('almond-'+key.lower()+'.bin')).read_bytes(),'<f4').reshape(-1,3)[ids].copy()
            material=copy.deepcopy(g['materials'][original['material']]);material['name']='Legacy almond face'
            image=len(g['images']);g['images'].append({'bufferView':b.view((source/'face-almond.png').read_bytes()),'mimeType':'image/png'})
            tex=len(g['textures']);g['textures'].append({'source':image})
            material['pbrMetallicRoughness']['baseColorTexture']['index']=tex
            p['material']=len(g['materials']);g['materials'].append(material)
            manifest['extraPaletteImages'].append({'image':image,'file':'face-almond.png','mask':'mask-almond.png'})
        p['attributes']={key:b.acc(value,model.g['accessors'][original['attributes'][key]]['type']) for key,value in values.items()}
        p['indices']=b.acc(indices.astype('<u4'))
        mesh_id=len(g['meshes']);g['meshes'].append({'name':'eyes-'+style+'-surface','primitives':[p]})
        node_id=len(g['nodes']);g['nodes'].append({'name':'eyes-'+style+'-surface','mesh':mesh_id,'skin':0});g['scenes'][g.get('scene',0)]['nodes'].append(node_id)
        manifest['eyeNodes'][style]=[node_id]

    # Separate named facial morphs do not touch the body's 46 corrective tracks.
    face_names=['brow_raise','brow_angry','brow_worry','eyes_soft','eyes_wide']
    def read_acc(index):
        if index<len(model.g['accessors']):return model.acc(index)
        a=g['accessors'][index];v=g['bufferViews'][a['bufferView']]
        from build_pack import DT,NC
        return np.frombuffer(b.b[v['byteOffset']:v['byteOffset']+v['byteLength']],DT[a['componentType']]).reshape(-1,NC[a['type']]).copy()
    for style,nodes in manifest['eyeNodes'].items():
        for node_id in nodes:
            node=g['nodes'][node_id];mesh=g['meshes'][node['mesh']]
            is_brow=node['name'].endswith('-brow');legacy=style in ['round','almond']
            mesh['weights']=[0]*len(face_names);mesh['extras']={'targetNames':face_names}
            for prim in mesh['primitives']:
                p=read_acc(prim['attributes']['POSITION']);n=read_acc(prim['attributes']['NORMAL']);f=read_acc(prim['indices']).reshape(-1,3)
                xx,yy,zz=p.T;cx=.055;cy=.788
                brow=np.ones(len(p)) if is_brow else (smooth(.818,.832,yy)*(1-smooth(.851,.868,yy)) if legacy else np.zeros(len(p)))
                eye=(1-smooth(.811,.823,yy))*smooth(.751,.767,yy) if legacy else np.ones(len(p))*(not is_brow)
                if legacy:
                    island=smooth(.020,.033,abs(xx))*(1-smooth(.090,.110,abs(xx)))
                    edges=np.sort(np.concatenate([f[:,[0,1]],f[:,[1,2]],f[:,[2,0]]]),axis=1)
                    edges,counts=np.unique(edges,axis=0,return_counts=True)
                    boundary=np.unique(edges[counts==1])
                    from scipy.spatial import cKDTree
                    distance=cKDTree(p[boundary]).query(p)[0]
                    island*=smooth(0,.004,distance)
                    brow*=island;eye*=island
                for control in face_names:
                    target=p.copy()
                    if control=='brow_raise':target[:,1]+=.006*brow
                    if control=='brow_angry':target[:,1]+=(.24*(abs(xx)-cx)-.002)*brow
                    if control=='brow_worry':target[:,1]+=(-.24*(abs(xx)-cx)+.003)*brow
                    # Small changes preserve each chosen eye's character and iris.
                    if control=='eyes_soft':target[:,1]-=(yy-cy)*.18*eye
                    if control=='eyes_wide':target[:,1]+=(yy-cy)*.12*eye
                    prim.setdefault('targets',[]).append({'POSITION':b.acc((target-p).astype('<f4')),'NORMAL':b.acc((normals(target,f)-normals(p,f)).astype('<f4'))})

    presets={
        'neutral':{},
        'happy':{'mouth_smile':1,'eyes_soft':.55,'brow_raise':.35},
        'sad':{'mouth_frown':1,'brow_worry':.85,'eyes_soft':.45},
        'angry':{'mouth_flat':1,'brow_angry':1,'eyes_soft':.45},
        'surprised':{'mouth_open':1,'brow_raise':1,'eyes_wide':1},
        'smug':{'mouth_smirk':1,'brow_raise':.2,'eyes_soft':.7},
        'worried':{'mouth_tense':.7,'brow_worry':1,'eyes_wide':.25},
        'tired':{'mouth_flat':1,'eyes_soft':1,'brow_worry':.25},
        'laughing':{'mouth_laugh':1,'eyes_soft':1,'brow_raise':.5},
    }
    g.setdefault('extras',{})['trainerFace']={'schema':1,'controls':mouth_names+face_names,'mouths':list(mouth_specs),'expressions':presets,
        'reactions':{'celebrate':{'expression':'laughing','hold':1.2},'disappointed':{'expression':'sad','hold':1.4},'startled':{'expression':'surprised','hold':.7},'hurt':{'expression':'worried','hold':.65}},'speech':False}
    out.mkdir(parents=True,exist_ok=True)
    for path in source.iterdir():
        if path.is_file():shutil.copy2(path,out/path.name)
    b.save(out/'template.glb')
    manifest.update(revision=5,facialRig={'schema':1,'metadata':'extras.trainerFace','speech':False})
    manifest['files']['template.glb']=hashlib.sha256((out/'template.glb').read_bytes()).hexdigest()
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'revision':5,'bytes':(out/'template.glb').stat().st_size,'mouths':list(mouth_specs),'expressions':list(presets),'eyeSets':len(manifest['eyeNodes'])}))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('source',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();build(args.source,args.output)
