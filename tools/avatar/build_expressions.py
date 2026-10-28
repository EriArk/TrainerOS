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
        'cat_smile': (.025,.739,0,.00085,.0015),
    }
    mouth_names = ['mouth_'+name for name in mouth_specs if name!='rest']
    def mouth_point(u,v,spec):
        width,cy,curve,height,tilt=spec
        cat = .030*(u*u-.38)**2-.0025 if spec==mouth_specs['cat_smile'] else 0
        # A modest uniform enlargement keeps every pose and its teeth/tongue
        # readable at ordinary gameplay distance, without moving its face anchor.
        return [1.15*width*u,.740+1.15*(cy+curve*u*u+height*v+tilt*u+cat-.740),0]
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
            opened = spec in [mouth_specs['open'],mouth_specs['laugh'],mouth_specs['tense']] if label=='Teeth' else spec==mouth_specs['laugh']
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

    # The two original IDs now use independent eye surfaces too. A painted eye
    # embedded in skin cannot close or become a symbol without stretching skin.
    # Retain IDs/recipes, body correctives and source assets, reuse our own fitted
    # eye construction with round/almond proportions instead of painting a cover.
    for style in ['round','almond']:
        ids=[]
        for source_id in manifest['eyeNodes']['innocent-'+('base' if style=='round' else 'feminine')]:
            node=copy.deepcopy(g['nodes'][source_id]);mesh=copy.deepcopy(g['meshes'][node['mesh']])
            node['name']='eyes-'+style+'-'+node['name'].rsplit('-',1)[1]
            node['mesh']=len(g['meshes']);g['meshes'].append(mesh)
            ids.append(len(g['nodes']));g['nodes'].append(node)
            g['scenes'][g.get('scene',0)]['nodes'].append(ids[-1])
        manifest['eyeNodes'][style]=ids
        manifest['eyeBodyMeshes'][style]=manifest['preparedBodyMesh']

    # Separate named facial morphs do not touch the body's 46 corrective tracks.
    symbolic=['eyes_closed','eyes_squeezed','eyes_hearts','eyes_shocked']
    face_names=['brow_raise','brow_angry','brow_worry','eyes_soft','eyes_wide','brow_asymmetric','eyes_half']+symbolic
    def read_acc(index):
        if index<len(model.g['accessors']):return model.acc(index)
        a=g['accessors'][index];v=g['bufferViews'][a['bufferView']]
        from build_pack import DT,NC
        return np.frombuffer(b.b[v['byteOffset']:v['byteOffset']+v['byteLength']],DT[a['componentType']]).reshape(-1,NC[a['type']]).copy()
    face_surface=LinearNDInterpolator(pos[(z>.025)&(y>.747)&(y<.88)&(abs(x)<.12),:2],pos[(z>.025)&(y>.747)&(y<.88)&(abs(x)<.12),2])
    def fit_face(points,offset):
        result=points.copy();depth=face_surface(result[:,:2])
        assert np.isfinite(depth).all(), 'Eye left the supported face'
        result[:,2]=depth+offset
        return result.astype('<f4')
    for style,nodes in manifest['eyeNodes'].items():
        white_node=next(g['nodes'][i] for i in nodes if g['nodes'][i]['name'].endswith('-white'))
        # The approved white is cut around the original iris. Fill that opening
        # with the same fitted topology before reducing the colored component.
        iris_node=next(g['nodes'][i] for i in nodes if g['nodes'][i]['name'].endswith('-iris'))
        white_mesh=g['meshes'][white_node['mesh']]
        fill=copy.deepcopy(g['meshes'][iris_node['mesh']]['primitives'][0])
        fill['material']=white_mesh['primitives'][0]['material']
        fill['attributes'].pop('TEXCOORD_0',None)
        white_mesh['primitives'].append(fill)
        white=read_acc(g['meshes'][white_node['mesh']]['primitives'][0]['attributes']['POSITION'])
        columns=np.unique(abs(white[:,0]));upper=np.array([white[np.isclose(abs(white[:,0]),v,atol=1e-7),1].max() for v in columns])
        lower=np.array([white[np.isclose(abs(white[:,0]),v,atol=1e-7),1].min() for v in columns])
        center=(columns[0]+columns[-1])/2
        for node_id in nodes:
            node=g['nodes'][node_id];mesh=g['meshes'][node['mesh']]
            is_brow=node['name'].endswith('-brow');is_iris=node['name'].endswith('-iris')
            is_rim=node['name'].endswith('-rim')
            mesh['weights']=[0]*len(face_names);mesh['extras']={'targetNames':face_names}
            for prim in mesh['primitives']:
                p=read_acc(prim['attributes']['POSITION']);n=read_acc(prim['attributes']['NORMAL']);f=read_acc(prim['indices']).reshape(-1,3)
                if is_iris:
                    # Scale the whole painted component (pupil and glints included)
                    # over the existing white, keeping the original iris artwork.
                    p[:,0]=np.sign(p[:,0])*center+(p[:,0]-np.sign(p[:,0])*center)*.78
                    p[:,1]=.786+(p[:,1]-.786)*.78
                    p=fit_face(p,.0011)
                if style in ['round','almond'] and not is_brow:
                    p[:,1]=.786+(p[:,1]-.786)*(.96 if style=='round' else .82)
                if not is_brow:
                    p=fit_face(p,.0018 if is_rim else (.0011 if is_iris else .0006))
                else:
                    p=fit_face(p,.002)
                    # Hair-color material remains customizable. A vertex tint
                    # makes brows darker than matching hair/skin, even after the
                    # resolver replaces the material's base color.
                    tint=np.full((len(p),4),.28,dtype='<f4');tint[:,3]=1
                    prim['attributes']['COLOR_0']=b.acc(tint,'VEC4')
                prim['attributes']['POSITION']=b.acc(p)
                prim['attributes']['NORMAL']=b.acc(normals(p,f))
                xx,yy,zz=p.T;cx=.055;cy=.788
                brow=np.ones(len(p)) if is_brow else np.zeros(len(p))
                eye=np.ones(len(p))*(not is_brow)
                for control in face_names:
                    target=p.copy()
                    if control=='brow_raise':target[:,1]+=.006*brow
                    if control=='brow_angry':target[:,1]+=(.24*(abs(xx)-cx)-.002)*brow
                    if control=='brow_worry':target[:,1]+=(-.24*(abs(xx)-cx)+.003)*brow
                    if control=='brow_asymmetric':target[:,1]+=(np.sign(xx)*.004+.10*(abs(xx)-cx))*brow
                    if control=='eyes_soft':target[:,1]-=(yy-cy)*.18*eye
                    if control=='eyes_wide':target[:,1]+=(yy-cy)*.12*eye
                    if control=='eyes_half' and not is_brow:
                        lid=.785+.055*(abs(xx)-center)
                        if is_iris:target[:,1]=np.minimum(yy,lid-.0002)
                        else:
                            top=np.interp(abs(xx),columns,upper);bottom=np.interp(abs(xx),columns,lower)
                            t=np.clip((yy-bottom)/np.maximum(top-bottom,.001),0,1.3)
                            target[:,1]=yy+(lid-top)*t
                    if control in symbolic and not is_brow:
                        target[:,0]=np.sign(xx)*center
                        target[:,1]=.786
                    if not is_brow and control in ['eyes_soft','eyes_wide','eyes_half',*symbolic]:
                        target=fit_face(target,.0018 if is_rim else (.0011 if is_iris else .0006))
                    if is_brow:
                        target=fit_face(target,.002)
                    prim.setdefault('targets',[]).append({'POSITION':b.acc((target-p).astype('<f4'),sparse=True),'NORMAL':b.acc((normals(target,f)-normals(p,f)).astype('<f4'),sparse=True)})

    # Explicit cartoon poses grow from zero-area geometry. No skin cover, texture
    # swaps or per-game meshes; the ordinary eye surfaces collapse in those poses.
    dark=next(i for i,m in enumerate(g['materials']) if m['name']=='Soft warm eyelid')
    white_mat=next(i for i,m in enumerate(g['materials']) if m['name']=='Eye white')
    heart_mat=len(g['materials']);g['materials'].append({'name':'Reaction hearts','doubleSided':True,'pbrMetallicRoughness':{'baseColorFactor':[.65,.035,.09,1],'metallicFactor':0,'roughnessFactor':1}})
    icons=[]
    def icon(points,faces,control,material,offset=.002):
        target=fit_face(np.array(points,dtype='<f4'),offset);base=target.copy()
        base[:,0]=np.sign(base[:,0])*.053;base[:,1]=.786;base=fit_face(base,offset)
        faces=np.array(faces,dtype='<u4');normal=normals(target,faces)
        joints=np.zeros((len(base),4),dtype='<u2');joints[:,0]=head
        weights=np.zeros((len(base),4),dtype='<f4');weights[:,0]=1
        prim={'attributes':{'POSITION':b.acc(base),'NORMAL':b.acc(normal),'JOINTS_0':b.acc(joints),'WEIGHTS_0':b.acc(weights)},'indices':b.acc(faces.ravel()),'material':material,'targets':[]}
        for name in face_names:
            prim['targets'].append({'POSITION':b.acc(target-base if name==control else np.zeros_like(base),sparse=True),'NORMAL':b.acc(np.zeros_like(base),sparse=True)})
        icons.append(prim)
    for side in [-1,1]:
        for control in ['eyes_closed','eyes_squeezed']:
            u=np.linspace(-1,1,49)
            height=.011*(1-u*u) if control=='eyes_closed' else .012*abs(u)
            if control=='eyes_closed':
                pts=[[side*(.053+.025*t),.779+h+edge*.0014*np.sqrt(max(0,1-t*t)),0] for t,h in zip(u,height) for edge in [-1,1]]
            else:
                # > < point towards the nose, like the reference's tight squint.
                pts=[[side*(.043+.020*abs(t)),.786+.014*t+edge*.0013,0] for t in u for edge in [-1,1]]
            faces=[[2*i,2*i+1,2*i+2] for i in range(len(u)-1)]+[[2*i+1,2*i+3,2*i+2] for i in range(len(u)-1)]
            icon(pts,faces,control,dark)
        t=np.linspace(0,2*np.pi,65)[:-1]
        heart=np.c_[side*.053+.00125*16*np.sin(t)**3,.788+.00125*(13*np.cos(t)-5*np.cos(2*t)-2*np.cos(3*t)-np.cos(4*t))]
        points,faces=fill_polygon(heart);icon(np.c_[points,np.zeros(len(points))],faces,'eyes_hearts',heart_mat)
        for rx,ry,material,depth in [(.022,.025,dark,.002),(.020,.023,white_mat,.0024),(.004,.005,dark,.0028)]:
            boundary=np.c_[side*.053+rx*np.cos(t),.786+ry*np.sin(t)]
            points,faces=fill_polygon(boundary);icon(np.c_[points,np.zeros(len(points))],faces,'eyes_shocked',material,depth)
    mesh_id=len(g['meshes']);g['meshes'].append({'name':'Cartoon eye reactions','primitives':icons,'weights':[0]*len(face_names),'extras':{'targetNames':face_names}})
    for style in manifest['eyeNodes']:
        node_id=len(g['nodes']);g['nodes'].append({'name':'eyes-'+style+'-reactions','mesh':mesh_id,'skin':0})
        g['scenes'][g.get('scene',0)]['nodes'].append(node_id);manifest['eyeNodes'][style].append(node_id)

    presets={
        'neutral':{},
        'happy':{'mouth_smile':1,'eyes_soft':.55,'brow_raise':.35},
        'sad':{'mouth_frown':1,'brow_worry':.85,'eyes_soft':.45},
        'angry':{'mouth_flat':1,'brow_angry':1,'eyes_soft':.45},
        'surprised':{'mouth_open':1,'brow_raise':1,'eyes_wide':1},
        'smug':{'mouth_smirk':1,'brow_raise':.2,'eyes_half':.8},
        'worried':{'mouth_tense':.7,'brow_worry':1,'eyes_wide':.25},
        'tired':{'mouth_flat':1,'eyes_half':.85,'brow_worry':.25},
        'laughing':{'mouth_laugh':1,'eyes_closed':1,'brow_raise':.5},
        'cat_smug':{'mouth_cat_smile':1,'eyes_half':1,'brow_asymmetric':1},
        'content':{'mouth_smile':1,'eyes_closed':1,'brow_raise':.2},
        'squeezed':{'mouth_laugh':1,'eyes_squeezed':1,'brow_worry':.5},
        'love':{'mouth_laugh':1,'eyes_hearts':1,'brow_raise':.4},
        'shocked':{'mouth_open':1,'eyes_shocked':1,'brow_raise':1},
    }
    g.setdefault('extras',{})['trainerFace']={'schema':1,'controls':mouth_names+face_names,'mouths':list(mouth_specs),'expressions':presets,
        'reactions':{'celebrate':{'expression':'laughing','hold':1.2},'disappointed':{'expression':'sad','hold':1.4},'startled':{'expression':'surprised','hold':.7},'hurt':{'expression':'worried','hold':.65}},'speech':False}
    out.mkdir(parents=True,exist_ok=True)
    for path in source.iterdir():
        if path.is_file():shutil.copy2(path,out/path.name)
    b.save(out/'template.glb')
    manifest.update(revision=6,facialRig={'schema':1,'metadata':'extras.trainerFace','speech':False})
    manifest['files']['template.glb']=hashlib.sha256((out/'template.glb').read_bytes()).hexdigest()
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'revision':6,'bytes':(out/'template.glb').stat().st_size,'mouths':list(mouth_specs),'expressions':list(presets),'eyeSets':len(manifest['eyeNodes'])}))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('source',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();build(args.source,args.output)
