"""Bind render positions to an oriented triangle proxy in the same Y-up meter space."""
import numpy as np


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


def bind_vertex_frames(points, faces, render_points, max_distance, *, face_candidates=None):
    """Bind each render vertex to the 3 vertices of its supporting proxy triangle with clamped barycentric weights.
    Because each triangle owns distinct indices across a slit, blending never crosses slit boundaries."""
    raw = bind_surface(points, faces, render_points, max_distance, face_candidates=face_candidates)
    tris = np.asarray(faces, dtype=np.int32)
    out = []
    for face, u, v, _ in raw:
        a, b, c = (int(x) for x in tris[face])
        # Project (u, v) to the triangle simplex [0, 1]^2, u + v <= 1 cleanly.
        w0, w1, w2 = 1.0 - u - v, u, v
        if w0 < 0.0 or w1 < 0.0 or w2 < 0.0:
            # Find closest point on triangle edges in barycentric space.
            w0, w1, w2 = max(0.0, w0), max(0.0, w1), max(0.0, w2)
            s = w0 + w1 + w2
            if s > 1e-12:
                w0, w1, w2 = w0 / s, w1 / s, w2 / s
            else:
                w0, w1, w2 = 1.0, 0.0, 0.0
        out.append((a, b, c, float(w0), float(w1), float(w2)))
    return out


def pack_vertex_binding_u16x4(v0, v1, v2, w0, w1, w2, amount=1.0):
    """Pack 3 1-based vertex IDs (11 bits each, 0..2047) and 3 weights (10 bits each, sum <= 1023)
    into 4 x uint16 with bit 31 of hi = 1 identifying the smooth 3-vertex format."""
    if not (0 <= v0 <= 2047 and 0 <= v1 <= 2047 and 0 <= v2 <= 2047):
        raise ValueError("vertex frame index exceeds 11-bit capacity (2047)")
    scale = float(np.clip(amount, 0.0, 1.0))
    total = int(round(scale * 1023.0))
    if total <= 0:
        return (0, 0, 0, 0)
    ws = np.array([max(0.0, float(w0)), max(0.0, float(w1)), max(0.0, float(w2))], dtype=np.float64)
    ws_sum = float(ws.sum())
    if ws_sum <= 1e-12:
        return (0, 0, 0, 0)
    raw = (ws / ws_sum) * total
    q = np.floor(raw).astype(int)
    rem = total - int(q.sum())
    if rem > 0:
        order = np.argsort(-(raw - q))
        for k in range(rem):
            q[order[k]] += 1
    q0, q1, q2 = int(q[0]), int(q[1]), int(q[2])
    if q0 == 0:
        v0 = 0
    if q1 == 0:
        v1 = 0
    if q2 == 0:
        v2 = 0
    lo = (v0 & 0x7FF) | ((v1 & 0x7FF) << 11) | ((q0 & 0x3FF) << 22)
    hi = (v2 & 0x7FF) | ((q1 & 0x3FF) << 11) | ((q2 & 0x3FF) << 21) | (1 << 31)
    return (lo & 0xFFFF, (lo >> 16) & 0xFFFF, hi & 0xFFFF, (hi >> 16) & 0xFFFF)


def pack_indexed_surface(vertex_count, pins, palette_offset=0):
    """Bind a render surface sharing the simulation indices. Pins keep ordinary skinning."""
    if not 0 < vertex_count <= 2047 or not 0 <= palette_offset <= 2047 - vertex_count:
        raise ValueError("indexed surface exceeds the vertex palette")
    movable = np.ones(vertex_count, dtype=bool)
    for pin in pins:
        if not isinstance(pin, (int, np.integer)) or isinstance(pin, bool) or not 0 <= pin < vertex_count:
            raise ValueError("invalid indexed surface pin")
        movable[pin] = False
    rows = np.zeros((vertex_count, 4), dtype="<u2")
    for index in np.flatnonzero(movable):
        vertex = palette_offset + int(index) + 1
        rows[index] = pack_vertex_binding_u16x4(vertex, vertex, vertex, 1., 0., 0.)
    return rows


def unpack_vertex_binding_u16x4(row):
    """Unpack a 4 x uint16 smooth 3-vertex binding row into (is_vertex_mode, (v0,v1,v2), (w0,w1,w2))."""
    g0, g1, g2, g3 = (int(x) for x in row)
    lo = g0 | (g1 << 16)
    hi = g2 | (g3 << 16)
    is_vmode = bool((hi >> 31) & 1)
    v0 = lo & 0x7FF
    v1 = (lo >> 11) & 0x7FF
    q0 = (lo >> 22) & 0x3FF
    v2 = hi & 0x7FF
    q1 = (hi >> 11) & 0x3FF
    q2 = (hi >> 21) & 0x3FF
    return is_vmode, (v0, v1, v2), (q0 / 1023.0, q1 / 1023.0, q2 / 1023.0)
