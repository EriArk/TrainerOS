"""Reversible headwear fits in the shared avatar's Y-up bind coordinates.

Produce portable delta patches; never overwrite approved hair or body assets.
python tools/avatar/golf_hair_fit.py --cap-avatar resolved.glb --visor-avatar resolved.glb --output collection
"""
import argparse,hashlib,json
from pathlib import Path
import numpy as np

def fit_positions(vertices,headwear):
    v=np.asarray(vertices,dtype=float).copy();y=v[:,1].copy()
    if headwear=='golf-cap':
        # Compress the crown vertically, blend back to untouched fringe ends.
        amount=np.clip((y-.840)/.055,0,1)
        v[:,1]-=np.maximum(y-.880,0)*.17*amount
        height=np.clip((v[:,1]-.901)/.099,0,.999)
        radius=np.maximum((1-height**2)**.375,.05)
        rx=(.147-.004)*radius;rz=(.174-.004)*radius
        influence=np.clip((y-.840)/.044,0,1)
    elif headwear=='golf-visor':
        rx=np.full(len(v),.138-.003);rz=np.full(len(v),.157-.003)
        # The band presses a narrow belt; loose tips and crown keep their lift.
        influence=np.minimum(np.clip((y-.845)/.039,0,1),np.clip((.940-y)/.039,0,1))
    else:raise ValueError(headwear)
    radial=np.sqrt((v[:,0]/rx)**2+(v[:,2]/rz)**2)
    scale=1-(1-1/np.maximum(radial,1))*influence
    v[:,0]*=scale;v[:,2]*=scale
    return v

def main():
    from build_pack import GLB
    from build_golf_clothes import normals
    parser=argparse.ArgumentParser();parser.add_argument('--cap-avatar',type=Path,required=True);parser.add_argument('--visor-avatar',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    folder=args.output/'hair-fit';folder.mkdir(parents=True,exist_ok=True)
    fits=[]
    for source,hat,hair in [(args.cap_avatar,'golf-cap','hair-crop'),(args.visor_avatar,'golf-visor','hair-bob')]:
        src=GLB(source);mesh=next(m for m in src.g['meshes'] if m.get('name')==hair)
        primitives=[]
        for i,primitive in enumerate(mesh['primitives']):
            p=src.acc(primitive['attributes']['POSITION']);n=src.acc(primitive['attributes']['NORMAL']);f=src.acc(primitive['indices']).reshape(-1,3)
            fitted=fit_positions(p,hat);delta=(fitted-p).astype('<f4');dn=(normals(fitted,f)-n).astype('<f4')
            stem=hat+'-'+hair+'-'+str(i)
            record={'primitive':i,'vertices':len(p),'positionSha256':hashlib.sha256(p.astype('<f4').tobytes()).hexdigest()}
            for label,data in [('positionDelta',delta),('normalDelta',dn)]:
                file=stem+'-'+label+'.bin';(folder/file).write_bytes(data.tobytes());record[label]={'file':file,'sha256':hashlib.sha256(data.tobytes()).hexdigest()}
            assert np.isfinite(fitted).all() and np.linalg.norm(delta,axis=1).max()<.08
            record['movedVertices']=int(np.count_nonzero(np.linalg.norm(delta,axis=1)>1e-6));primitives.append(record)
        fits.append({'headwear':hat,'hairMesh':hair,'primitives':primitives})
    (folder/'manifest.json').write_text(json.dumps({'schema':1,'space':'Y-up shared bind pose, metres','encoding':'little-endian float32 VEC3','operation':'original + delta; restore original on unequip; do not accumulate','fits':fits,'runtimeInstalled':False},indent=2)+'\n')
    print('Exported reversible crop/cap and bob/visor fits')

if __name__=='__main__':main()
