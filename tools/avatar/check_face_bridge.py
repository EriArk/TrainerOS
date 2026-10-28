"""Bounded bridge regression and preservation checks on exported review geometry."""
import hashlib
import json
import argparse
from pathlib import Path
import numpy as np
from build_pack import GLB
from check_garment_fit import depth_profile

ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--resolved-before',type=Path);parser.add_argument('--resolved-after',type=Path)
args=parser.parse_args()
source=ROOT/'assets/avatar/approved/2026-10-26/chibi-eyes/avatar-prepared-face.glb'
folder=ROOT/'assets/avatar/collections/face-bridge-v1'
before=GLB(source);after=GLB(folder/'prepared-face.glb')
a=before.g['meshes'][0]['primitives'][0]['attributes'];b=after.g['meshes'][0]['primitives'][0]['attributes']
p=before.acc(a['POSITION']);q=after.acc(b['POSITION']);n=after.acc(b['NORMAL'])
changed=np.any(p!=q,axis=1)
assert changed.any() and np.isfinite(q).all() and np.isfinite(n).all()
assert np.array_equal(p[:,:2],q[:,:2])
assert (abs(p[changed,0])<.085).all() and (p[changed,1]>.750).all() and (p[changed,1]<.880).all()
assert (q[changed,2]>p[changed,2]).all()
assert np.max(abs(np.linalg.norm(n,axis=1)-1))<1e-4
for field in ['nodes','skins','animations','materials','images','textures','scenes']:
    assert before.g.get(field)==after.g.get(field),field
for index in range(len(before.g['accessors'])):
    assert np.array_equal(before.acc(index),after.acc(index)),index
for old,new in zip(before.g['meshes'],after.g['meshes']):
    assert len(old['primitives'])==len(new['primitives'])
    for op,np_ in zip(old['primitives'],new['primitives']):
        for field in set(op)-{'attributes','targets'}:assert op[field]==np_[field],field
        for field in set(op['attributes'])-{'POSITION','NORMAL'}:
            assert op['attributes'][field]==np_['attributes'][field],field
faces=np.concatenate([before.acc(t['indices']).reshape(-1,3) for t in before.g['meshes'][0]['primitives']])
ys=np.linspace(.770,.830,121)
def hollow(vertices):
    z=depth_profile(vertices,faces,0,ys)[:,1]
    chord=np.linspace(z[0],z[-1],len(z))
    return float((chord-z).max())
old_depth=hollow(p);new_depth=hollow(q)
assert old_depth>.010 and new_depth<.002,('Bridge hollow regression including nose boundary',old_depth,new_depth)
core_y=np.linspace(.778,.835,65)
core_z=depth_profile(q,faces,0,core_y)[:,1]
plane=.11913+(core_y-.770)*((.12426-.11913)/(.840-.770))
plane_error=float(abs(core_z-plane).max())
assert plane_error<.0001,('Bridge depth is not level',plane_error)
# The rejected narrow fix passed the center check but retained both side grooves.
side_errors=[]
for x in [-.03,-.02,.02,.03]:
    z=depth_profile(q,faces,x,core_y)[:,1]
    side_errors.append(float(abs(z-(plane-4*x*x)).max()))
assert max(side_errors)<.00025,('Side grooves remain',side_errors)

def surface_depth(vertices,points,triangles=None):
    tri=vertices[faces if triangles is None else triangles].astype(float)
    tri=tri[(tri[:,:,2]>.06).all(1)&(tri[:,:,1]>.72).all(1)&(tri[:,:,1]<.87).all(1)]
    a=tri[:,0,:2];ab=tri[:,1,:2]-a;ac=tri[:,2,:2]-a
    den=ab[:,0]*ac[:,1]-ab[:,1]*ac[:,0]
    valid=abs(den)>1e-12;tri=tri[valid];a=a[valid];ab=ab[valid];ac=ac[valid];den=den[valid]
    # Exact triangle AABB bins keep the dense-eye check bounded. Unlike nearest
    # centers, every triangle covering a query point is included.
    cell=.006;bins={}
    lo=np.floor((tri[:,:,:2].min(1)-1e-7)/cell).astype(int)
    hi=np.floor((tri[:,:,:2].max(1)+1e-7)/cell).astype(int)
    for i,(l,h) in enumerate(zip(lo,hi)):
        for x in range(l[0],h[0]+1):
            for y in range(l[1],h[1]+1):bins.setdefault((x,y),[]).append(i)
    cells=np.floor(points[:,:2]/cell).astype(int);unique,reverse=np.unique(cells,axis=0,return_inverse=True)
    result=np.full(len(points),-np.inf)
    for index,key in enumerate(unique):
        candidates=bins.get(tuple(key),[])
        if not candidates:continue
        ids=np.array(candidates);group=np.flatnonzero(reverse==index)
        for start in range(0,len(group),256):
            rows=group[start:start+256];ap=points[rows,None,:2]-a[ids]
            u=(ap[:,:,0]*ac[ids,1]-ap[:,:,1]*ac[ids,0])/den[ids]
            v=(ab[ids,0]*ap[:,:,1]-ab[ids,1]*ap[:,:,0])/den[ids]
            z=tri[ids,0,2]+u*(tri[ids,1,2]-tri[ids,0,2])+v*(tri[ids,2,2]-tri[ids,0,2])
            tolerance=1e-7*np.maximum(np.linalg.norm(ab[ids],axis=1),np.linalg.norm(ac[ids],axis=1))/abs(den[ids])
            inside=(u>=-tolerance)&(v>=-tolerance)&(u+v<=1+tolerance)
            result[rows]=np.where(inside,z,-np.inf).max(1)
    return result
def eye_samples(v,f):
    tri=v[f,:2].astype(float)
    ab=tri[:,1]-tri[:,0];ac=tri[:,2]-tri[:,0]
    # Unused vertices and zero-area side walls have no front-facing surface.
    f=f[abs(ab[:,0]*ac[:,1]-ab[:,1]*ac[:,0])>1e-12]
    return np.concatenate([v[np.unique(f)],v[f].mean(1),v[f[:,[0,1]]].mean(1),v[f[:,[1,2]]].mean(1),v[f[:,[2,0]]].mean(1)])

eye_checks={}
for file in sorted(source.parent.glob('eyes-*.glb')):
    model=GLB(file);fixed=GLB(folder/file.name);max_gap_loss=0;raised_clearance=[]
    for field in ['nodes','skins','animations','materials','images','textures','scenes']:
        assert model.g.get(field)==fixed.g.get(field),(file.name,field)
    for index in range(len(model.g['accessors'])):
        assert np.array_equal(model.acc(index),fixed.acc(index)),(file.name,index)
    for mesh,new_mesh in zip(model.g['meshes'],fixed.g['meshes']):
        for prim,new_prim in zip(mesh['primitives'],new_mesh['primitives']):
            old_v=model.acc(prim['attributes']['POSITION']);new_v=fixed.acc(new_prim['attributes']['POSITION'])
            assert np.array_equal(old_v[:,:2],new_v[:len(old_v),:2]),'Original eye outline moved'
            for attr in set(prim['attributes'])-{'POSITION','NORMAL'}:
                original=model.acc(prim['attributes'][attr]);updated=fixed.acc(new_prim['attributes'][attr])
                assert np.array_equal(original,updated[:len(original)]),(file.name,attr)
                if attr.startswith(('JOINTS','WEIGHTS')):assert (updated==original[0]).all()
            assert prim['material']==new_prim['material']
            assert len(prim.get('targets',[]))==len(new_prim.get('targets',[]))
            if model.g['materials'][prim['material']]['name']!='Eye white':continue
            f=model.acc(prim['indices']).reshape(-1,3);nf=fixed.acc(new_prim['indices']).reshape(-1,3)
            assert nf.max()<len(new_v)
            new_samples=eye_samples(new_v,nf)
            samples=new_samples.copy();samples[:,2]=surface_depth(old_v,new_samples,f)
            assert np.isfinite(samples[:,2]).all(),'Tessellation changed the eye footprint'
            old_skin=surface_depth(p,samples);new_skin=surface_depth(q,samples)
            valid=np.isfinite(old_skin)&np.isfinite(new_skin)
            assert np.array_equal(np.isfinite(old_skin),np.isfinite(new_skin)) and valid.any()
            movement=np.zeros(len(samples));movement[valid]=new_skin[valid]-old_skin[valid]
            affected=valid&(movement>1e-6)
            if affected.any():
                old_gap=samples[affected,2]-old_skin[affected]
                new_gap=new_samples[affected,2]-new_skin[affected]
                clearance=float(new_gap.min());loss=float((old_gap-new_gap).max())
                max_gap_loss=max(max_gap_loss,loss)
                # Different tessellations need positive clearance, not an equal
                # gap at every barycentric sample. Retain a 0.25 mm margin on
                # formerly exposed white surfaces, including triangle interiors.
                assert (new_gap[old_gap>.00035]>.00025).all(),('Eye clearance below margin',file.name,clearance)
                raised_clearance.append(clearance)
    eye_checks[file.stem]={'maxEyeClearanceLoss':max_gap_loss,
                          'minimumEyeClearanceWhereSkinRaised':min(raised_clearance) if raised_clearance else None,
                          'sha256':hashlib.sha256((folder/file.name).read_bytes()).hexdigest()}
assert len(eye_checks)==12
expression_checks={}
if args.resolved_before or args.resolved_after:
    assert args.resolved_before and args.resolved_after
    old=GLB(args.resolved_before);new=GLB(args.resolved_after)
    for node in old.g['nodes']:
        if 'mesh' not in node or not node.get('name','').endswith('-white'):continue
        om=old.g['meshes'][node['mesh']];nm=new.g['meshes'][node['mesh']]
        names=om['extras']['targetNames']
        for control in ['rest','eyes_soft','eyes_wide','eyes_half']:
            minimum=np.inf;loss=0
            for op,np_ in zip(om['primitives'],nm['primitives']):
                ov=old.acc(op['attributes']['POSITION']);nv=new.acc(np_['attributes']['POSITION'])
                if control!='rest':
                    index=names.index(control)
                    ov=ov+old.acc(op['targets'][index]['POSITION'])
                    nv=nv+new.acc(np_['targets'][index]['POSITION'])
                assert np.max(abs(ov[:,:2]-nv[:len(ov),:2]))<1e-7
                f=old.acc(op['indices']).reshape(-1,3);nf=new.acc(np_['indices']).reshape(-1,3)
                ns=eye_samples(nv,nf)
                os=ns.copy();os[:,2]=surface_depth(ov,ns,f)
                assert np.isfinite(os[:,2]).all(),'Expression footprint changed'
                oz=surface_depth(p,os);nz=surface_depth(q,ns)
                valid=np.isfinite(oz)&np.isfinite(nz)
                field_changed=np.zeros(len(ns),dtype=bool);field_changed[valid]=nz[valid]-oz[valid]>1e-6
                valid &= field_changed
                og=os[valid,2]-oz[valid];ng=ns[valid,2]-nz[valid]
                minimum=min(minimum,float(ng.min()));loss=max(loss,float((og-ng).max()))
                assert (ng[og>.00035]>.00015).all(),('Expression clearance below margin',control,float(ng.min()))
            expression_checks[control]={'minimumEyeClearance':minimum,'maximumClearanceLoss':loss}
result={'passed':True,'changedVertices':int(changed.sum()),'originalAccessorsPreserved':len(before.g['accessors']),
        'rigAnimationMaterialsUVUnchanged':True,'eyeSilhouettesPreserved':True,'oldBridgeHollow':old_depth,
        'newBridgeHollow':new_depth,'maxCorePlaneError':plane_error,'eyePairs':eye_checks,
        'maxSidePlaneError':max(side_errors),
        'resolvedExpressionChecks':expression_checks,
        'scope':'Shared prepared face and rest-pose eye clearance; no new runtime/device or full-expression acceptance.'}
(folder/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'passed':True,'changedVertices':int(changed.sum()),'eyePairs':12,'oldHollow':old_depth,'newHollow':new_depth}))
