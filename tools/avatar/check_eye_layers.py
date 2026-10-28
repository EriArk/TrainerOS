"""Check iris/white ordering through real exported facial morph interpolation."""
import argparse
import json
from pathlib import Path
import numpy as np
from build_pack import GLB
from restore_painted_face import Surface


def check(pack):
    manifest = json.loads((pack/'manifest.json').read_text())
    model = GLB(pack/'template.glb')
    cases = []
    for style, nodes in manifest['eyeNodes'].items():
        meshes = {model.g['nodes'][i]['name'].rsplit('-', 1)[-1]:
                  model.g['meshes'][model.g['nodes'][i]['mesh']] for i in nodes}
        white, iris = meshes['white'], meshes['iris']
        names = iris['extras']['targetNames']
        # All four symbolic controls collapse ordinary eyes identically.
        for mesh in [white, iris]:
            for prim in mesh['primitives']:
                expected = model.acc(prim['targets'][names.index('eyes_closed')]['POSITION'])
                for control in ['eyes_squeezed', 'eyes_hearts', 'eyes_shocked']:
                    assert np.array_equal(expected, model.acc(prim['targets'][names.index(control)]['POSITION']))
        for control in [None, 'eyes_soft', 'eyes_wide', 'eyes_half', 'eyes_closed']:
            for weight in ([0] if control is None else [.125, .25, .5, .75, .875, 1]):
                def pose(prim):
                    p = model.acc(prim['attributes']['POSITION'])
                    if control is not None:
                        p += model.acc(prim['targets'][names.index(control)]['POSITION'])*weight
                    return p
                positions, faces, normals = [], [], []
                count = 0
                for prim in white['primitives']:
                    p = pose(prim)
                    positions.append(p)
                    faces.append(model.acc(prim['indices']).reshape(-1, 3)+count)
                    normals.append(model.acc(prim['attributes']['NORMAL']))
                    count += len(p)
                p, f = np.concatenate(positions), np.concatenate(faces)
                tri = p[f]
                valid = abs(np.cross(tri[:, 1]-tri[:, 0], tri[:, 2]-tri[:, 0])[:, 2]) > 1e-12
                if not valid.any():
                    continue  # Fully collapsed invisible eye has no surface.
                surface = Surface(p, np.concatenate(normals), f[valid])
                minimum = 1.
                for prim in iris['primitives']:
                    p = pose(prim)
                    tri = p[model.acc(prim['indices']).reshape(-1, 3)]
                    samples = np.concatenate([p, tri.mean(1), (tri[:, 0]+tri[:, 1])*.5,
                                              (tri[:, 1]+tri[:, 2])*.5, (tri[:, 2]+tri[:, 0])*.5])
                    depth, _ = surface.sample(samples, allow_missing=True)
                    covered = np.isfinite(depth)
                    if covered.any():
                        minimum = min(minimum, float((samples[covered, 2]-depth[covered]).min()))
                cases.append({'style': style, 'control': control, 'weight': weight, 'clearance': minimum})
    worst = min(cases, key=lambda c: c['clearance'])
    assert worst['clearance'] > .0001, ('Iris intersects white', worst)
    return {'passed': True, 'eyeStyles': len(manifest['eyeNodes']), 'posesAndWeights': len(cases),
            'minimumIrisWhiteClearanceMetres': worst['clearance'], 'worstCase': worst}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pack', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    report = check(args.pack)
    if args.report:
        args.report.write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    print(json.dumps(report))
