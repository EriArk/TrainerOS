"""Compile approved modular art into the existing shared avatar pack.

python tools/avatar/import_approved.py APPROVED_ROOT LEGACY_V3_PACK OUTPUT
The legacy pack supplies the retained round/almond textures and UV palette masks.
Requires the same numpy/scipy/Pillow authoring dependencies as build_pack.py.
"""
import argparse, copy, hashlib, json, shutil
from pathlib import Path
import numpy as np
from PIL import Image
from build_pack import GLB, Builder


def build(approved, legacy, out):
    out.mkdir(parents=True, exist_ok=True)
    original = GLB(approved/'2026-10-26/anime-hair/avatar-all-hair-options.glb')
    prepared = GLB(approved/'2026-10-26/chibi-eyes/avatar-prepared-face.glb')
    old = GLB(legacy/'template.glb')
    manifest = json.loads((legacy/'manifest.json').read_text())
    g = copy.deepcopy(original.g)
    g.update(meshes=[], materials=[], images=[], textures=[])
    b = Builder(g)
    accessors, images, materials = {}, {}, {}

    def accessor(model, index, sparse=False):
        meta = model.g['accessors'][index]
        arr = model.acc(index)
        key = (meta['type'], meta['componentType'], arr.shape, sparse, hashlib.sha256(arr.tobytes()).digest())
        if key not in accessors:
            # Assimp/Qt cannot read a zero-filled ordinary vertex attribute with
            # no bufferView. Keep sparse encoding limited to corrective deltas.
            accessors[key] = b.acc(arr, meta['type'], sparse=sparse)
        return accessors[key]

    def material(model, index):
        mat = copy.deepcopy(model.g['materials'][index])
        def visit(value):
            for key, item in value.items():
                if key.endswith('Texture') and isinstance(item, dict) and 'index' in item:
                    texture = model.g['textures'][item['index']]
                    image = model.g['images'][texture['source']]
                    view = model.g['bufferViews'][image['bufferView']]
                    start = view.get('byteOffset', 0)
                    raw = model.b[start:start+view['byteLength']]
                    digest = hashlib.sha256(raw).digest()
                    if digest not in images:
                        images[digest] = len(g['images'])
                        g['images'].append({'bufferView':b.view(raw), 'mimeType':image['mimeType']})
                        g['textures'].append({'source':images[digest]})
                    item['index'] = images[digest]
                elif isinstance(item, dict):
                    visit(item)
        visit(mat)
        key = json.dumps(mat, sort_keys=True)
        if key not in materials:
            materials[key] = len(g['materials']); g['materials'].append(mat)
        return materials[key]

    def mesh(model, index):
        result = copy.deepcopy(model.g['meshes'][index])
        for p in result['primitives']:
            p['attributes'] = {k:accessor(model, v) for k,v in p['attributes'].items()}
            p['indices'] = accessor(model, p['indices'])
            p['material'] = material(model, p['material'])
            for target in p.get('targets', []):
                for key in target:target[key] = accessor(model, target[key], sparse=True)
        index = len(g['meshes']); g['meshes'].append(result)
        return index

    for index in range(len(original.g['meshes'])):assert mesh(original,index)==index
    for skin, src in zip(g['skins'],original.g['skins']):skin['inverseBindMatrices']=accessor(original,src['inverseBindMatrices'])
    for animation, src in zip(g['animations'],original.g['animations']):
        for sampler, old_sampler in zip(animation['samplers'],src['samplers']):
            for key in ['input','output']:sampler[key]=accessor(original,old_sampler[key])
    g['extras']={'uncoveredBodyIndices':g['meshes'][0]['primitives'][0]['indices']}
    body_node=next(i for i,n in enumerate(g['nodes']) if n.get('name')=='Trainer_Body')
    prepared_mesh=mesh(prepared,0)
    # Qt merges a mesh's primitives into one vertex layout. Missing colors on
    # the textured body otherwise become black when the face has COLOR_0.
    for prim in g['meshes'][prepared_mesh]['primitives']:
        if 'COLOR_0' not in prim['attributes']:
            count=g['accessors'][prim['attributes']['POSITION']]['count']
            prim['attributes']['COLOR_0']=b.acc(np.ones((count,4),dtype='<f4'),'VEC4')
    eye_nodes={}
    for variant in json.loads((approved/'2026-10-26/chibi-eyes/eye-sets.json').read_text(encoding='utf-8'))['variants']:
        model=GLB(approved/'2026-10-26/chibi-eyes'/variant['file'])
        assert [model.g['nodes'][i]['name'] for i in model.g['skins'][0]['joints']]==[g['nodes'][i]['name'] for i in g['skins'][0]['joints']]
        ids=[]
        for node in model.g['nodes']:
            if 'mesh' not in node:continue
            new=copy.deepcopy(node);new['mesh']=mesh(model,node['mesh']);new['skin']=0
            ids.append(len(g['nodes']));g['nodes'].append(new)
            g['scenes'][g.get('scene',0)]['nodes'].append(ids[-1])
        eye_nodes[variant['id']]=ids

    # Preserve both legacy choices, with the same head transform and shorts repair.
    p=original.g['meshes'][0]['primitives'][0];oldp=old.g['meshes'][0]['primitives'][0]
    source_pos=old.acc(oldp['attributes']['POSITION']);current=original.acc(p['attributes']['POSITION'])
    t=np.clip((source_pos[:,1]-.680)/.025,0,1);scale=1-.08*t*t*(3-2*t)
    patches=[]
    for semantic in ['POSITION','NORMAL']:
        name='almond-'+semantic.lower()+'.bin'
        previous=np.frombuffer((legacy/name).read_bytes(),'<f4').reshape(-1,3)
        delta=previous-old.acc(oldp['attributes'][semantic])
        values=original.acc(p['attributes'][semantic]).copy()
        changed=np.any(delta!=0,axis=1)
        if semantic=='POSITION':values[changed]+=delta[changed]*scale[changed,None]
        else:
            values[changed]+=delta[changed]
            values[changed]/=np.linalg.norm(values[changed],axis=1)[:,None]
        raw=values.astype('<f4').tobytes();(out/name).write_bytes(raw)
        a=g['accessors'][g['meshes'][0]['primitives'][0]['attributes'][semantic]]
        assert 'bufferView' in a
        v=g['bufferViews'][a['bufferView']]
        patches.append({'file':name,'offset':v.get('byteOffset',0),'length':len(raw)})
    for name in ['face-round.png','face-almond.png','mask-round.png','mask-almond.png']:
        shutil.copy2(legacy/name,out/name)

    # One shared painted iris, with a mask that preserves pupils and highlights.
    iris_mat=next(m for m in g['materials'] if m['name']=='Original painted iris')
    iris_image=g['textures'][iris_mat['pbrMetallicRoughness']['baseColorTexture']['index']]['source']
    view=g['bufferViews'][g['images'][iris_image]['bufferView']];start=view.get('byteOffset',0)
    (out/'iris.png').write_bytes(b.b[start:start+view['byteLength']])
    rgb=np.asarray(Image.open(out/'iris.png').convert('RGB'),dtype=float)/255
    maximum=rgb.max(2);sat=(maximum-rgb.min(2))/np.maximum(maximum,.001)
    def smooth(lo,hi,x):
        t=np.clip((x-lo)/(hi-lo),0,1);return t*t*(3-2*t)
    weight=smooth(.18,.32,sat)*smooth(.01,.06,rgb[:,:,0]-rgb[:,:,1])*smooth(.20,.32,maximum)*(1-smooth(.70,.86,maximum))
    mask=np.zeros_like(rgb);mask[:,:,2]=weight
    Image.fromarray(np.uint8(mask*255)).save(out/'mask-iris.png')
    vertex_roles=[]
    for prim in g['meshes'][prepared_mesh]['primitives']:
        if 'COLOR_0' in prim['attributes'] and g['materials'][prim['material']]['name']=='Face skin transition':
            vertex_roles.append({'accessor':prim['attributes']['COLOR_0'],'role':'skinColor','base':[.87137,.55201,.37626]})
    roles={}
    for i,mat in enumerate(g['materials']):
        if mat['name'].startswith('hair-') or mat['name']=='Warm brown eyebrow':roles[str(i)]='hairColor'
    # Materials of approved meshes, independent of their authoring names.
    for node in g['nodes']:
        if node.get('name')=='shirt' or node.get('name','').startswith('hair-'):
            for prim in g['meshes'][node['mesh']]['primitives']:
                roles[str(prim['material'])]='shirtColor' if node['name']=='shirt' else 'hairColor'
    for item in g['meshes']:
        for prim in item['primitives']:
            assert all('bufferView' in g['accessors'][index] for index in prim['attributes'].values())
    b.save(out/'template.glb')
    body_material=g['materials'][g['meshes'][0]['primitives'][0]['material']]
    body_image=g['textures'][body_material['pbrMetallicRoughness']['baseColorTexture']['index']]['source']
    manifest.update(revision=4,template='template.glb',bodyImage=body_image,almondPatches=patches,
        hairNodes={n['name'][5:]:i for i,n in enumerate(g['nodes']) if n.get('name','').startswith('hair-')},
        materialRoles=roles,eyeHeight=.787,eyeNodes=eye_nodes,
        bodyNode=body_node,legacyBodyMesh=0,preparedBodyMesh=prepared_mesh,
        vertexPalette=vertex_roles,extraPaletteImages=[{'image':iris_image,'file':'iris.png','mask':'mask-iris.png'}],
        clothing={'deformation':'torso-skin','simulation':False,'fit':3},
        sourceSha256={str(path.relative_to(approved)):hashlib.sha256(path.read_bytes()).hexdigest()
            for path in [approved/'base-body/trainer-body.glb',approved/'2026-10-26/anime-hair/avatar-all-hair-options.glb',
                         approved/'2026-10-26/chibi-eyes/avatar-prepared-face.glb',
                         *sorted((approved/'2026-10-26/chibi-eyes').glob('eyes-*.glb'))]})
    names=['template.glb','face-round.png','face-almond.png','mask-round.png','mask-almond.png',
           'almond-position.bin','almond-normal.bin','iris.png','mask-iris.png']
    manifest['files']={name:hashlib.sha256((out/name).read_bytes()).hexdigest() for name in names}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
    assert (out/'template.glb').stat().st_size<60*1024*1024
    print(json.dumps({'bytes':(out/'template.glb').stat().st_size,'eyeSets':len(eye_nodes),'meshes':len(g['meshes']),'images':len(g['images']),'vertexPalette':vertex_roles}))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('approved',type=Path);parser.add_argument('legacy',type=Path);parser.add_argument('out',type=Path)
    args=parser.parse_args();build(args.approved,args.legacy,args.out)
