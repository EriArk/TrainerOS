"""Exact owner-approved head check for all future avatar pack deliveries."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from build_pack import GLB

REFERENCE = Path(__file__).resolve().parents[2]/'assets/avatar/approved/face-2026-10-29'


def check(pack, reference=REFERENCE):
    lock = json.loads((reference/'reference.json').read_text())
    file = reference/lock['referenceFile']
    assert hashlib.sha256(file.read_bytes()).hexdigest() == lock['referenceSha256'], 'Reference changed'
    source = np.load(file)
    assert str(source['source_sha256']) == lock['sourceSha256']
    p = source['POSITION']*lock['headScale']
    p[:, 1] += .05728
    head = source['POSITION'][:, 1] > lock['sourceHeadMinY']
    m = json.loads((pack/'manifest.json').read_text())
    model = GLB(pack/'template.glb')
    assert m['faceSurface']['referenceId'] == lock['id']
    assert model.g['extras']['trainerApprovedFace']['referenceId'] == lock['id']
    mesh = model.g['meshes'][m['preparedBodyMesh']]
    for prim in mesh['primitives']:
        for attr, expected in [('POSITION', p), ('NORMAL', source['NORMAL'])]:
            actual = model.acc(prim['attributes'][attr])
            assert np.array_equal(actual[head], expected[head]), f'Approved head {attr} changed'
        for attr in ['TEXCOORD_0', 'JOINTS_0', 'WEIGHTS_0']:
            assert np.array_equal(model.acc(prim['attributes'][attr])[head], source[attr][head]), attr
        for target in prim.get('targets', []):
            for attr in ['POSITION', 'NORMAL']:
                if attr in target:
                    assert not np.any(model.acc(target[attr])[head]), 'Body corrective deforms locked head'
    assert int(head.sum()) == lock['headVertices']
    assert hashlib.sha256(p[head].astype('<f4').tobytes()).hexdigest() == lock['headPositionSha256']
    assert hashlib.sha256(source['NORMAL'][head].astype('<f4').tobytes()).hexdigest() == lock['headNormalSha256']
    assert all(m.get('eyeBodyMeshes', {}).get(style, m['preparedBodyMesh']) == m['preparedBodyMesh']
               for style in m['eyeNodes'])
    return {'passed': True, 'reference': lock['id'], 'headVertices': int(head.sum()),
            'positionsExact': True, 'normalsExact': True, 'bodyCorrectivesPreserveHead': True}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pack', type=Path)
    args = parser.parse_args()
    print(json.dumps(check(args.pack)))
