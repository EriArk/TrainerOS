"""Restore the owner's painted-face geometry; fit eye layers to that surface.

The reference is lossless geometry extracted from the supplied GLB, not a sculpt
of a previously prepared face. Never modify the input pack or reference file.
"""
import argparse
import copy
import hashlib
import json
import shutil
from pathlib import Path

import numpy as np
from build_pack import GLB, Builder
from package_face_bridge import compact


def extract_reference(source, output):
    model = GLB(source)
    prim = model.g['meshes'][0]['primitives'][0]
    values = {key: model.acc(index) for key, index in prim['attributes'].items()}
    values['indices'] = model.acc(prim['indices'])
    values['source_sha256'] = np.array(hashlib.sha256(source.read_bytes()).hexdigest())
    output.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(output, **values)


class Surface:
    """Exact front envelope with authored, barycentrically interpolated normals."""
    def __init__(self, positions, normals, faces):
        tri = positions[faces].astype(float)
        keep = (tri[:, :, 2] > .015).all(1) & (tri[:, :, 1] > .73).all(1)
        keep &= (tri[:, :, 1] < .91).all(1)
        self.tri = tri[keep]
        self.normals = normals[faces[keep]]
        self.a = self.tri[:, 0, :2]
        self.ab = self.tri[:, 1, :2] - self.a
        self.ac = self.tri[:, 2, :2] - self.a
        self.den = self.ab[:, 0]*self.ac[:, 1] - self.ab[:, 1]*self.ac[:, 0]
        valid = abs(self.den) > 1e-12
        for key in ['tri', 'normals', 'a', 'ab', 'ac', 'den']:
            setattr(self, key, getattr(self, key)[valid])
        self.cell = .006
        self.bins = {}
        low = np.floor((self.tri[:, :, :2].min(1)-1e-7)/self.cell).astype(int)
        high = np.floor((self.tri[:, :, :2].max(1)+1e-7)/self.cell).astype(int)
        for i, (lo, hi) in enumerate(zip(low, high)):
            for x in range(lo[0], hi[0]+1):
                for y in range(lo[1], hi[1]+1):
                    self.bins.setdefault((x, y), []).append(i)
        self.cache = {}

    def sample(self, points, allow_missing=False):
        xy = np.ascontiguousarray(points[:, :2])
        key = xy.tobytes()
        if key in self.cache:
            assert allow_missing or np.isfinite(self.cache[key][0]).all()
            return self.cache[key]
        cells = np.floor(xy/self.cell).astype(int)
        unique, reverse = np.unique(cells, axis=0, return_inverse=True)
        depth = np.full(len(points), -np.inf)
        normal = np.zeros((len(points), 3))
        for k, cell in enumerate(unique):
            ids = np.asarray(self.bins.get(tuple(cell), []), dtype=int)
            if not len(ids):
                continue
            group = np.flatnonzero(reverse == k)
            for start in range(0, len(group), 256):
                rows = group[start:start+256]
                ap = xy[rows, None, :]-self.a[ids]
                u = (ap[:, :, 0]*self.ac[ids, 1]-ap[:, :, 1]*self.ac[ids, 0])/self.den[ids]
                v = (self.ab[ids, 0]*ap[:, :, 1]-self.ab[ids, 1]*ap[:, :, 0])/self.den[ids]
                inside = (u >= -1e-5) & (v >= -1e-5) & (u+v <= 1+1e-5)
                z = self.tri[ids, 0, 2]+u*(self.tri[ids, 1, 2]-self.tri[ids, 0, 2])+v*(self.tri[ids, 2, 2]-self.tri[ids, 0, 2])
                z = np.where(inside, z, -np.inf)
                best = z.argmax(1)
                selected = ids[best]
                r = np.arange(len(rows))
                depth[rows] = z[r, best]
                weights = np.stack([1-u[r, best]-v[r, best], u[r, best], v[r, best]], axis=1)
                normal[rows] = (self.normals[selected]*weights[:, :, None]).sum(1)
        assert allow_missing or np.isfinite(depth).all(), ('Eye outside reference surface', points[~np.isfinite(depth)].tolist()[:4])
        normal /= np.maximum(np.linalg.norm(normal, axis=1)[:, None], 1e-12)
        result = depth.astype('<f4'), normal.astype('<f4')
        if len(self.cache) > 256:
            self.cache.clear()
        self.cache[key] = result
        return result

    def fit(self, points, offset):
        depth, normal = self.sample(points)
        fitted = points.copy()
        fitted[:, 2] = depth+offset
        return fitted, normal.copy()


def build(source, reference, output):
    assert source.resolve() != output.resolve() and not output.exists()
    manifest = json.loads((source/'manifest.json').read_text())
    assert manifest['revision'] == 9, 'Requires the rejected revision 9, once only'
    model = GLB(source/'template.glb')
    g = copy.deepcopy(model.g)
    assert 'trainerApprovedFace' not in g.get('extras', {}), 'Already restored: do not sculpt the approved reference again'
    b = Builder.__new__(Builder)
    b.g, b.b = g, bytearray(model.b)
    ref = np.load(reference)
    original = ref['POSITION']
    scaled = original*.92
    scaled[:, 1] += .05728  # Scale 0.92 about the approved head pivot y=0.716.
    head = original[:, 1] > .75
    body_id = manifest['preparedBodyMesh']
    body = model.g['meshes'][body_id]['primitives'][0]
    positions = model.acc(body['attributes']['POSITION'])
    normal = model.acc(body['attributes']['NORMAL'])
    assert len(original) == len(positions)
    assert np.allclose(positions[head, :2], scaled[head, :2], atol=1e-7)
    restored = positions.copy()
    restored[head] = scaled[head]
    restored_normal = normal.copy()
    restored_normal[head] = ref['NORMAL'][head]
    surface = Surface(restored, restored_normal, ref['indices'].reshape(-1, 3))
    changed = {}
    # Every current style, including round/almond, selects the prepared mesh.
    # Retain the unused legacy patch views for old resolver compatibility.
    assert all(manifest['eyeBodyMeshes'].get(style, body_id) == body_id
               for style in manifest['eyeNodes'])
    for mesh_id in [body_id]:
        for prim in g['meshes'][mesh_id]['primitives']:
            for attr, replacement in [('POSITION', scaled), ('NORMAL', ref['NORMAL'])]:
                old_id = prim['attributes'][attr]
                if old_id not in changed:
                    values = model.acc(old_id)
                    values[head] = replacement[head]
                    changed[old_id] = b.acc(values)
                prim['attributes'][attr] = changed[old_id]
            for target in prim.get('targets', []):
                for attr in ['POSITION', 'NORMAL']:
                    if attr not in target:
                        continue
                    old_id = target[attr]
                    values = model.acc(old_id)
                    # Existing body correctives have no facial motion. The
                    # bridge pipeline may have introduced numerical normal noise.
                    assert np.max(abs(values[head])) < 1e-5, (attr, old_id)
                    if np.any(values[head]):
                        values[head] = 0
                        target[attr] = b.acc(values, sparse=True)
    eye_meshes = {g['nodes'][i]['mesh'] for nodes in manifest['eyeNodes'].values() for i in nodes}
    for mesh_id in sorted(eye_meshes):
        mesh = g['meshes'][mesh_id]
        reaction = mesh.get('name') == 'Cartoon eye reactions'
        for pi, prim in enumerate(mesh['primitives']):
            p = model.acc(prim['attributes']['POSITION'])
            name = g['materials'][prim['material']]['name']
            offset = {'Eye white': .0025, 'Original painted iris': .0031,
                      'Soft warm eyelid': .0037, 'Warm brown eyebrow': .0025}[name] if not reaction else [.0037, .0037, .0037, .0037, .0041, .0045][pi % 6]
            offset += .0003  # Reserve for triangle interiors during interpolation.
            # Iris and eyelid triangulations differ from the two white surfaces.
            # Keep their ordering through half-close and wide morph interpolation.
            front_layer = not reaction and name in {'Original painted iris', 'Soft warm eyelid'}
            layer_reserve = .0008 if front_layer else 0
            offset += layer_reserve
            # Mirrored authored eye halves used opposite winding. With outward
            # skin normals, backface normal inversion would blacken one eye.
            faces = model.acc(prim['indices']).reshape(-1, 3)
            signed = np.zeros(len(faces))
            for pose in [p, *[p+model.acc(t['POSITION']) for t in prim.get('targets', [])]]:
                tri = pose[faces, :2]
                ab, ac = tri[:, 1]-tri[:, 0], tri[:, 2]-tri[:, 0]
                area = ab[:, 0]*ac[:, 1]-ab[:, 1]*ac[:, 0]
                replace = abs(area) > abs(signed)
                signed[replace] = area[replace]
            reverse = signed < 0
            faces[reverse] = faces[reverse][:, [0, 2, 1]]
            prim['indices'] = b.acc(faces.astype('<u4').reshape(-1))
            # A straight morph chord can cut the original convex eyelid. Give
            # the invisible collapsed endpoint depth reserve, not the skin.
            collapsed_offset = .0133
            q, nn = surface.fit(p, collapsed_offset if reaction else offset)
            prim['attributes']['POSITION'] = b.acc(q)
            prim['attributes']['NORMAL'] = b.acc(nn)
            for control, target in zip(mesh['extras']['targetNames'], prim.get('targets', [])):
                dp = model.acc(target['POSITION'])
                symbolic = control in ['eyes_closed', 'eyes_squeezed', 'eyes_hearts', 'eyes_shocked']
                reserve = (reaction and not np.any(dp)) or (not reaction and symbolic and name != 'Warm brown eyebrow')
                target_offset = collapsed_offset+layer_reserve if reserve else offset
                if front_layer:
                    target_offset += {'eyes_wide': .0015, 'eyes_half': .002}.get(control, 0)
                tq, tn = surface.fit(p+dp, target_offset)
                target['POSITION'] = b.acc((tq-q).astype('<f4'), sparse=True)
                target['NORMAL'] = b.acc((tn-nn).astype('<f4'), sparse=True)
        print('Fitted', mesh_id, mesh.get('name'), flush=True)
    shutil.copytree(source, output)
    work = output.parent/(output.name+'-uncompacted.glb')
    g.setdefault('extras', {})['trainerApprovedFace'] = {
        'referenceId': 'painted-face-2026-10-29', 'sourceSha256': str(ref['source_sha256']),
        'headScale': .92, 'ownerApproved': True}
    b.save(work)
    compact(GLB(work), manifest, output/'template.glb')
    manifest['revision'] = 10
    manifest.pop('faceBridge', None)
    manifest['faceSurface'] = {'version': 2, 'sourceSha256': str(ref['source_sha256']),
                               'referenceId': 'painted-face-2026-10-29', 'headScale': .92,
                               'headPivot': [0, .716, 0], 'method': 'restore-source-and-project-eye-layers'}
    manifest['faceContrast'] = {'version': 1, 'skinReference': '#f7d8bf',
                                'curve': 'sqrt-linear-luminance-ratio', 'minimumFactor': .12,
                                'materials': ['Warm brown eyebrow', 'Soft warm eyelid', 'Mouth Contour']}
    manifest['files']['template.glb'] = hashlib.sha256((output/'template.glb').read_bytes()).hexdigest()
    (output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf8')
    from check_face_reference import check
    check(output)
    print(json.dumps({'revision': 10, 'bytes': (output/'template.glb').stat().st_size,
                      'headVertices': int(head.sum()), 'referenceSha256': str(ref['source_sha256'])}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--extract', type=Path, help='Extract reference from owner-supplied GLB first')
    args = parser.parse_args()
    if args.extract:
        extract_reference(args.extract, args.reference)
    build(args.source, args.reference, args.output)
