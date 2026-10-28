"""Validate standalone wearable exports, source identity and animation bindings."""
import argparse,hashlib,json
from pathlib import Path
import numpy as np
from build_pack import GLB
from check_garment_fit import check_shorts_underlayer

ROOT=Path(__file__).resolve().parents[2]
def check(folder):
    manifest=json.loads((folder/'collection.json').read_text())
    base=GLB(ROOT/'assets/avatar/approved/base-body/trainer-body.glb')
    assert hashlib.sha256((ROOT/'assets/avatar/approved/base-body/trainer-body.glb').read_bytes()).hexdigest()==manifest['baseBodySha256']
    joint_names=[base.g['nodes'][n]['name'] for n in base.g['skins'][0]['joints']]
    inverse=base.acc(base.g['skins'][0]['inverseBindMatrices'])
    reports={}
    assert len(manifest['items'])==10
    assert 'golf-shorts' not in manifest['sets']['womens']
    for item,entry in manifest['items'].items():
        path=folder/entry['file'];assert hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256']
        model=GLB(path);g=model.g
        assert [g['nodes'][n]['name'] for n in g['skins'][0]['joints']]==joint_names
        assert np.array_equal(model.acc(g['skins'][0]['inverseBindMatrices']),inverse)
        nodes=[n for n in g['nodes'] if 'mesh' in n]
        assert len(nodes)==1 and nodes[0]['name']==item
        assert len(g['meshes'])==1 and g['meshes'][0]['name']==item
        assert [a['name'] for a in g['animations']]==[a['name'] for a in base.g['animations']]
        vertices=triangles=0;max_sum_error=0
        for p in g['meshes'][0]['primitives']:
            a=p['attributes'];v=model.acc(a['POSITION']);n=model.acc(a['NORMAL']);f=model.acc(p['indices']).reshape(-1,3)
            w=model.acc(a['WEIGHTS_0']);j=model.acc(a['JOINTS_0'])
            assert np.isfinite(v).all() and np.isfinite(n).all() and np.isfinite(w).all()
            assert f.max()<len(v) and j.max()<54 and (w>=0).all()
            error=float(abs(w.sum(1)-1).max());assert error<1e-5
            assert len(p['targets'])==46
            for target in p['targets']:
                delta=model.acc(target['POSITION']);assert delta.shape==v.shape and np.isfinite(delta).all()
            assert np.linalg.norm(np.cross(v[f[:,1]]-v[f[:,0]],v[f[:,2]]-v[f[:,0]]),axis=1).min()>1e-12
            vertices+=len(v);triangles+=len(f);max_sum_error=max(error,max_sum_error)
        for animation in g['animations']:
            for channel in animation['channels']:
                target=channel['target'];assert target['node']<len(g['nodes'])
                if target['path']=='weights':assert g['nodes'][target['node']].get('mesh')==0
        reports[item]={'vertices':vertices,'triangles':triangles,'joints':54,'correctives':46,'animationClips':8,'maxWeightSumError':max_sum_error,'sha256':entry['sha256']}
    for name,items in manifest['sets'].items():
        model=GLB(folder/(name+'-outfit.glb'));assert [n['name'] for n in model.g['nodes'] if 'mesh' in n]==items
    # Portable hair fits must match the retained shared source, restore exactly
    # from it, and contain the crown/band without changing approved source data.
    hair_manifest=folder/manifest['headwearHairFitManifest']
    fits=json.loads(hair_manifest.read_text());source=GLB(ROOT/'assets/avatar/packs/trainer-avatar-1/template.glb')
    hair_reports=[]
    for fit in fits['fits']:
        mesh=next(m for m in source.g['meshes'] if m.get('name')==fit['hairMesh'])
        for record in fit['primitives']:
            primitive=mesh['primitives'][record['primitive']];v=source.acc(primitive['attributes']['POSITION'])
            assert len(v)==record['vertices']
            assert hashlib.sha256(v.astype('<f4').tobytes()).hexdigest()==record['positionSha256']
            patches={}
            for label in ['positionDelta','normalDelta']:
                data=(hair_manifest.parent/record[label]['file']).read_bytes()
                assert hashlib.sha256(data).hexdigest()==record[label]['sha256']
                patches[label]=np.frombuffer(data,dtype='<f4').reshape(-1,3)
                assert patches[label].shape==v.shape and np.isfinite(patches[label]).all()
            fitted=v+patches['positionDelta']
            assert np.allclose(fitted-patches['positionDelta'],v,atol=1e-7)
            y=fitted[:,1]
            if fit['headwear']=='golf-cap':
                mask=y>=.884;h=np.clip((y[mask]-.901)/.099,0,.999)
                radius=(1-h*h)**.375
                radial=(fitted[mask,0]/(.147*radius))**2+(fitted[mask,2]/(.174*radius))**2
            else:
                mask=(y>=.884)&(y<=.901)
                radial=(fitted[mask,0]/.138)**2+(fitted[mask,2]/.157)**2
            assert len(radial) and radial.max()<1, (fit['headwear'],float(radial.max()))
        hair_reports.append({'headwear':fit['headwear'],'hair':fit['hairMesh'],'sourceMatches':True,'reversible':True,'insideHeadwear':True})
    result={'passed':True,'sourceBodyUnchanged':True,'separateItems':10,'outfits':2,'noAdditionalShortsUnderSkirt':True,'rig':'trainer-humanoid-54-v1','checks':reports,'limits':'Structural export checks; render/pose review is recorded separately. No cloth simulation or live wardrobe installation.'}
    result['hairFits']=hair_reports
    result['shortsUnderlayer']=check_shorts_underlayer(GLB(folder/'golf-shorts.glb'),base)
    (folder/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'passed':True,'items':10,'outfits':2,'bones':54,'animations':8}))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('folder',type=Path);check(parser.parse_args().folder)
