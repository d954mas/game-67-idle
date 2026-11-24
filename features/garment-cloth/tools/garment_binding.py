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


def render_owners(positions, faces, take, corners):
    owners = np.full(len(positions), -1, dtype=int)
    if not corners:
        return owners
    keys = [tuple(p) for p in np.round(positions, 7)]
    active_faces = faces[take[faces].any(axis=1)]
    for face in active_faces:
        ordered = sorted(face, key=keys.__getitem__)
        values = corners.get(tuple(keys[v] for v in ordered))
        if values is not None:
            for v, owner in zip(ordered, values):
                owners[v] = owner if owners[v] in (-1, owner) else -2
    # Hem and UV splits duplicate the same outer corner with different render normals.
    coincident = {}
    for vi in np.flatnonzero(owners >= 0):
        key = keys[vi]
        if coincident.setdefault(key, owners[vi]) != owners[vi]:
            coincident[key] = -2
    # A short thickness edge connects the lining at a cut to its own outer lip.
    lining = {}
    for triangle in active_faces:
        for v, u in zip(triangle, np.roll(triangle, 1)):
            ka, kb = keys[v], keys[u]
            if kb in coincident:
                ka, kb = kb, ka
            if ka in coincident and kb not in coincident and sum((x-y)**2 for x,y in zip(ka,kb)) < .008**2:
                owner = coincident[ka]
                if lining.setdefault(kb, owner) != owner:
                    lining[kb] = -2
    for vi in np.flatnonzero(take & (owners == -1)):
        owners[vi] = coincident.get(keys[vi], lining.get(keys[vi], -1))
    return owners


def surface_rows(proxy, triangles, positions, faces, take, corners, distance):
    owners = render_owners(positions, faces, take, corners)
    unique, inverse = np.unique(np.column_stack((positions[take], owners[take])), axis=0, return_inverse=True)
    rows = bind_vertex_frames(proxy, triangles, unique[:, :3], distance)
    if corners:
        incident = [[] for _ in proxy]
        for index, face in enumerate(triangles):
            for v in face:
                incident[v].append(index)
        for i, (target, row) in enumerate(zip(unique, rows)):
            owner = int(target[3])
            if owner < 0:
                continue
            # Nearby folds can be meters apart along the sheet. Keep projections on the owner's patch.
            patch = incident[owner]
            if not np.isin(row[:3], triangles[patch]).any():
                rows[i], = bind_vertex_frames(proxy, triangles, [target[:3]], distance, face_candidates=[patch])
    return inverse, rows


BINDING_DISTANCE_M = 0.16
LINING_GAP_M = 0.008  # solidified shells closer than this are one fabric layer
PATH_STRETCH = 2 / np.sqrt(3)  # longest path along the sides of equilateral triangles per unit of straight line


def torn_edges(positions, faces, take, corners, proxy, triangles):
    """Render edges whose two ends bind to proxy places farther apart along the proxy than the sheet allows.

    corners: per render vertex the three proxy vertices of the triangle it binds to (None where the piece does not
    take the vertex). A proxy path from a corner at one end to a corner at the other needs at most the edge's length
    stretched by PATH_STRETCH, plus the longest side of each end's triangle to reach a corner from the bound place. A
    longer path runs around an opening of the sheet, and the render edge would stretch across it once it opens.
    """
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import dijkstra
    tri = np.asarray(triangles)
    pairs = np.unique(np.sort(np.concatenate([tri[:, [0, 1]], tri[:, [1, 2]], tri[:, [2, 0]]]), axis=1), axis=0)
    lengths = np.linalg.norm(proxy[pairs[:, 0]] - proxy[pairs[:, 1]], axis=1)
    graph = coo_matrix((lengths, (pairs[:, 0], pairs[:, 1])), shape=(len(proxy),) * 2).tocsr()
    edges = np.unique(np.sort(np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]), axis=1), axis=0)
    edges = edges[take[edges].all(1)]
    if not len(edges):
        return edges
    side = {}
    for v in np.unique(edges):
        c = proxy[list(corners[v])]
        side[v] = max(np.linalg.norm(c[i] - c[(i + 1) % 3]) for i in range(3))
    span = np.linalg.norm(positions[edges[:, 0]] - positions[edges[:, 1]], axis=1)
    allowed = PATH_STRETCH * span + np.array([side[a] + side[b] for a, b in edges])
    sources = sorted({u for a in np.unique(edges) for u in corners[a]})
    index = {u: i for i, u in enumerate(sources)}
    distance = dijkstra(graph, directed=False, indices=sources, limit=float(allowed.max()) * 1.01)
    path = np.array([min(distance[index[u], v] for u in corners[a] for v in corners[b]) for a, b in edges])
    return edges[path > allowed]


def bind_render(positions, faces, normals, labels, freedom, pieces, palette_offsets):
    """The 4 x uint16 `_GARMENT` stream of one render primitive.

    pieces: per piece (label, proxy points, proxy triangles, proxy corners or None, proxy pin mask); the corners
    keep a render vertex on its own side of a cut and apply to the primitive the proxy was built from.
    A render pin (freedom 0) is skinned. A free vertex follows the cloth by the bound proxy triangle's interpolated
    "not a pin" indicator, so it is skinned where every bound proxy vertex is a pin and fully simulated where none is. Inner-lining vertices take
    their outer shell partner's binding, which keeps a solidified shell's thickness through any fold. A render edge
    bound across an opening of the proxy fails the cook (`torn_edges`).
    """
    from scipy.spatial import cKDTree
    stream = np.zeros((len(positions), 4), dtype="<u2")
    amount = np.zeros(len(positions))
    for (label, proxy, triangles, corners, pins), offset in zip(pieces, palette_offsets):
        take = labels == label
        if not take.any():
            continue
        inverse, rows = surface_rows(proxy, triangles, positions, faces, take, corners, BINDING_DISTANCE_M)
        rows = list(rows)
        take_idx = np.flatnonzero(take)
        if normals is not None:
            a, b, c = proxy[triangles].transpose(1, 0, 2)
            p_vn = np.zeros_like(proxy)
            for col in triangles.T:
                np.add.at(p_vn, col, np.cross(b - a, c - a))
            p_vn /= np.maximum(np.linalg.norm(p_vn, axis=1, keepdims=True), 1e-12)
            bound_normal = np.array([sum(w * p_vn[v] for v, w in zip(rows[int(u)][:3], rows[int(u)][3:])) for u in inverse])
            dots = np.einsum("ij,ij->i", normals[take_idx], bound_normal)
            outer, inner = np.flatnonzero(dots > 0.15), np.flatnonzero(dots < -0.15)
            if len(outer) and len(inner):
                gap, nearest = cKDTree(positions[take_idx[outer]]).query(positions[take_idx[inner]], k=1)
                partner = outer[nearest]
                opposite = np.einsum("ij,ij->i", normals[take_idx[inner]], normals[take_idx[partner]]) < -0.5
                paired = (gap < LINING_GAP_M) & opposite
                inverse = inverse.copy()
                inverse[inner[paired]] = inverse[partner[paired]]
        free = ~np.asarray(pins, bool)
        for local, vertex in enumerate(take_idx):
            v0, v1, v2, w0, w1, w2 = rows[int(inverse[local])]
            amount[vertex] = (w0 * free[v0] + w1 * free[v1] + w2 * free[v2]) if freedom[vertex] > 0 else 0.0
            stream[vertex] = pack_vertex_binding_u16x4(offset + v0 + 1, offset + v1 + 1, offset + v2 + 1,
                                                       w0, w1, w2, amount[vertex])
        bound_corners = [None] * len(positions)
        for local, vertex in enumerate(take_idx):
            bound_corners[vertex] = rows[int(inverse[local])][:3]
        torn = torn_edges(positions, faces, take, bound_corners, proxy, triangles)
        if len(torn):
            raise ValueError(f"piece {label}: {len(torn)} render edges bind across an opening of the proxy, "
                             f"around {np.round(positions[torn[0]].mean(0), 3).tolist()}")
    return stream, amount
