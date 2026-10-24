"""Bind the sleeveless tank to the torso; no cloth solver or limb correctives.

Run against a runtime pack. Body buffers, bind pose and animation stay byte exact.
Fits the garment's depth to the torso with a small loose clearance.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree
from build_pack import GLB


def torso_weights(joints, weights, names):
    result = np.zeros((len(joints), len(names)))
    for source, name in enumerate(names):
        target = name
        if name.startswith(('upper_arm.', 'forearm.', 'hand.')):
            target = 'chest'
        elif name.startswith('shoulder_blend.'):
            target = 'clavicle.' + name[-1]
        elif name.startswith(('thigh.', 'shin.')):
            target = 'pelvis'
        result[:, names.index(target)] += (weights * (joints == source)).sum(1)
    selected = np.argsort(result, axis=1)[:, -4:]
    values = np.take_along_axis(result, selected, axis=1)
    values /= values.sum(1)[:, None]
    return selected.astype('<u2'), values.astype('<f4')


def bind(pack):
    manifest_path = pack / 'manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    if manifest.get('clothing', {}).get('fit') == 1:
        print('Torso skin and rest fit already applied.')
        return
    path = pack / 'template.glb'
    src = GLB(path)
    g = src.g
    binary = bytearray(src.b)
    names = [g['nodes'][i]['name'] for i in g['skins'][0]['joints']]
    nodes = [i for i, n in enumerate(g['nodes']) if n.get('name') == 'shirt']
    assert len(nodes) == 1
    mesh = g['meshes'][g['nodes'][nodes[0]]['mesh']]
    body = g['meshes'][0]['primitives'][0]['attributes']
    body_pos = src.acc(body['POSITION'])
    body_normal = src.acc(body['NORMAL'])
    body_joints, body_weights = src.acc(body['JOINTS_0']), src.acc(body['WEIGHTS_0'])
    torso = (body_weights * np.isin(body_joints, [names.index(n) for n in
              ['root', 'pelvis', 'spine', 'chest', 'clavicle.L', 'clavicle.R', 'neck']])).sum(1) > .85

    def replace(index, values):
        a = g['accessors'][index]
        view = g['bufferViews'][a['bufferView']]
        assert 'sparse' not in a and 'byteStride' not in view
        start = view.get('byteOffset', 0) + a.get('byteOffset', 0)
        raw = values.tobytes()
        width = {'VEC3': 3, 'VEC4': 4}[a['type']]
        assert len(raw) == a['count'] * width * values.dtype.itemsize
        binary[start:start + len(raw)] = raw

    changed = set()
    for primitive in mesh['primitives']:
        a = primitive['attributes']
        pos = src.acc(a['POSITION']).copy()
        # Bring the front/back panels close to the torso in the bind pose.
        # Keep a loose hem; a large rear air gap used to look like a second back.
        for side in [-1, 1]:
            donor = body_pos[torso & (body_normal[:, 2] * side > .4)]
            selected = pos[:, 2] * side > 0
            points = pos[selected]
            distance, nearest = cKDTree(donor[:, :2]).query(points[:, :2], k=24)
            weight = 1 / np.maximum(distance, .001) ** 3
            depth = (donor[nearest, 2] * weight).sum(1) / weight.sum(1)
            clearance = .010 + .008 * np.clip((.60 - points[:, 1]) / .17, 0, 1)
            influence = np.clip(abs(points[:, 2]) / .035, 0, 1)
            pos[selected, 2] = points[:, 2] * (1 - influence) + (depth + side * clearance) * influence
        faces = src.acc(primitive['indices']).reshape(-1, 3)
        # Smooth the fitted depth across the actual surface, including UV seams.
        # Preserve the garment's width, neckline and hem rather than shrinking it.
        from scipy.sparse import coo_matrix
        _, weld = np.unique(np.round(pos, 6), axis=0, return_inverse=True)
        edges = np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]])
        edges = np.unique(np.sort(weld[edges], axis=1), axis=0)
        edges = np.concatenate([edges, edges[:, ::-1]])
        count = weld.max() + 1
        adjacency = coo_matrix((np.ones(len(edges)), (edges[:, 0], edges[:, 1])), shape=(count, count)).tocsr()
        degree = np.maximum(np.asarray(adjacency.sum(1)).ravel(), 1)
        multiplicity = np.bincount(weld)
        for _ in range(24):
            depth = np.bincount(weld, weights=pos[:, 2]) / multiplicity
            pos[:, 2] = .55 * pos[:, 2] + .45 * (adjacency @ depth / degree)[weld]
        replace(a['POSITION'], pos.astype('<f4'))
        changed.add(a['POSITION'])
        g['accessors'][a['POSITION']].update(min=pos.min(0).tolist(), max=pos.max(0).tolist())
        _, weld = np.unique(np.round(pos, 6), axis=0, return_inverse=True)
        normals = np.zeros((weld.max() + 1, 3))
        face_normals = np.cross(pos[faces[:, 1]] - pos[faces[:, 0]], pos[faces[:, 2]] - pos[faces[:, 0]])
        for corner in range(3):
            np.add.at(normals, weld[faces[:, corner]], face_normals)
        normals = normals[weld]
        normals /= np.maximum(np.linalg.norm(normals, axis=1)[:, None], 1e-8)
        replace(a['NORMAL'], normals.astype('<f4'))
        changed.add(a['NORMAL'])
        joints, weights = torso_weights(src.acc(a['JOINTS_0']), src.acc(a['WEIGHTS_0']), names)
        for semantic, values in [('JOINTS_0', joints), ('WEIGHTS_0', weights)]:
            replace(a[semantic], values)
            changed.add(a[semantic])
        primitive.pop('targets', None)
    mesh.pop('weights', None)
    mesh.pop('extras', None)
    for animation in g.get('animations', []):
        animation['channels'] = [c for c in animation['channels']
                                 if c['target'] != {'node': nodes[0], 'path': 'weights'}]
    raw = json.dumps(g, separators=(',', ':')).encode()
    raw += b' ' * (-len(raw) % 4)
    path.write_bytes(struct.pack('<III', 0x46546c67, 2, 28 + len(raw) + len(binary))
                     + struct.pack('<II', len(raw), 0x4e4f534a) + raw
                     + struct.pack('<II', len(binary), 0x004e4942) + binary)
    result = GLB(path)
    for index in range(len(g['accessors'])):
        if index not in changed:
            assert np.array_equal(src.acc(index), result.acc(index)), index
    manifest['revision'] = 2
    manifest['clothing'] = {'deformation': 'torso-skin', 'simulation': False, 'fit': 1}
    manifest['files']['template.glb'] = hashlib.sha256(path.read_bytes()).hexdigest()
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print('Torso skin bound; all non-garment accessors preserved.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pack', type=Path)
    bind(parser.parse_args().pack)
