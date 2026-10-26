"""Extract the approved body without changing geometry, rig or animations.

Usage: python export-standalone-body.py INPUT.glb OUTPUT.glb
Only Python's standard library is required. Embedded GLB buffers are expected.
"""
import copy
import json
from pathlib import Path
import struct
import sys


def extract(source, destination):
    raw = Path(source).read_bytes()
    assert struct.unpack_from('<I', raw)[0] == 0x46546C67
    length = struct.unpack_from('<I', raw, 12)[0]
    g = json.loads(raw[20:20 + length])
    binary = raw[28 + length:]
    body_node = next(i for i, n in enumerate(g['nodes']) if n.get('name') == 'Trainer_Body')
    mesh_id = g['nodes'][body_node]['mesh']
    body = g['meshes'][mesh_id]
    # Never silently export a garment-specific coverage mask as the base body.
    full_indices = g.get('extras', {}).get('uncoveredBodyIndices')
    assert full_indices is not None and len(body['primitives']) == 1
    body['primitives'][0]['indices'] = full_indices
    removed = {i for i, n in enumerate(g['nodes']) if 'mesh' in n and i != body_node}
    assert all(not g['nodes'][i].get('children') for i in removed)
    node_map = {old: new for new, old in enumerate(i for i in range(len(g['nodes'])) if i not in removed)}
    g['nodes'] = [n for i, n in enumerate(g['nodes']) if i not in removed]
    for node in g['nodes']:
        if 'children' in node:
            node['children'] = [node_map[i] for i in node['children'] if i not in removed]
        if 'mesh' in node:
            node['mesh'] = 0
    g['meshes'] = [body]
    for scene in g['scenes']:
        scene['nodes'] = [node_map[i] for i in scene['nodes'] if i not in removed]
    for skin in g['skins']:
        skin['joints'] = [node_map[i] for i in skin['joints']]
        if 'skeleton' in skin:
            skin['skeleton'] = node_map[skin['skeleton']]
    for animation in g['animations']:
        for channel in animation['channels']:
            assert channel['target']['node'] not in removed
            channel['target']['node'] = node_map[channel['target']['node']]

    materials = sorted({p['material'] for p in body['primitives']})
    g['materials'] = [g['materials'][i] for i in materials]
    for p in body['primitives']:
        p['material'] = materials.index(p['material'])
    texture_slots = []

    def visit(value):
        if isinstance(value, dict):
            for key, item in value.items():
                if key.endswith('Texture') and isinstance(item, dict) and 'index' in item:
                    texture_slots.append(item)
                else:
                    visit(item)
        elif isinstance(value, list):
            for item in value:
                visit(item)

    visit(g['materials'])
    textures = sorted({s['index'] for s in texture_slots})
    g['textures'] = [g['textures'][i] for i in textures]
    for slot in texture_slots:
        slot['index'] = textures.index(slot['index'])
    images = sorted({t['source'] for t in g['textures']})
    g['images'] = [g['images'][i] for i in images]
    for texture in g['textures']:
        texture['source'] = images.index(texture['source'])

    needed = set()
    for p in body['primitives']:
        needed.update(p['attributes'].values())
        needed.add(p['indices'])
        for target in p.get('targets', []):
            needed.update(target.values())
    for skin in g['skins']:
        needed.add(skin['inverseBindMatrices'])
    for animation in g['animations']:
        for sampler in animation['samplers']:
            needed.update([sampler['input'], sampler['output']])
    remap = {old: new for new, old in enumerate(sorted(needed))}
    g['accessors'] = [g['accessors'][i] for i in sorted(needed)]
    for p in body['primitives']:
        p['attributes'] = {k: remap[v] for k, v in p['attributes'].items()}
        p['indices'] = remap[p['indices']]
        for target in p.get('targets', []):
            for key in target:
                target[key] = remap[target[key]]
    for skin in g['skins']:
        skin['inverseBindMatrices'] = remap[skin['inverseBindMatrices']]
    for animation in g['animations']:
        for sampler in animation['samplers']:
            for key in ['input', 'output']:
                sampler[key] = remap[sampler[key]]
    g['extras'] = {'uncoveredBodyIndices': remap[full_indices], 'component': 'approved-base-body'}

    views = set()
    for accessor in g['accessors']:
        if 'bufferView' in accessor:
            views.add(accessor['bufferView'])
        for part in accessor.get('sparse', {}).values():
            if isinstance(part, dict) and 'bufferView' in part:
                views.add(part['bufferView'])
    views.update(image['bufferView'] for image in g['images'])
    view_map = {old: new for new, old in enumerate(sorted(views))}
    output = bytearray()
    new_views = []
    for i in sorted(views):
        view = copy.deepcopy(g['bufferViews'][i])
        offset = view.get('byteOffset', 0)
        output.extend(b'\0' * (-len(output) % 4))
        view['byteOffset'] = len(output)
        output.extend(binary[offset:offset + view['byteLength']])
        new_views.append(view)
    g['bufferViews'] = new_views
    for accessor in g['accessors']:
        if 'bufferView' in accessor:
            accessor['bufferView'] = view_map[accessor['bufferView']]
        for part in accessor.get('sparse', {}).values():
            if isinstance(part, dict) and 'bufferView' in part:
                part['bufferView'] = view_map[part['bufferView']]
    for image in g['images']:
        image['bufferView'] = view_map[image['bufferView']]
    output.extend(b'\0' * (-len(output) % 4))
    g['buffers'] = [{'byteLength': len(output)}]
    encoded = json.dumps(g, separators=(',', ':')).encode()
    encoded += b' ' * (-len(encoded) % 4)
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(struct.pack('<III', 0x46546C67, 2, 28 + len(encoded) + len(output))
                           + struct.pack('<II', len(encoded), 0x4E4F534A) + encoded
                           + struct.pack('<II', len(output), 0x004E4942) + output)
    print(f'Extracted {destination}: one body mesh, {len(g["animations"])} clips')


if __name__ == '__main__':
    extract(*sys.argv[1:])
