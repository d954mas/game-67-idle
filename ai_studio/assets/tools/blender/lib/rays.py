"""Vectorised ray/triangle intersection (Moller-Trumbore) for checks that run without Blender's BVH."""
import numpy as np

# Rays x triangles per numpy block: bounds peak memory to ~200 MB of float64 temporaries.
BLOCK = 2_000_000


def triangles(points, faces):
    points = np.asarray(points, float)
    faces = np.asarray(faces, np.int64).reshape(-1, 3)
    return points[faces[:, 0]], points[faces[:, 1]], points[faces[:, 2]]


def _hits(origins, directions, tris, epsilon=1e-12):
    """(rays, triangles) matrix of hit distances t, NaN where the ray's line misses."""
    a, b, c = tris
    e1, e2 = b - a, c - a
    d = directions[:, None, :] if directions.ndim == 2 else directions[None, None, :]
    p = np.cross(d, e2[None])
    det = (p * e1[None]).sum(-1)
    ok = np.abs(det) > epsilon
    inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
    s = origins[:, None, :] - a[None]
    u = (s * p).sum(-1) * inv
    q = np.cross(s, e1[None])
    v = (q * d).sum(-1) * inv
    t = (q * e2[None]).sum(-1) * inv
    inside = ok & (u >= -1e-9) & (v >= -1e-9) & (u + v <= 1 + 1e-9)
    return np.where(inside, t, np.nan)


def _blocks(origins, directions, tris):
    size = max(1, BLOCK // max(1, len(tris[0])))
    for start in range(0, len(origins), size):
        block = slice(start, start + size)
        yield block, _hits(origins[block], directions[block] if directions.ndim == 2 else directions, tris)


def first_hit(origins, directions, tris, t_min=0.0, t_max=np.inf):
    """Nearest hit distance per ray within [t_min, t_max]; inf where nothing is hit.
    `directions` is one shared (3,) direction or one per ray; distances are in its units."""
    origins = np.asarray(origins, float).reshape(-1, 3)
    directions = np.asarray(directions, float)
    out = np.full(len(origins), np.inf)
    if not len(origins) or not len(tris[0]):
        return out
    for block, t in _blocks(origins, directions, tris):
        t = np.where((t >= t_min) & (t <= t_max), t, np.inf)
        out[block] = np.min(np.nan_to_num(t, nan=np.inf), axis=1)
    return out


def crossings(origins, directions, tris, t_min=0.0):
    """Number of surfaces each ray crosses beyond t_min: odd means the origin is inside a closed shell."""
    origins = np.asarray(origins, float).reshape(-1, 3)
    directions = np.asarray(directions, float)
    out = np.zeros(len(origins), np.int64)
    if not len(origins) or not len(tris[0]):
        return out
    for block, t in _blocks(origins, directions, tris):
        out[block] = np.sum(np.nan_to_num(t, nan=-np.inf) > t_min, axis=1)
    return out
