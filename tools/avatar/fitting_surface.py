"""Smooth selected underlayer details on a disposable garment fitting guide.

The approved body is also a skin/corrective donor. It may contain baked hems,
seams or underwear: those details must not become the outer garment's shape.
Keep this guide separate from that donor and recompute its normals BEFORE
applying fabric clearance. Never project the result back onto the baked seam.
Bands are author-selected Y intervals in the source's metre/Y-up coordinates.
"""
import numpy as np
from scipy.sparse import coo_matrix


def fitting_guide(positions, faces, bands, iterations=80):
    vertices, reverse = np.unique(np.round(positions, 5), axis=0,
                                  return_inverse=True)
    triangles = reverse[faces]
    edges = np.concatenate([triangles[:, [0, 1]], triangles[:, [1, 2]],
                            triangles[:, [2, 0]]])
    edges = np.unique(np.sort(edges, axis=1), axis=0)
    edges = np.concatenate([edges, edges[:, ::-1]])
    adjacency = coo_matrix((np.ones(len(edges)), (edges[:, 0], edges[:, 1])),
                           shape=(len(vertices), len(vertices))).tocsr()
    degree = np.maximum(np.asarray(adjacency.sum(1)).ravel(), 1)
    strength = np.zeros(len(vertices))
    for lower, upper in bands:
        if not lower < upper:
            raise ValueError('Fitting bands require lower < upper')
        t = np.clip((vertices[:, 1] - lower) / (upper - lower), 0, 1)
        strength = np.maximum(strength, np.sin(np.pi * t) ** 2)
    strength[strength < 1e-12] = 0
    # Paired Taubin passes filter the seam without shrinking the thigh volume.
    # Plain Laplacian relaxation would pull a longer garment into the underlayer.
    for _ in range(iterations):
        for coefficient in (.5, -.53):
            average = (adjacency @ vertices) / degree[:, None]
            vertices += coefficient * strength[:, None] * (average - vertices)
    # Preserve the source exactly outside the selected bands, including seams.
    guide = positions.copy()
    active = strength[reverse] > 0
    guide[active] = vertices[reverse[active]]
    face_normals = np.cross(vertices[triangles[:, 1]] - vertices[triangles[:, 0]],
                            vertices[triangles[:, 2]] - vertices[triangles[:, 0]])
    normals = np.zeros_like(vertices)
    for column in range(3):
        np.add.at(normals, triangles[:, column], face_normals)
    normals /= np.maximum(np.linalg.norm(normals, axis=1)[:, None], 1e-12)
    return guide, normals[reverse]
