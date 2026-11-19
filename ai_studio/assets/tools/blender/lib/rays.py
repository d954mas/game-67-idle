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
        t = np.where(np.isnan(t) | (t < t_min) | (t > t_max), np.inf, t)
        out[block] = np.min(t, axis=1)
    return out


def crossings(origins, directions, tris, t_min=0.0, tolerance=1e-9):
    """Number of surfaces each ray crosses beyond t_min: odd means the origin is inside a closed shell.
    Hits at the same distance count once: a ray through an edge or vertex shared by several
    triangles of one surface crosses that surface once."""
    origins = np.asarray(origins, float).reshape(-1, 3)
    directions = np.asarray(directions, float)
    out = np.zeros(len(origins), np.int64)
    if not len(origins) or not len(tris[0]):
        return out
    for block, t in _blocks(origins, directions, tris):
        t = np.sort(np.where(np.isnan(t) | (t <= t_min), np.inf, t), axis=1)
        finite = np.isfinite(t)
        with np.errstate(invalid="ignore"):
            gap = np.diff(t, axis=1, prepend=-np.inf)
        new = finite & (gap > tolerance * np.maximum(1.0, np.abs(t)))
        out[block] = new.sum(axis=1)
    return out


def zbuffer(uv, depth, xs, ys, pairs=4_000_000):
    """Nearest triangle per sample of an orthographic view, as a rasteriser finds it.

    `uv` (n, 3, 2) are triangle corners in view-plane coordinates, `depth` (n, 3) their distance along
    the view; samples sit at every (xs[i], ys[j]) of two evenly spaced axes. Returns (depth, triangle)
    arrays of shape (len(ys), len(xs)), (inf, -1) where nothing covers the sample. Only the samples
    inside each triangle's bounds are tested, so the cost follows covered area, not rays x triangles."""
    nx, ny = len(xs), len(ys)
    best = np.full(nx * ny, np.inf)
    owner = np.full(nx * ny, -1, np.int64)
    if not len(uv) or not nx or not ny:
        return best.reshape(ny, nx), owner.reshape(ny, nx)
    dx = float(xs[1] - xs[0]) if nx > 1 else 1.0
    dy = float(ys[1] - ys[0]) if ny > 1 else 1.0
    lo, hi = uv.min(axis=1), uv.max(axis=1)
    i0 = np.clip(np.ceil((lo[:, 0] - xs[0]) / dx - 1e-9), 0, nx).astype(np.int64)
    i1 = np.clip(np.floor((hi[:, 0] - xs[0]) / dx + 1e-9), -1, nx - 1).astype(np.int64)
    j0 = np.clip(np.ceil((lo[:, 1] - ys[0]) / dy - 1e-9), 0, ny).astype(np.int64)
    j1 = np.clip(np.floor((hi[:, 1] - ys[0]) / dy + 1e-9), -1, ny - 1).astype(np.int64)
    width, height = np.maximum(i1 - i0 + 1, 0), np.maximum(j1 - j0 + 1, 0)
    count = width * height
    a, b, c = uv[:, 0], uv[:, 1], uv[:, 2]
    area = (b[:, 0] - a[:, 0]) * (c[:, 1] - a[:, 1]) - (b[:, 1] - a[:, 1]) * (c[:, 0] - a[:, 0])
    tris = np.flatnonzero((count > 0) & (np.abs(area) > 1e-18))
    while len(tris):
        take = tris[:max(1, int(np.searchsorted(np.cumsum(count[tris]), pairs)))]
        tris = tris[len(take):]
        tri = np.repeat(take, count[take])
        k = np.arange(len(tri)) - np.repeat(np.cumsum(count[take]) - count[take], count[take])
        i = i0[tri] + k % width[tri]
        j = j0[tri] + k // width[tri]
        px, py = xs[0] + i * dx, ys[0] + j * dy
        ta, tb, tc = a[tri], b[tri], c[tri]
        w1 = ((px - ta[:, 0]) * (tc[:, 1] - ta[:, 1]) - (py - ta[:, 1]) * (tc[:, 0] - ta[:, 0])) / area[tri]
        w2 = ((tb[:, 0] - ta[:, 0]) * (py - ta[:, 1]) - (tb[:, 1] - ta[:, 1]) * (px - ta[:, 0])) / area[tri]
        w0 = 1.0 - w1 - w2
        inside = (w0 >= -1e-9) & (w1 >= -1e-9) & (w2 >= -1e-9)
        tri, sample = tri[inside], (j * nx + i)[inside]
        z = (w0[inside] * depth[tri, 0] + w1[inside] * depth[tri, 1] + w2[inside] * depth[tri, 2])
        order = np.lexsort((z, sample))
        first = order[np.r_[True, np.diff(sample[order]) != 0]] if len(order) else order
        better = z[first] < best[sample[first]]
        best[sample[first[better]]] = z[first[better]]
        owner[sample[first[better]]] = tri[first[better]]
    return best.reshape(ny, nx), owner.reshape(ny, nx)
