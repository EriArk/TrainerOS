"""Level the shared bridge and move its eye layers with the same depth field.

Writes a separate output; approved inputs and installed packs stay intact.
package_face_bridge applies the same repair to a staged shared runtime pack.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path

import numpy as np
from scipy.interpolate import LinearNDInterpolator
from scipy.spatial import cKDTree
from build_pack import GLB, Builder

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'assets/avatar/approved/2026-10-26/chibi-eyes'


def smooth(lo, hi, value):
    t = np.clip((value-lo)/(hi-lo), 0, 1)
    return t*t*(3-2*t)


class BridgeField:
    def __init__(self):
        body = GLB(SOURCE / 'avatar-prepared-face.glb')
        p = body.acc(body.g['meshes'][0]['primitives'][0]['attributes']['POSITION'])
        select = (abs(p[:, 0]) < .10) & (p[:, 1] > .74) & (p[:, 1] < .89) & (p[:, 2] > .075)
        region = p[select]
        xy, reverse = np.unique(region[:, :2], axis=0, return_inverse=True)
        z = np.full(len(xy), -np.inf)
        np.maximum.at(z, reverse, region[:, 2])
        self.surface = LinearNDInterpolator(xy, z, fill_value=np.nan)
        delta = self.sculpt_amount(p)
        faces = np.concatenate([body.acc(t['indices']).reshape(-1, 3) for t in body.g['meshes'][0]['primitives']])
        faces = faces[select[faces].all(1)]
        tri = p[faces, :2].astype(float)
        self.a = tri[:, 0]; self.ab = tri[:, 1]-self.a; self.ac = tri[:, 2]-self.a
        self.den = self.ab[:, 0]*self.ac[:, 1]-self.ab[:, 1]*self.ac[:, 0]
        valid = abs(self.den) > 1e-12
        self.a = self.a[valid]; self.ab = self.ab[valid]; self.ac = self.ac[valid]; self.den = self.den[valid]
        self.delta = delta[faces[valid]]
        self.depth = p[faces[valid], 2]
        self.tree = cKDTree(tri[valid].mean(1))
        self.cache = {}

    def sculpt_amount(self, p):
        x, y, z = p.T
        # Include the two grooves beside the bridge, not just its center strip.
        w = (1-smooth(.035, .073, abs(x))) * smooth(.762, .773, y) * (1-smooth(.838, .865, y))
        active = (w > 0) & (z > .075)
        result = np.zeros(len(p))
        if active.any():
            base = self.surface(p[active, :2])
            assert np.isfinite(base).all(), 'Depth field does not cover the facial surface'
            target = .11913+(y[active]-.770)*((.12426-.11913)/(.840-.770))-4*x[active]**2
            result[active] = np.maximum(target-base, 0)*w[active]
        return result

    def amount(self, p):
        # Interpolate the ACTUAL body triangles. Evaluating the analytic sculpt
        # independently on denser eyes makes the surfaces diverge at the nose.
        xy = np.ascontiguousarray(p[:, :2], dtype=float)
        key = xy.tobytes()
        if key in self.cache:
            return self.cache[key]*(p[:, 2] > .075)
        result = np.zeros(len(p))
        active = np.flatnonzero((abs(xy[:, 0]) < .085) & (xy[:, 1] > .75) & (xy[:, 1] < .88))
        remaining = active
        for count in [128, len(self.a)]:
            if not len(remaining): break
            ids = self.tree.query(xy[remaining], k=min(count, len(self.a)))[1]
            ap = xy[remaining, None, :]-self.a[ids]
            u = (ap[:, :, 0]*self.ac[ids, 1]-ap[:, :, 1]*self.ac[ids, 0])/self.den[ids]
            v = (self.ab[ids, 0]*ap[:, :, 1]-self.ab[ids, 1]*ap[:, :, 0])/self.den[ids]
            inside = (u >= -1e-6) & (v >= -1e-6) & (u+v <= 1+1e-6)
            found = inside.any(1)
            d = self.delta[ids]; z = self.depth[ids]
            before = z[:, :, 0]+u*(z[:, :, 1]-z[:, :, 0])+v*(z[:, :, 2]-z[:, :, 0])
            shift = d[:, :, 0]+u*(d[:, :, 1]-d[:, :, 0])+v*(d[:, :, 2]-d[:, :, 0])
            # Prepared eye sockets contain overlapping projected triangles.
            # Follow the visible front envelope, never the first triangle hit.
            before = np.where(inside, before, -np.inf)
            after = np.where(inside, before+shift, -np.inf)
            values = np.zeros(len(ids))
            values[found] = after[found].max(1)-before[found].max(1)
            result[remaining[found]] = np.maximum(values[found], 0)
            remaining = remaining[~found]
        # Points outside the front patch have no bridge displacement.
        if len(self.cache) > 32: self.cache.clear()
        self.cache[key] = result
        return result*(p[:, 2] > .075)

    def offset(self, p, eye=False):
        if not eye: return self.sculpt_amount(p)
        d = self.amount(p)
        # A coarse eye triangle can span raised skin even if its vertices are
        # outside the sculpt. Give every eye layer the same 1 mm allowance.
        return d+.001*(p[:, 2] > .075) if eye else d

    def positions(self, p, eye=False):
        q = p.astype(float).copy()
        q[:, 2] += self.offset(p, eye)
        return q.astype('<f4')

    def normals(self, p, n):
        # Inverse-transpose of z'=z+d(x,y). Keeps the authored eye-layer normals.
        out = n.astype(float).copy()
        h = .00015
        for axis in [0, 1]:
            plus = p.astype(float).copy(); minus = plus.copy()
            plus[:, axis] += h; minus[:, axis] -= h
            derivative = (self.offset(plus, True)-self.offset(minus, True))/(2*h)
            out[:, axis] -= derivative*n[:, 2]
        changed = np.any(out != n, axis=1)
        out[changed] /= np.maximum(np.linalg.norm(out[changed], axis=1)[:, None], 1e-12)
        return out.astype('<f4')


def body_normals(p, q, original, faces):
    # The expression body is split into islands; use complete source topology.
    unique, reverse = np.unique(np.round(p, 6), axis=0, return_inverse=True)
    values = np.zeros_like(unique, dtype=float)
    np.add.at(values, reverse, q); values /= np.bincount(reverse)[:, None]
    tri = reverse[faces]; normals = np.zeros_like(values)
    fn = np.cross(values[tri[:, 1]]-values[tri[:, 0]], values[tri[:, 2]]-values[tri[:, 0]])
    for k in range(3): np.add.at(normals, tri[:, k], fn)
    normals /= np.maximum(np.linalg.norm(normals, axis=1)[:, None], 1e-12)
    x, y, z = p.T
    w = (1-smooth(.035, .079, abs(x)))*smooth(.750, .773, y)*(1-smooth(.838, .871, y))*(z > .075)
    out = original.astype(float).copy(); local = w > 0
    out[local] = original[local]*(1-w[local, None])+normals[reverse[local]]*w[local, None]
    out[local] /= np.maximum(np.linalg.norm(out[local], axis=1)[:, None], 1e-12)
    return out.astype('<f4')


def subdivide_eye(model, prim):
    """Split long eye triangles before bending them over the repaired bridge.

    Some very narrow triangles span almost 2 cm vertically despite the dense
    vertex count. Moving only their corners lets their interiors cut the skin.
    Conforming edge splits preserve the existing outline, UVs and head binding.
    """
    attributes = {k: model.acc(v) for k, v in prim['attributes'].items()}
    targets = [{k: model.acc(v) for k, v in t.items()} for t in prim.get('targets', [])]
    faces = model.acc(prim['indices']).reshape(-1, 3)
    original_count = len(attributes['POSITION'])
    for iteration in range(6):
        edges, inverse = np.unique(np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1), axis=0, return_inverse=True)
        p = attributes['POSITION']
        length = np.linalg.norm(p[edges[:, 0], :2]-p[edges[:, 1], :2], axis=1)
        for t in targets:
            if 'POSITION' not in t: continue
            tp = p+t['POSITION']
            length = np.maximum(length, np.linalg.norm(tp[edges[:, 0], :2]-tp[edges[:, 1], :2], axis=1))
        selected = np.flatnonzero(length > .003)
        if not len(selected): break
        pairs = edges[selected]; count = len(p)
        midpoint = np.full(len(edges), -1, dtype=int)
        midpoint[selected] = np.arange(count, count+len(pairs))
        old_normal = attributes['NORMAL']
        averaged_normal = old_normal[pairs].mean(1)
        averaged_normal /= np.maximum(np.linalg.norm(averaged_normal, axis=1)[:, None], 1e-12)
        for name, values in list(attributes.items()):
            if name.startswith('JOINTS'):
                assert np.array_equal(values[pairs[:, 0]], values[pairs[:, 1]]), 'Eye binding is not rigid'
                added = values[pairs[:, 0]]
            elif name == 'NORMAL': added = averaged_normal
            else: added = values[pairs].mean(1)
            attributes[name] = np.concatenate([values, added.astype(values.dtype)])
        for t in targets:
            for name, values in list(t.items()):
                if name == 'NORMAL':
                    added = (old_normal+values)[pairs].mean(1)
                    added /= np.maximum(np.linalg.norm(added, axis=1)[:, None], 1e-12)
                    added -= averaged_normal
                else: added = values[pairs].mean(1)
                t[name] = np.concatenate([values, added.astype(values.dtype)])
        mid = midpoint[inverse.reshape(-1, 3)]; split = []
        for f, m in zip(faces, mid):
            marked = np.flatnonzero(m >= 0)
            if not len(marked): split.append(f); continue
            if len(marked) == 1:
                i = marked[0]; a, c, d = f[i], f[(i+1)%3], f[(i+2)%3]; h = m[i]
                split.extend([[a, h, d], [h, c, d]])
            elif len(marked) == 2:
                i = (np.flatnonzero(m < 0)[0]+1)%3
                a, c, d = f[i], f[(i+1)%3], f[(i+2)%3]; h, j = m[i], m[(i+1)%3]
                split.extend([[c, j, h], [a, h, d], [h, j, d]])
            else:
                a, c, d = f; h, j, k = m
                split.extend([[a, h, k], [h, c, j], [k, j, d], [h, j, k]])
        faces = np.asarray(split, dtype='<u4')
    else: raise AssertionError('Eye subdivision did not converge')
    return attributes, targets, faces, original_count


def repair(source, output, mesh_index=None, eyes_only=False):
    assert source.resolve() != output.resolve()
    model = GLB(source); g = copy.deepcopy(model.g)
    assert 'trainerApprovedFace' not in g.get('extras', {}), 'Owner-approved face is locked; do not apply the rejected bridge sculpt'
    b = Builder.__new__(Builder); b.g = g; b.b = bytearray(model.b)
    field = BridgeField()
    if mesh_index is None and not eyes_only:
        candidates = {n['mesh'] for n in g['nodes'] if n.get('name') == 'Trainer_Body' and 'mesh' in n}
        assert len(candidates) == 1, 'Specify the prepared body mesh explicitly'
        mesh_index = candidates.pop()
    reference = GLB(ROOT / 'assets/avatar/approved/base-body/trainer-body.glb')
    faces = reference.acc(reference.g['meshes'][0]['primitives'][0]['indices']).reshape(-1, 3)
    updated = {}; target_cache = {}; reports = []
    eye_materials = {'Eye white', 'Original painted iris', 'Soft warm eyelid', 'Warm brown eyebrow', 'Reaction hearts'}
    referenced = {n['mesh'] for n in g['nodes'] if 'mesh' in n}
    for mi, mesh in enumerate(g['meshes']):
        if mi not in referenced and mi != mesh_index: continue
        is_body = mi == mesh_index
        is_eye = any(g['materials'][p['material']].get('name') in eye_materials for p in mesh['primitives'])
        if not (is_body or is_eye): continue
        for primitive_index, prim in enumerate(mesh['primitives']):
            a = prim['attributes']; original_key = (a['POSITION'], a['NORMAL'], is_body)
            key = original_key if is_body else (mi, primitive_index, False)
            if is_body:
                p = model.acc(original_key[0]); n = model.acc(original_key[1]); source_count = len(p)
                target_arrays = [{k: model.acc(v) for k, v in t.items()} for t in prim.get('targets', [])]
            else:
                attributes, target_arrays, eye_faces, source_count = subdivide_eye(model, prim)
                p = attributes['POSITION']; n = attributes['NORMAL']
                if len(p) != source_count:
                    prim['indices'] = b.acc(eye_faces.reshape(-1))
                    for name, values in attributes.items():
                        if name not in ['POSITION', 'NORMAL']: a[name] = b.acc(values)
            if key not in updated:
                q = field.positions(p, not is_body)
                nn = body_normals(p, q, n, faces) if is_body else field.normals(p, n)
                updated[key] = (b.acc(q), b.acc(nn), q, nn)
                reports.append({'mesh': mi, 'positionAccessor': original_key[0],
                                'sourceVertices': source_count, 'exportVertices': len(p),
                                'changedVertices': int(np.any(p != q, axis=1).sum()),
                                'maxDepthChange': float(abs(q-p).max())})
            pi, ni, q, nn = updated[key]; a['POSITION'] = pi; a['NORMAL'] = ni
            # Transfer the deformation to blink/gaze/corrective targets as well.
            for target, source_target in zip(prim.get('targets', []), target_arrays):
                tk = (key, tuple(sorted(target.items())))
                if tk not in target_cache:
                    changed_target = dict(target)
                    dp = source_target.get('POSITION', np.zeros_like(p))
                    dn = source_target.get('NORMAL', np.zeros_like(n))
                    local = (abs(p[:, 0]) < .085) & (p[:, 1] > .745) & (p[:, 1] < .88) & (p[:, 2] > .075)
                    if len(p) == source_count and not np.any(dp[local]) and not np.any(dn[local]):
                        target_cache[tk] = changed_target
                        continue
                    tp = p.astype(float)+dp; tq = field.positions(tp, not is_body)
                    if 'POSITION' in target:
                        dq = dp.copy()
                        dq[:, 2] = dp[:, 2]+field.offset(tp, not is_body)-field.offset(p, not is_body)
                        if len(p) != source_count or not np.array_equal(dp, dq): changed_target['POSITION'] = b.acc(dq, sparse=True)
                    if 'NORMAL' in target:
                        tn = n.astype(float)+dn
                        tnn = body_normals(tp, tq, tn, faces) if is_body else field.normals(tp, tn)
                        dnn = (tnn-nn).astype('<f4')
                        untouched = np.all(nn == n, axis=1) & np.all(tnn == tn.astype('<f4'), axis=1)
                        dnn[untouched] = dn[untouched]
                        if len(p) != source_count or not np.array_equal(dn, dnn): changed_target['NORMAL'] = b.acc(dnn, sparse=True)
                    target_cache[tk] = changed_target
                target.clear(); target.update(target_cache[tk])
    assert reports and any(r['changedVertices'] for r in reports)
    output.parent.mkdir(parents=True, exist_ok=True); b.save(output)
    return {'sourceSha256': hashlib.sha256(source.read_bytes()).hexdigest(),
            'outputSha256': hashlib.sha256(output.read_bytes()).hexdigest(), 'surfaces': reports}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path); parser.add_argument('output', type=Path)
    parser.add_argument('--mesh', type=int); parser.add_argument('--eyes-only', action='store_true')
    parser.add_argument('--eye-set', action='store_true', help='Export matching standalone eye assets beside the face')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    report = {args.output.name: repair(args.source, args.output, args.mesh, args.eyes_only)}
    if args.eye_set:
        for source in sorted(SOURCE.glob('eyes-*.glb')):
            report[source.name] = repair(source, args.output.parent/source.name, eyes_only=True)
    if args.report: args.report.write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    print(json.dumps(report))
