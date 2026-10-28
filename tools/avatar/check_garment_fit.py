"""Sample actual triangle surfaces, independently of garment generation."""
import numpy as np


def depth_profile(vertices, faces, x, heights):
    """Front/back Z intersections of vertical samples in a Y-up mesh."""
    triangle = vertices[faces]
    a = triangle[:, 0, :2]
    b = triangle[:, 1, :2] - a
    c = triangle[:, 2, :2] - a
    determinant = b[:, 0] * c[:, 1] - b[:, 1] * c[:, 0]
    valid = abs(determinant) > 1e-12
    profile = []
    for height in heights:
        q = np.array([x, height]) - a
        u = np.divide(q[:, 0] * c[:, 1] - q[:, 1] * c[:, 0], determinant,
                      out=np.full(len(a), -10.), where=valid)
        w = np.divide(b[:, 0] * q[:, 1] - b[:, 1] * q[:, 0], determinant,
                      out=np.full(len(a), -10.), where=valid)
        inside = valid & (u >= -1e-6) & (w >= -1e-6) & (u + w <= 1 + 1e-6)
        if not inside.any():
            raise AssertionError(f'Missing surface at x={x}, y={height}')
        z = (triangle[:, 0, 2] + u * (triangle[:, 1, 2] - triangle[:, 0, 2])
             + w * (triangle[:, 2, 2] - triangle[:, 0, 2]))
        profile.append((z[inside].min(), z[inside].max()))
    return np.array(profile)


def check_shorts_underlayer(shorts, body):
    fabric = next(p for p in shorts.g['meshes'][0]['primitives']
                  if shorts.g['materials'][p['material']]['name'] == 'forest')
    source = body.g['meshes'][0]['primitives'][0]
    height = np.linspace(.330, .390, 31)
    samples = []
    for x in [-.07, -.06, -.05, .05, .06, .07]:
        profiles = []
        for model, primitive in [(shorts, fabric), (body, source)]:
            profiles.append(depth_profile(model.acc(primitive['attributes']['POSITION']),
                                           model.acc(primitive['indices']).reshape(-1, 3),
                                           x, height))
        cloth, skin = profiles
        # A transverse baked hem creates a sharp bulge across the front of
        # each leg. Measure it against a 20 mm chord, not vertex density.
        residual = abs(cloth[5:-5, 1] - (cloth[:-10, 1] + cloth[10:, 1]) / 2)
        peak = float(residual.max())
        clearance = float(min((cloth[:, 1] - skin[:, 1]).min(),
                              (skin[:, 0] - cloth[:, 0]).min()))
        assert peak < .0013, ('Inherited underlayer hem', x, peak)
        assert clearance > .004, ('Garment pulled into underlayer', x, clearance)
        samples.append({'x': x, 'maxFrontChordDeviation': peak,
                        'minFrontBackClearance': clearance})
    return {'passed': True, 'units': 'metres', 'heightRange': [.330, .390],
            'chordLength': .020, 'maxAllowedDeviation': .0013,
            'minimumClearance': .004, 'samples': samples,
            'scope': 'Rest-pose sections across both thighs; not full collision coverage.'}
