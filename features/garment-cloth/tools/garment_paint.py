"""Paint the freedom map `_CLOTH` of a garment piece from its geometry.

`_CLOTH` is 0 on a pin (the vertex follows skinning) and in (0, 1] elsewhere; a free vertex is driven toward
its target at drive_hz * (1 - _CLOTH). Pins are the top `seam_fraction` of the piece's sheet, measured from the
seam: the sheet holds every triangle with a corner on the piece, so its top is the skinned part the piece is sewn to.
The drive weight falls along the surface distance from the pins. A drive weight w is a frequency ratio of sqrt(w), so
_CLOTH = 1 - sqrt(w).
A piece's seam is its not-cloth side: an unlabelled vertex is skinned and holds the border, so the labelled row next
to it stays free. Two kinds of vertex are pins besides the band: one covered by an outer layer (an inner piece
follows skinning there, since nothing keeps two simulated layers apart) and one split from an unlabelled vertex at
the same position (`coincident`), which would open a crack if it moved.
"""
import heapq

import numpy as np
from scipy.spatial import cKDTree

FREE_FLOOR = 1.0 / 64.0  # a free vertex never reads as a pin
COINCIDENT_M = 1e-6


def coincident(points, others, tolerance=COINCIDENT_M):
    """Mask of `points` that sit on one of `others` within `tolerance` metres."""
    points, others = np.asarray(points, float).reshape(-1, 3), np.asarray(others, float).reshape(-1, 3)
    if not len(points) or not len(others):
        return np.zeros(len(points), bool)
    distance, _ = cKDTree(others).query(points, distance_upper_bound=tolerance)
    return np.isfinite(distance)


def weld(points):
    """Index of one representative per bit-identical position, for every vertex."""
    keys = np.ascontiguousarray(np.asarray(points, dtype=np.float64)).view(np.dtype((np.void, 24))).ravel()
    _, first, inverse = np.unique(keys, return_index=True, return_inverse=True)
    return first[inverse.ravel()]


def surface_distance(points, faces, sources):
    """Shortest edge-path length to the nearest source over welded connectivity. A part with no path to a source is
    reached across the smallest gap to an already reached vertex, which is where it is sewn on."""
    points = np.asarray(points, float)
    rep = weld(points)
    every = np.unique(np.asarray(faces))
    faces = rep[np.asarray(faces)]
    used = np.unique(faces)
    graph = {int(v): [] for v in used}
    for a, b in np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]):
        if a != b:
            length = float(np.linalg.norm(points[a] - points[b]))
            graph[int(a)].append((int(b), length))
            graph[int(b)].append((int(a), length))
    dist = {int(v): np.inf for v in used}
    queue = []
    for s_ in np.unique(np.asarray(sources)):
        v = int(rep[s_])
        if v in dist and dist[v] > 0:
            dist[v] = 0.0
            queue.append((0.0, v))
    heapq.heapify(queue)
    while True:
        while queue:
            d, u = heapq.heappop(queue)
            if d > dist[u]:
                continue
            for v, w in graph[u]:
                if d + w < dist[v]:
                    dist[v] = d + w
                    heapq.heappush(queue, (d + w, v))
        reached = np.array([v for v in used if np.isfinite(dist[v])])
        rest = np.array([v for v in used if not np.isfinite(dist[v])])
        if not len(rest):
            break
        gap, nearest = cKDTree(points[reached]).query(points[rest])
        i = int(np.argmin(gap))
        dist[int(rest[i])] = dist[int(reached[nearest[i]])] + float(gap[i])
        queue = [(dist[int(rest[i])], int(rest[i]))]
    out = np.full(len(points), np.nan)
    out[every] = [dist[int(v)] for v in rep[every]]
    return out


def covered(points, normals, cover_points, cover_faces, max_distance):
    """Vertices whose line along their normal, either way, meets the cover surface within max_distance; either way
    because a solidified shell's inner side faces the body while the cover is still outside it."""
    points, normals = np.asarray(points, float), np.asarray(normals, float)
    tri = np.asarray(cover_points, float)[np.asarray(cover_faces)]
    a, e1, e2 = tri[:, 0], tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]
    hit = np.zeros(len(points), bool)
    for i, (o, d) in enumerate(zip(points, normals)):
        length = np.linalg.norm(d)
        if length == 0:
            continue
        d = d / length
        h = np.cross(d, e2)
        det = np.einsum("ij,ij->i", e1, h)
        ok = np.abs(det) > 1e-12
        inv = np.where(ok, 1 / np.where(ok, det, 1), 0)
        s_ = o - a
        u = np.einsum("ij,ij->i", s_, h) * inv
        q = np.cross(s_, e1)
        v = (q @ d) * inv
        t = np.einsum("ij,ij->i", e2, q) * inv
        hit[i] = np.any(ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (np.abs(t) <= max_distance))
    return hit


def baseline_freedom(points, faces, seam_fraction, curve=None, pinned=None):
    """Freedom per vertex used by `faces` (NaN elsewhere).

    faces: the piece's sheet, every triangle with a corner on the piece, so the seam side is part of it.
    seam_fraction: share of the sheet's height, measured from its top, that is pinned.
    pinned: further pins, such as covered or coincident vertices.
    curve: (start, end, reach) drive weights at surface distance 0 and at `reach` of the largest distance,
    linear between and clamped; None means an undriven piece (freedom 1).
    """
    points, faces = np.asarray(points, float), np.asarray(faces)
    if not 0 < seam_fraction < 1:
        raise ValueError("seam_fraction must lie in (0, 1)")
    used = np.unique(faces)
    y = points[used, 1]
    height = (y.max() - y) / max(np.ptp(y), 1e-12)
    pins = used[height <= seam_fraction]
    if pinned is not None:
        pins = np.union1d(pins, np.asarray(pinned, int))
    if len(np.intersect1d(pins, used)) < 2:
        raise ValueError("the seam band holds fewer than two vertices")
    freedom = np.full(len(points), np.nan)
    if curve is None:
        freedom[used] = 1.0
    else:
        start, end, reach = (float(v) for v in curve)
        if not 0 < reach <= 1 or not 0 <= min(start, end) <= max(start, end) <= 1:
            raise ValueError("curve must hold weights in [0, 1] and a reach in (0, 1]")
        distance = surface_distance(points, faces, np.intersect1d(pins, used))
        depth = distance[used] / max(np.nanmax(distance[used]), 1e-12)
        weight = np.clip(np.interp(depth, [0.0, reach], [start, end]), 0.0, 1.0)
        freedom[used] = np.maximum(1.0 - np.sqrt(weight), FREE_FLOOR)
    freedom[pins] = 0.0
    return freedom
