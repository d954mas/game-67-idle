"""Bind render positions to an oriented triangle proxy in the same Y-up meter space."""
import numpy as np


def fold_rest_agreements(points, faces, bindings):
    """Reference agreement between each support face and its local rest surface."""
    p, triangles = np.asarray(points, dtype=float), np.asarray(faces)
    if (p.ndim != 2 or p.shape[1] != 3 or not np.isfinite(p).all()
            or triangles.ndim != 2 or triangles.shape[1] != 3 or not len(triangles)
            or not np.issubdtype(triangles.dtype, np.integer)
            or np.any(triangles < 0) or np.any(triangles >= len(p))):
        raise ValueError("invalid rest proxy")
    a, b, c = p[triangles].transpose(1, 0, 2)
    ab, ac, bc = b-a, c-a, c-b
    area = np.cross(ab, ac)
    area2 = np.einsum("ij,ij->i", area, area)
    span2 = np.maximum.reduce([np.einsum("ij,ij->i", edge, edge) for edge in (ab, ac, bc)])
    if not np.isfinite(area2).all() or not np.isfinite(span2).all():
        raise ValueError("invalid rest binding triangle")
    collapsed = area2 <= 1e-14 * span2**2
    area[collapsed] = 0
    lengths = np.sqrt(np.where(collapsed, 0, area2))
    fn = np.zeros_like(area)
    np.divide(area, lengths[:, None], out=fn, where=~collapsed[:, None])
    vn = np.zeros_like(p)
    incident_areas = np.zeros(len(p))
    for indices in triangles.T:
        np.add.at(vn, indices, area)
        np.add.at(incident_areas, indices, lengths)
    np.divide(vn, incident_areas[:, None], out=vn, where=incident_areas[:, None] > 0)

    rows = np.asarray(bindings, dtype=float)
    if rows.shape in ((0,), (0, 4)):
        return np.empty(0)
    if (rows.ndim != 2 or rows.shape[1] != 4 or not np.isfinite(rows).all()
            or np.any(rows[:, 0] != np.floor(rows[:, 0]))
            or np.any(rows[:, 0] < 0) or np.any(rows[:, 0] >= len(triangles))):
        raise ValueError("invalid render binding")
    face = rows[:, 0].astype(int)
    u, v = rows[:, 1], rows[:, 2]
    bary = np.maximum(np.stack((1-u-v, u, v), axis=1), 0)
    smooth = np.einsum("ij,ijk->ik", bary, vn[triangles[face]])
    dot = np.einsum("ij,ij->i", smooth, fn[face])
    return np.clip(dot / bary.sum(axis=1), -1, 1)


def bind_surface(points, faces, render_points, max_distance, *, face_candidates=None):
    """Optional face index lists per render vertex disambiguate coincident seam lips or garment layers."""
    p, target = np.asarray(points, dtype=float), np.asarray(render_points, dtype=float)
    indices = np.asarray(faces)
    if (p.ndim != 2 or p.shape[1] != 3 or target.ndim != 2 or target.shape[1] != 3
            or indices.ndim != 2 or indices.shape[1] != 3 or not len(indices)
            or not np.issubdtype(indices.dtype, np.integer) or np.any(indices < 0) or np.any(indices >= len(p))
            or not np.isfinite(p).all() or not np.isfinite(target).all()
            or not np.isfinite(max_distance) or max_distance < 0):
        raise ValueError("invalid surface binding geometry or distance")
    a, b, c = p[indices].transpose(1, 0, 2)
    ab, ac = b - a, c - a
    n = np.cross(ab, ac)
    area2 = np.linalg.norm(n, axis=1)
    if np.min(area2) <= 1e-10:
        raise ValueError("degenerate binding triangle")
    n /= area2[:, None]
    if face_candidates is not None and len(face_candidates) != len(target):
        raise ValueError("expected one candidate face list per render vertex")
    rows = []
    for vertex, x in enumerate(target):
        ax = x - a
        height = np.einsum("ij,ij->i", ax, n)
        u = np.einsum("ij,ij->i", np.cross(ax, ac), n) / area2
        v = np.einsum("ij,ij->i", np.cross(ab, ax), n) / area2
        d2 = np.where((u >= 0) & (v >= 0) & (u + v <= 1), height**2, np.inf)
        for start, end in ((a,b), (b,c), (c,a)):
            edge = end - start
            t = np.clip(np.einsum("ij,ij->i", x-start, edge) / np.einsum("ij,ij->i", edge, edge), 0, 1)
            delta = x - (start + edge * t[:, None])
            d2 = np.minimum(d2, np.einsum("ij,ij->i", delta, delta))
        if face_candidates is None:
            face = int(np.argmin(d2))
        else:
            candidates = np.asarray(face_candidates[vertex])
            if (candidates.ndim != 1 or not len(candidates) or not np.issubdtype(candidates.dtype, np.integer)
                    or np.any(candidates < 0) or np.any(candidates >= len(indices))):
                raise ValueError(f"invalid binding face candidates for render vertex {vertex}")
            face = int(candidates[np.argmin(d2[candidates])])
        if d2[face] > max_distance**2 + 1e-16:
            raise ValueError(f"render vertex {vertex} exceeds binding distance {max_distance:g} m")
        rows.append((face, float(u[face]), float(v[face]), float(height[face])))
    return rows
