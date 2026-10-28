"""Apply the reviewed bridge after expressions/wardrobe, then compact the pack.

Usage: python tools/avatar/package_face_bridge.py SOURCE_PACK OUTPUT_PACK
The revision-7 input stays intact. All node/mesh/material identities survive.
"""
import argparse
import copy
import hashlib
import json
import shutil
from pathlib import Path
from build_pack import GLB, Builder
from repair_face_bridge import repair


def eye_reserve(model, output, margin=.00025):
    """Extra runtime clearance for all fourteen expression-fitted eye variants."""
    g=copy.deepcopy(model.g);b=Builder.__new__(Builder);b.g=g;b.b=bytearray(model.b)
    eye_materials={'Eye white','Original painted iris','Soft warm eyelid','Warm brown eyebrow','Reaction hearts'}
    positions={}
    for mesh in g['meshes']:
        if not any(g['materials'][p['material']].get('name') in eye_materials for p in mesh['primitives']):continue
        for prim in mesh['primitives']:
            index=prim['attributes']['POSITION'];p=model.acc(index)
            if index not in positions:
                # Translate the complete eye layer. A depth gate here misses
                # its outer edge where a body triangle spans the gate boundary.
                # A uniform translation also preserves every morph delta.
                q=p.copy();q[:,2]+=margin;positions[index]=b.acc(q)
            prim['attributes']['POSITION']=positions[index]
    b.save(output)


def compact(model, manifest, output):
    g = copy.deepcopy(model.g)
    refs = []
    for mesh in g['meshes']:
        for p in mesh['primitives']:
            refs.extend((p['attributes'], k) for k in p['attributes'])
            if 'indices' in p: refs.append((p, 'indices'))
            for t in p.get('targets', []): refs.extend((t, k) for k in t)
    for skin in g['skins']: refs.append((skin, 'inverseBindMatrices'))
    for clip in g['animations']:
        for s in clip['samplers']: refs.extend([(s, 'input'), (s, 'output')])
    if 'uncoveredBodyIndices' in g.get('extras', {}):
        refs.append((g['extras'], 'uncoveredBodyIndices'))
    for p in manifest['vertexPalette']: refs.append((p, 'accessor'))
    for patches in manifest['headwearFits'].values():
        refs.extend((p, 'accessor') for p in patches)
    ordered = sorted({obj[key] for obj, key in refs})
    amap = {old: new for new, old in enumerate(ordered)}
    accessors = [g['accessors'][i] for i in ordered]
    view_refs = []
    for a in accessors:
        if 'bufferView' in a: view_refs.append((a, 'bufferView'))
        for part in a.get('sparse', {}).values():
            if isinstance(part, dict): view_refs.append((part, 'bufferView'))
    view_refs.extend((im, 'bufferView') for im in g['images'] if 'bufferView' in im)
    views = sorted({obj[key] for obj, key in view_refs})
    # Legacy almond replacement is still accepted by the runtime. Its original
    # body accessors are retained even though current eyes use the prepared body.
    for patch in manifest['almondPatches']:
        matches = [i for i, v in enumerate(g['bufferViews'])
                   if v.get('byteOffset', 0) == patch['offset'] and v['byteLength'] == patch['length']]
        assert len(matches) == 1 and matches[0] in views
    b = Builder.__new__(Builder); b.g = g; b.b = bytearray()
    # Only immutable views may share bytes. Palette and hair/legacy patches are
    # edited in place by the resolver, so aliasing those would recolour twice or
    # fit another mesh. Keep each such view independent.
    mutable_accessors = {p['accessor'] for p in manifest['vertexPalette']}
    mutable_accessors.update(p['accessor'] for patches in manifest['headwearFits'].values() for p in patches)
    mutable_views = {g['accessors'][i]['bufferView'] for i in mutable_accessors}
    mutable_views.update(i for i, v in enumerate(g['bufferViews']) for patch in manifest['almondPatches']
                         if v.get('byteOffset', 0) == patch['offset'])
    packed = []; vmap = {}; offsets = {}; immutable = {}
    for i in views:
        v = copy.deepcopy(g['bufferViews'][i]); start = v.get('byteOffset', 0)
        raw = model.b[start:start+v['byteLength']]
        key = (hashlib.sha256(raw).digest(), v.get('byteStride'), v.get('target'))
        if i not in mutable_views and key in immutable:
            vmap[i] = immutable[key]; offsets[start] = packed[vmap[i]]['byteOffset']; continue
        b.b.extend(b'\0' * (-len(b.b) % 4)); offsets[start] = len(b.b)
        v['byteOffset'] = len(b.b); v['buffer'] = 0
        b.b.extend(raw); vmap[i] = len(packed); packed.append(v)
        if i not in mutable_views: immutable[key] = vmap[i]
    for obj, key in refs: obj[key] = amap[obj[key]]
    for obj, key in view_refs: obj[key] = vmap[obj[key]]
    for patch in manifest['almondPatches']: patch['offset'] = offsets[patch['offset']]
    g['accessors'] = accessors; g['bufferViews'] = packed; b.save(output)
    assert output.stat().st_size < 64*1024*1024


def build(source, output):
    assert source.resolve() != output.resolve() and not output.exists()
    manifest = json.loads((source/'manifest.json').read_text())
    assert manifest['revision'] == 7, 'Use the unmodified revision-7 pack; never apply the sculpt twice'
    shutil.copytree(source, output)
    work = output.parent/(output.name+'-face-work.glb')
    report = repair(source/'template.glb', work, manifest['preparedBodyMesh'])
    eye_reserve(GLB(work), work)
    before = GLB(source/'template.glb'); after = GLB(work)
    # Subdivision duplicates/interpolates vertex colours. Recolour those live
    # accessors rather than obsolete pre-subdivision copies.
    colours = {}
    for om, nm in zip(before.g['meshes'], after.g['meshes']):
        for op, np in zip(om['primitives'], nm['primitives']):
            if 'COLOR_0' in op['attributes']:
                colours.setdefault(op['attributes']['COLOR_0'], set()).add(np['attributes']['COLOR_0'])
    manifest['vertexPalette'] = [dict(p, accessor=i) for p in manifest['vertexPalette']
                                 for i in sorted(colours.get(p['accessor'], {p['accessor']}))]
    compact(after, manifest, output/'template.glb')
    manifest['revision'] = 8
    manifest['faceBridge'] = {'version': 1, 'sourceTemplateSha256': report['sourceSha256'],
                              'review': 'face-bridge-v1', 'eyeTriangleLimitMetres': .003,
                              'runtimeExtraEyeClearanceMetres': .00025}
    manifest['files']['template.glb'] = hashlib.sha256((output/'template.glb').read_bytes()).hexdigest()
    (output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf8', newline='\n')
    print(json.dumps({'revision': 8, 'bytes': (output/'template.glb').stat().st_size,
                      'sha256': manifest['files']['template.glb'], 'surfaces': len(report['surfaces'])}))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('source', type=Path); p.add_argument('output', type=Path)
    args = p.parse_args(); build(args.source, args.output)
