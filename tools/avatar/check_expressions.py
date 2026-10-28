"""Check brow clearance/area through every exported pose and intermediate weight.

python tools/avatar/check_expressions.py PATH_TO_PACK
Uses the actual GLB, including sparse deltas; no renderer or profile writes.
"""
import argparse,json,hashlib
from pathlib import Path
import numpy as np
from scipy.interpolate import LinearNDInterpolator
from build_pack import GLB


def check(pack):
    manifest=json.loads((pack/'manifest.json').read_text())
    assert hashlib.sha256((pack/'template.glb').read_bytes()).hexdigest()==manifest['files']['template.glb'],'template/manifest mismatch'
    model=GLB(pack/'template.glb');g=model.g
    body=g['meshes'][manifest['preparedBodyMesh']]['primitives'][0]
    skin=model.acc(body['attributes']['POSITION']);x,y,z=skin.T
    front=(z>.025)&(y>.747)&(y<.88)&(abs(x)<.12)
    surface=LinearNDInterpolator(skin[front,:2],skin[front,2])
    minimum=1;checked=0
    for style,nodes in manifest['eyeNodes'].items():
        brows=[g['nodes'][i] for i in nodes if g['nodes'][i]['name'].endswith('-brow')]
        assert len(brows)==1,(style,'missing brow pair')
        mesh=g['meshes'][brows[0]['mesh']];names=mesh['extras']['targetNames']
        for prim in mesh['primitives']:
            base=model.acc(prim['attributes']['POSITION']);faces=model.acc(prim['indices']).reshape(-1,3)
            deltas={name:model.acc(target['POSITION']) for name,target in zip(names,prim['targets'])}
            for name in ['eyes_closed','eyes_squeezed','eyes_hearts','eyes_shocked']:
                assert np.max(abs(deltas[name]))<1e-7,(style,name,'eye replacement moves brows')
            for pose,weights in g['extras']['trainerFace']['expressions'].items():
                delta=sum((deltas.get(name,np.zeros_like(base))*weight for name,weight in weights.items()),np.zeros_like(base))
                for strength in [0,.25,.5,.75,1]:
                    p=base+strength*delta;clearance=p[:,2]-surface(p[:,:2])
                    assert np.isfinite(p).all() and np.isfinite(clearance).all(),(style,pose,'invalid geometry')
                    assert clearance.min()>.0005,(style,pose,strength,'brow buried in skin',clearance.min())
                    area=np.linalg.norm(np.cross(p[faces[:,1]]-p[faces[:,0]],p[faces[:,2]]-p[faces[:,0]]),axis=1).sum()/2
                    assert area>.00005,(style,pose,'collapsed brow')
                    minimum=min(minimum,float(clearance.min()));checked+=1
    print(json.dumps({'eyeSets':len(manifest['eyeNodes']),'browPosesAndWeights':checked,'minimumSkinClearanceMetres':minimum,'passed':True}))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('pack',type=Path)
    check(parser.parse_args().pack)
