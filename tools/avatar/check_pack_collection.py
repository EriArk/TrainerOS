"""Validate face integration, palette bindings and every shipped garment hash."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from build_pack import GLB
from check_garment_fit import depth_profile


def check(pack, output):
    m=json.loads((pack/'manifest.json').read_text());assert m['revision']>=9
    for name, digest in m['files'].items():
        assert Path(name).name==name
        assert hashlib.sha256((pack/name).read_bytes()).hexdigest()==digest,name
    model=GLB(pack/'template.glb');g=model.g
    body=g['meshes'][m['preparedBodyMesh']]
    p=model.acc(body['primitives'][0]['attributes']['POSITION'])
    faces=np.concatenate([model.acc(t['indices']).reshape(-1,3) for t in body['primitives']])
    errors=[];reference=None
    if m['revision'] >= 10:
        from check_face_reference import check as check_reference
        reference=check_reference(pack)
    else:
        # Historical revision 9 validation, superseded by the owner reference.
        ys=np.linspace(.778,.835,65)
        for x in [-.03,-.02,0,.02,.03]:
            z=depth_profile(p,faces,x,ys)[:,1]
            target=.11913+(ys-.770)*((.12426-.11913)/(.840-.770))-4*x*x
            errors.append(float(abs(z-target).max()))
        assert max(errors)<.00025,errors
    palette={p['accessor'] for p in m['vertexPalette']}
    colours={p['attributes']['COLOR_0'] for mesh in g['meshes'] for p in mesh['primitives'] if 'COLOR_0' in p['attributes']}
    assert palette<=colours,'Palette refers to obsolete pre-subdivision colours'
    for patches in m['headwearFits'].values():
        for patch in patches:
            assert hashlib.sha256(model.acc(patch['accessor']).astype('<f4').tobytes()).hexdigest()==patch['sourceSha256']
    for mesh in g['meshes']:
        for prim in mesh['primitives']:
            count=g['accessors'][prim['attributes']['POSITION']]['count']
            assert all(g['accessors'][a]['count']==count for a in prim['attributes'].values())
            assert all(g['accessors'][a]['count']==count for t in prim.get('targets',[]) for a in t.values())
            assert model.acc(prim['indices']).max()<count
    assert len(g['skins'][0]['joints'])==54 and len(g['animations'])==8
    assert len(m['eyeNodes'])==14 and len(g['extras']['trainerFace']['expressions'])==14
    assert m['starterSets']==['starter-sundress','starter-denim'] and len(m['outfitSets'])==4
    assert len(m['wearables'])==22
    for item in ('sundress','racing-suit','rose-racing-suit'):assert m['wearables'][item]['covers']==['bottom']
    for prefix in ('','rose-'):
        assert m['wearables'][prefix+'racing-suit']['companions']==[prefix+'racing-gloves']
        assert m['wearables'][prefix+'racing-helmet']['hidesHair']
    report=dict(passed=True,revision=m['revision'],templateBytes=(pack/'template.glb').stat().st_size,
                files=len(m['files']),wearables=22,eyeSets=14,
                paletteAccessors=len(palette),hairFitSourcesPreserved=True,rigBones=54,clips=8,
                manifestSha256=hashlib.sha256((pack/'manifest.json').read_bytes()).hexdigest())
    if reference:report['approvedFace']=reference
    else:report['maxBridgePlaneErrorMetres']=max(errors)
    output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('pack',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();check(a.pack,a.output)
