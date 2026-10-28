"""Structural, source-identity and inherited-hem checks on actual exports."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
from build_pack import GLB
from check_garment_fit import check_shorts_underlayer,depth_profile

ROOT=Path(__file__).resolve().parents[2]

def check_dress_sides(model):
    primitive=next(p for p in model.g['meshes'][0]['primitives']
                   if model.g['materials'][p['material']]['name']=='dress')
    v=model.acc(primitive['attributes']['POSITION'])
    f=model.acc(primitive['indices']).reshape(-1,3)
    edges=np.concatenate([f[:,[0,1]],f[:,[1,2]],f[:,[2,0]]])
    a=v[edges[:,0]];b=v[edges[:,1]]
    widths=[]
    for y in np.linspace(.488,.598,45):
        crosses=((a[:,1]<=y)&(b[:,1]>y))|((b[:,1]<=y)&(a[:,1]>y))
        aa=a[crosses];bb=b[crosses]
        points=aa+(bb-aa)*((y-aa[:,1])/(bb[:,1]-aa[:,1]))[:,None]
        widths.append(float(abs(points[:,0]).max()))
    w=np.array(widths)
    peak=float(abs(w[4:-4]-(w[:-8]+w[8:])/2).max())
    reversal=float(np.diff(w).max())
    assert peak<.0013,('Lumpy dress side contour',peak)
    assert reversal<.00015,('Dress side turns into an unwanted waist bump',reversal)
    return {'passed':True,'heightRange':[.488,.598],'chordLength':.020,
            'maxChordDeviation':peak,'maxUpwardWidthIncrease':reversal}


def check_boot_heel(model,color):
    primitive=next(p for p in model.g['meshes'][0]['primitives']
                   if model.g['materials'][p['material']]['name']==color)
    v=model.acc(primitive['attributes']['POSITION']);f=model.acc(primitive['indices']).reshape(-1,3)
    heights=np.linspace(.025,.140,47)
    report=[]
    for x in [-.078,.078]:
        rear=depth_profile(v,f,x,heights)[:,0]
        reversal=float(np.diff(rear).max())
        deviation=float(abs(rear[4:-4]-(rear[:-8]+rear[8:])/2).max())
        assert reversal<.00015,('Boot heel pinches inward below shaft',x,reversal)
        assert deviation<.0008,('Abrupt boot heel/shaft transition',x,deviation)
        report.append({'x':x,'maxInwardStep':reversal,'max20mmChordDeviation':deviation})
    return {'passed':True,'heightRange':[.025,.140],'samples':report}


def check(folder):
    manifest=json.loads((folder/'collection.json').read_text())
    base=GLB(ROOT/'assets/avatar/approved/base-body/trainer-body.glb')
    assert hashlib.sha256((ROOT/'assets/avatar/approved/base-body/trainer-body.glb').read_bytes()).hexdigest()==manifest['baseBodySha256']
    names=[base.g['nodes'][n]['name'] for n in base.g['skins'][0]['joints']]
    inverse=base.acc(base.g['skins'][0]['inverseBindMatrices'])
    assert len(manifest['sets'])==4 and len(manifest['items'])==12
    reports={};fingerprints={}
    def mesh_fingerprint(model,mesh):
        h=hashlib.sha256()
        for primitive in mesh['primitives']:
            h.update(model.g['materials'][primitive['material']]['name'].encode())
            for semantic in ['POSITION','NORMAL','JOINTS_0','WEIGHTS_0']:
                h.update(model.acc(primitive['attributes'][semantic]).tobytes())
            h.update(model.acc(primitive['indices']).tobytes())
            for target in primitive.get('targets',[]):h.update(model.acc(target['POSITION']).tobytes())
        return h.hexdigest()
    for item,entry in manifest['items'].items():
        path=folder/entry['file'];assert hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256']
        model=GLB(path);g=model.g
        assert [g['nodes'][n]['name'] for n in g['skins'][0]['joints']]==names
        assert np.array_equal(model.acc(g['skins'][0]['inverseBindMatrices']),inverse)
        nodes=[n for n in g['nodes'] if 'mesh' in n]
        assert len(nodes)==1 and nodes[0]['name']==item
        assert len(g['meshes'])==1 and g['meshes'][0]['name']==item
        assert [a['name'] for a in g['animations']]==[a['name'] for a in base.g['animations']]
        vertices=triangles=0;max_error=0
        for p in g['meshes'][0]['primitives']:
            a=p['attributes'];v=model.acc(a['POSITION']);n=model.acc(a['NORMAL'])
            f=model.acc(p['indices']).reshape(-1,3);w=model.acc(a['WEIGHTS_0']);j=model.acc(a['JOINTS_0'])
            assert all(np.isfinite(x).all() for x in [v,n,w])
            assert f.max()<len(v) and j.max()<54 and (w>=0).all()
            error=float(abs(w.sum(1)-1).max());assert error<1e-5
            assert len(p['targets'])==46
            for target in p['targets']:
                delta=model.acc(target['POSITION']);assert delta.shape==v.shape and np.isfinite(delta).all()
            assert np.linalg.norm(np.cross(v[f[:,1]]-v[f[:,0]],v[f[:,2]]-v[f[:,0]]),axis=1).min()>1e-12
            if 'helmet' in item:
                head=names.index('head')
                assert np.allclose((w*(j==head)).sum(1),1), 'Helmet must be rigidly head-bound'
            vertices+=len(v);triangles+=len(f);max_error=max(error,max_error)
        for animation in g['animations']:
            for channel in animation['channels']:
                target=channel['target'];assert target['node']<len(g['nodes'])
                if target['path']=='weights':assert g['nodes'][target['node']].get('mesh')==0
        reports[item]={'vertices':vertices,'triangles':triangles,'joints':54,
                       'correctives':46,'animationClips':8,'maxWeightSumError':max_error}
        fingerprints[item]=mesh_fingerprint(model,g['meshes'][0])
    for key,items in manifest['sets'].items():
        model=GLB(folder/(key+'-outfit.glb'))
        assert [n['name'] for n in model.g['nodes'] if 'mesh' in n]==items
        for mesh in model.g['meshes']:
            assert mesh_fingerprint(model,mesh)==fingerprints[mesh['name']]
    combined=GLB(folder/'starter-racing-wardrobe.glb')
    assert len(combined.g['meshes'])==12
    for mesh in combined.g['meshes']:
        assert mesh_fingerprint(combined,mesh)==fingerprints[mesh['name']]
    dress=GLB(folder/'sundress.glb')
    fabric=next(p for p in dress.g['meshes'][0]['primitives']
                if dress.g['materials'][p['material']]['name']=='dress')
    v=dress.acc(fabric['attributes']['POSITION']);f=dress.acc(fabric['indices']).reshape(-1,3)
    edges=np.concatenate([f[:,[0,1]],f[:,[1,2]],f[:,[2,0]]])
    graph=coo_matrix((np.ones(len(edges)),(edges[:,0],edges[:,1])),shape=(len(v),len(v)))
    _,components=connected_components(graph,directed=False)
    joined=False
    for component in np.unique(components):
        points=v[components==component]
        if points[:,1].min()<.30 and points[:,1].max()>.60:
            joined=True
    assert joined,'Bodice and skirt must be a continuous connected fabric surface'
    dress_sides=check_dress_sides(dress)
    hems={}
    for item,color in [('denim-breeches','denim'),('racing-suit','race'),('rose-racing-suit','rose')]:
        model=GLB(folder/(item+'.glb'))
        # The reusable geometric test selects a material by name; rename only this in-memory view.
        for mat in model.g['materials']:
            if mat['name']=='forest':mat['name']='unused-forest'
        next(m for m in model.g['materials'] if m['name']==color)['name']='forest'
        hems[item]=check_shorts_underlayer(model,base)
    boot_checks={}
    for prefix,color in [('', 'race'),('rose-','rose')]:
        pants=GLB(folder/(prefix+'racing-suit.glb'))
        boots=GLB(folder/(prefix+'racing-boots.glb'))
        samples=[]
        for x in [-.09,-.07,-.05,.05,.07,.09]:
            profiles=[]
            for model in [pants,boots]:
                primitive=next(p for p in model.g['meshes'][0]['primitives']
                               if model.g['materials'][p['material']]['name']==color)
                profiles.append(depth_profile(model.acc(primitive['attributes']['POSITION']),
                                               model.acc(primitive['indices']).reshape(-1,3),
                                               x,[.122,.129,.136]))
            leg,shaft=profiles
            gap=float(min((shaft[:,1]-leg[:,1]).min(),(leg[:,0]-shaft[:,0]).min()))
            assert gap>.001,('Trouser hem crosses boot shaft',prefix,x,gap)
            samples.append({'x':x,'minFrontBackClearance':gap})
        boot_checks[prefix+'racing-boots']={'passed':True,'heights':[.122,.129,.136],
                                           'smoothHeel':check_boot_heel(boots,color),
                                           'minimumClearance':.001,'samples':samples}
    result={'passed':True,'items':12,'outfits':4,'sourceBodyUnchanged':True,'assemblyMeshesMatchSeparateItems':True,
            'checks':reports,'underlayerHemChecks':hems,'continuousDressBodiceAndSkirt':joined,
            'trouserInsideBootChecks':boot_checks,
            'smoothDressSideContour':dress_sides,
            'limits':'Structural and rest-pose cross-sections; sampled render poses are separate. No runtime grant, wardrobe/device or gameplay acceptance.'}
    (folder/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'passed':True,'items':12,'outfits':4,'hemChecks':len(hems)}))


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);check(p.parse_args().folder)
