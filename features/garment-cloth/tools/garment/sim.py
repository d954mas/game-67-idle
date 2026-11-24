"""Geometry and topology of a sim mesh held as numpy arrays: points (V, 3) in rest model space, Y-up metres, and
counter-clockwise triangles (F, 3) whose front faces away from the body.

A topology edit returns new points and triangles and a sparse matrix P (V_new x V_old) that says where every new
vertex came from, so per-vertex data follows it: new = P @ old. A row of P sums to 1."""
import heapq

import numpy as np
from scipy.sparse import coo_matrix, csr_matrix, identity
from scipy.sparse.csgraph import connected_components, dijkstra
from scipy.spatial import cKDTree



EDGE_WARP, EDGE_WEFT, EDGE_DIAG = 0, 1, 2

def _closest_on_triangles(p, a, b, c):
    """Ericson's closest point on each triangle row; returns the point and its barycentric coordinates."""
    ab, ac, ap = b - a, c - a, p - a
    d1, d2 = np.einsum("ij,ij->i", ab, ap), np.einsum("ij,ij->i", ac, ap)
    bp = p - b
    d3, d4 = np.einsum("ij,ij->i", ab, bp), np.einsum("ij,ij->i", ac, bp)
    cp = p - c
    d5, d6 = np.einsum("ij,ij->i", ab, cp), np.einsum("ij,ij->i", ac, cp)
    va, vb, vc = d3 * d6 - d5 * d4, d5 * d2 - d1 * d6, d1 * d4 - d3 * d2
    with np.errstate(divide="ignore", invalid="ignore"):
        denom = va + vb + vc
        v, w = vb / denom, vc / denom
        bary = np.column_stack([1 - v - w, v, w])
        t_ab, t_ac = d1 / (d1 - d3), d2 / (d2 - d6)
        t_bc = (d4 - d3) / ((d4 - d3) + (d5 - d6))
    cases = [((d1 <= 0) & (d2 <= 0), np.array([1, 0, 0])), ((d3 >= 0) & (d4 <= d3), np.array([0, 1, 0])),
             ((d6 >= 0) & (d5 <= d6), np.array([0, 0, 1]))]
    out = bary.copy()
    edge = [((vc <= 0) & (d1 >= 0) & (d3 <= 0), lambda t: np.column_stack([1 - t, t, 0 * t]), t_ab),
            ((vb <= 0) & (d2 >= 0) & (d6 <= 0), lambda t: np.column_stack([1 - t, 0 * t, t]), t_ac),
            ((va <= 0) & (d4 - d3 >= 0) & (d5 - d6 >= 0), lambda t: np.column_stack([0 * t, 1 - t, t]), t_bc)]
    done = np.zeros(len(p), bool)
    for mask, corner in cases:
        m = mask & ~done
        out[m] = corner
        done |= m
    for mask, make, t in edge:
        m = mask & ~done
        if m.any():
            out[m] = make(t[m])
        done |= m
    point = out[:, :1] * a + out[:, 1:2] * b + out[:, 2:] * c
    return point, out


def closest_surface_points(points, vertices, faces):
    """Exact closest point on a triangle mesh: (face, barycentrics, distance) per query point."""
    points, vertices, faces = np.asarray(points, float), np.asarray(vertices, float), np.asarray(faces)
    tri = vertices[faces]
    centers = tri.mean(1)
    radius = np.linalg.norm(tri - centers[:, None], axis=2).max(1)
    tree = cKDTree(centers)
    _, near = tree.query(points, k=min(8, len(faces)))
    near = near.reshape(len(points), -1)
    best_face = np.zeros(len(points), int)
    best_bary = np.zeros((len(points), 3))
    best = np.full(len(points), np.inf)

    def consider(rows, cand):
        nonlocal best
        q, bary = _closest_on_triangles(points[rows], tri[cand, 0], tri[cand, 1], tri[cand, 2])
        d = np.linalg.norm(points[rows] - q, axis=1)
        better = d < best[rows]
        r = rows[better]
        best[r], best_face[r], best_bary[r] = d[better], cand[better], bary[better]

    every = np.arange(len(points))
    for k in range(near.shape[1]):
        consider(every, near[:, k])
    # A triangle can be closer than its centroid suggests: test every one whose bounding sphere reaches the bound.
    rmax = radius.max()
    for i, cand in enumerate(tree.query_ball_point(points, best + rmax)):
        cand = np.asarray(cand, int)
        cand = cand[np.linalg.norm(centers[cand] - points[i], axis=1) - radius[cand] <= best[i]]
        if len(cand):
            consider(np.full(len(cand), i), cand)
    return best_face, best_bary, best


def geodesic_depth(points, faces, pins):
    """Return (distance_m, normalized_01) shortest edge-path distance from pinned seam vertices."""
    pts = np.asarray(points, dtype=np.float64)
    if pts.ndim != 2 or pts.shape[1] != 3 or not np.isfinite(pts).all():
        raise ValueError("invalid proxy vertex coordinates")
    pin_set = {int(p) for p in pins}
    if not pin_set:
        raise ValueError("geodesic_depth requires at least one pinned vertex")
    edges = _unique_edges(faces)
    graph = [[] for _ in range(len(pts))]
    for a, b in edges:
        length = float(np.linalg.norm(pts[b] - pts[a]))
        graph[a].append((b, length))
        graph[b].append((a, length))
    dist = np.full(len(pts), np.inf, dtype=np.float64)
    queue = []
    for p in pin_set:
        if not 0 <= p < len(pts):
            raise ValueError(f"pin index {p} out of range")
        dist[p] = 0.0
        heapq.heappush(queue, (0.0, p))
    while queue:
        d, u = heapq.heappop(queue)
        if d > dist[u]:
            continue
        for v, w in graph[u]:
            nd = d + w
            if nd < dist[v]:
                dist[v] = nd
                heapq.heappush(queue, (nd, v))
    if not np.isfinite(dist).all():
        raise ValueError("disconnected proxy island has no pinned seam vertex")
    span = float(dist.max())
    norm = dist / span if span > 1e-12 else np.zeros_like(dist)
    return dist, norm


def _unique_edges(faces):
    f = np.asarray(faces, dtype=np.int64)
    pairs = np.concatenate([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]], axis=0)
    return np.unique(np.sort(pairs, axis=1), axis=0)


def classify_edges(points, faces, pins, *, uv=None):
    """Classify each undirected edge (min(a,b), max(a,b)) into WARP (0), WEFT (1), or DIAG (2)."""
    pts = np.asarray(points, dtype=np.float64)
    edges = _unique_edges(faces)
    kinds = {}
    if uv is not None:
        coords = np.asarray(uv, dtype=np.float64)
        if coords.shape != (len(pts), 2) or not np.isfinite(coords).all():
            raise ValueError("uv must be finite (N, 2)")
        for a, b in edges:
            du = abs(float(coords[b, 0] - coords[a, 0]))
            dv = abs(float(coords[b, 1] - coords[a, 1]))
            if dv >= 2.0 * du and dv > 1e-9:
                kinds[(int(a), int(b))] = EDGE_WARP
            elif du >= 2.0 * dv and du > 1e-9:
                kinds[(int(a), int(b))] = EDGE_WEFT
            else:
                kinds[(int(a), int(b))] = EDGE_DIAG
        return kinds

    dist_m, _ = geodesic_depth(pts, faces, pins)
    for a, b in edges:
        length = float(np.linalg.norm(pts[b] - pts[a]))
        slope = abs(float(dist_m[b] - dist_m[a])) / max(length, 1e-12)
        if slope >= 0.72:
            kinds[(int(a), int(b))] = EDGE_WARP
        elif slope <= 0.28:
            kinds[(int(a), int(b))] = EDGE_WEFT
        else:
            kinds[(int(a), int(b))] = EDGE_DIAG
    return kinds


def edges(faces):
    """Unique undirected edges (E, 2), lower index first."""
    f = np.asarray(faces, np.int64).reshape(-1, 3)
    return np.unique(np.sort(np.concatenate([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]]), axis=1), axis=0)


def edge_graph(points, faces):
    """Symmetric sparse graph of edge lengths."""
    e = edges(faces)
    length = np.linalg.norm(points[e[:, 0]] - points[e[:, 1]], axis=1)
    n = len(points)
    return csr_matrix((np.r_[length, length], (np.r_[e[:, 0], e[:, 1]], np.r_[e[:, 1], e[:, 0]])), shape=(n, n))


def geodesic(points, faces, sources, limit=np.inf):
    """Shortest edge-path length from the nearest source vertex; inf where no path reaches. Paths follow edges, so
    they never cross an open boundary or a cut."""
    sources = np.flatnonzero(np.asarray(sources)) if np.asarray(sources).dtype == bool else np.asarray(sources, int)
    if not len(sources):
        return np.full(len(points), np.inf)
    return dijkstra(edge_graph(points, faces), indices=sources, min_only=True, limit=limit)


def dijkstra_all(points, faces, limit=np.inf):
    """(V, V) shortest edge-path lengths between all vertices, inf beyond limit."""
    return dijkstra(edge_graph(points, faces), directed=False, limit=limit)


def neighbours(faces, n):
    """Vertex adjacency as a sparse boolean matrix (n, n)."""
    e = edges(faces)
    return csr_matrix((np.ones(2 * len(e), bool), (np.r_[e[:, 0], e[:, 1]], np.r_[e[:, 1], e[:, 0]])), shape=(n, n))


def components(faces, n):
    """Connected component id per vertex."""
    return connected_components(neighbours(faces, n), directed=False)[1]


def boundary_edges(faces):
    f = np.asarray(faces, np.int64).reshape(-1, 3)
    pairs = np.sort(np.concatenate([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]]), axis=1)
    e, count = np.unique(pairs, axis=0, return_counts=True)
    return e[count == 1]


def boundary(faces, n):
    """Mask of vertices on an open boundary (a hem, a slit lip, a cut)."""
    out = np.zeros(n, bool)
    out[boundary_edges(faces).ravel()] = True
    return out


def face_normals(points, faces):
    a, b, c = points[faces].transpose(1, 0, 2)
    return np.cross(b - a, c - a)


def vertex_normals(points, faces):
    """Area-weighted unit normals, on the front side of the triangles."""
    n = np.zeros_like(points, dtype=float)
    fn = face_normals(points, faces)
    for k in range(3):
        np.add.at(n, faces[:, k], fn)
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)


def closest(points, surface_points, surface_faces):
    """(face, barycentrics, distance) of the closest surface point to each point."""
    return closest_surface_points(np.asarray(points, float).reshape(-1, 3), surface_points, surface_faces)


def ray_hits(origins, directions, points, faces, max_t=np.inf):
    """First hit of each ray on the triangles, either side: (t, face, barycentrics); t = inf where none within max_t."""
    origins, directions = np.asarray(origins, float).reshape(-1, 3), np.asarray(directions, float).reshape(-1, 3)
    a, b, c = points[faces].transpose(1, 0, 2)
    e1, e2 = b - a, c - a
    best_t = np.full(len(origins), np.inf)
    best_f = np.full(len(origins), -1)
    best_b = np.zeros((len(origins), 3))
    for i, (o, d) in enumerate(zip(origins, directions)):
        p = np.cross(d, e2)
        det = np.einsum("ij,ij->i", e1, p)
        ok = np.abs(det) > 1e-14
        inv = np.where(ok, 1 / np.where(ok, det, 1), 0)
        s = o - a
        u = np.einsum("ij,ij->i", s, p) * inv
        q = np.cross(s, e1)
        v = (q @ d) * inv
        t = np.einsum("ij,ij->i", e2, q) * inv
        hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-9) & (t <= max_t)
        if hit.any():
            k = np.flatnonzero(hit)[np.argmin(t[hit])]
            best_t[i], best_f[i], best_b[i] = t[k], k, (1 - u[k] - v[k], u[k], v[k])
    return best_t, best_f, best_b


def shortest_path(points, faces, start, end):
    """Vertex ids of the shortest edge path from start to end, both included."""
    _, previous = dijkstra(edge_graph(points, faces), indices=int(start), return_predecessors=True)
    if previous[int(end)] < 0 and int(end) != int(start):
        raise ValueError(f"no edge path from vertex {start} to vertex {end}")
    path = [int(end)]
    while path[-1] != int(start):
        path.append(int(previous[path[-1]]))
    return path[::-1]


def _rows(sources, n_old):
    """P from a list of (weights by old vertex) dicts."""
    r, c, v = [], [], []
    for i, row in enumerate(sources):
        for j, w in row.items():
            r.append(i)
            c.append(j)
            v.append(w)
    return coo_matrix((v, (r, c)), shape=(len(sources), n_old)).tocsr()


def cut(points, faces, path):
    """Open the mesh along a path of vertices joined by edges: every path vertex whose fan the path splits gets one
    copy per extra side, so the triangles on each side keep their own vertex. A path that ends inside the mesh leaves
    its end vertex whole (the tip of a slit)."""
    path = [int(v) for v in path]
    known = {tuple(e) for e in edges(faces).tolist()}
    cuts = {tuple(sorted(pair)) for pair in zip(path, path[1:])}
    missing = sorted(cuts - known)
    if missing:
        raise ValueError(f"cut path steps are not mesh edges: {missing[:4]}")
    faces = np.asarray(faces, np.int64).copy()
    new_faces = faces.copy()
    sources = [{i: 1.0} for i in range(len(points))]
    extra = []
    for v in dict.fromkeys(path):
        fan = np.flatnonzero((faces == v).any(1))
        parent = {f: f for f in fan}

        def root(f):
            while parent[f] != f:
                parent[f] = parent[parent[f]]
                f = parent[f]
            return f
        by_edge = {}
        for f in fan:
            for u in faces[f]:
                if u != v and tuple(sorted((v, int(u)))) not in cuts:
                    by_edge.setdefault(int(u), []).append(f)
        for group in by_edge.values():
            for f in group[1:]:
                parent[root(f)] = root(group[0])
        groups = {}
        for f in fan:
            groups.setdefault(root(f), []).append(f)
        for group in sorted(groups.values(), key=min)[1:]:
            copy = len(points) + len(extra)
            extra.append(v)
            sources.append({v: 1.0})
            for f in group:
                new_faces[f][faces[f] == v] = copy
    out = np.concatenate([points, points[extra]]) if extra else points.copy()
    return out, new_faces, _rows(sources, len(points))


def delete(points, faces, mask):
    """Remove the masked vertices, their triangles and any vertex left without a triangle."""
    mask = np.asarray(mask, bool)
    keep_faces = faces[~mask[faces].any(1)]
    used = np.unique(keep_faces)
    index = np.full(len(points), -1)
    index[used] = np.arange(len(used))
    P = coo_matrix((np.ones(len(used)), (np.arange(len(used)), used)), shape=(len(used), len(points))).tocsr()
    return points[used], index[keep_faces], P


def _split_once(points, faces, mask, longest):
    """Split, longest first, every masked edge longer than `longest` whose triangles no other split in this pass
    touches. Returns None when none was split."""
    e = edges(faces)
    length = np.linalg.norm(points[e[:, 0]] - points[e[:, 1]], axis=1)
    order = [i for i in np.argsort(-length, kind="stable") if length[i] > longest and mask[e[i]].all()]
    if not order:
        return None
    incident = {}
    for f, tri in enumerate(faces):
        for k in range(3):
            incident.setdefault(tuple(sorted((int(tri[k]), int(tri[(k + 1) % 3])))), []).append(f)
    taken, splits = set(), []
    for i in order:
        key = (int(e[i, 0]), int(e[i, 1]))
        if taken.isdisjoint(incident[key]):
            taken.update(incident[key])
            splits.append(key)
    points = list(points)
    sources = [{i: 1.0} for i in range(len(points))]
    out = [list(t) for t in faces]
    for a, b in splits:
        m = len(points)
        points.append(0.5 * (points[a] + points[b]))
        sources.append({a: 0.5, b: 0.5})
        for f in incident[(a, b)]:
            tri = out[f]
            k = next(k for k in range(3) if {tri[k], tri[(k + 1) % 3]} == {a, b})
            x, y, z = tri[k], tri[(k + 1) % 3], tri[(k + 2) % 3]
            out[f] = [x, m, z]
            out.append([m, y, z])
    return np.asarray(points), np.asarray(out, np.int64), _rows(sources, len(sources) - len(splits))


def _collapse_once(points, faces, mask, shortest, longest):
    """Collapse, shortest first, masked edges shorter than `shortest` where the link condition holds, no triangle
    flips or degenerates and no edge grows past `longest`; a boundary vertex keeps its place, and an interior edge
    between two boundary vertices stays. Returns None when none was collapsed."""
    e = edges(faces)
    length = np.linalg.norm(points[e[:, 0]] - points[e[:, 1]], axis=1)
    order = [i for i in np.argsort(length, kind="stable") if length[i] < shortest and mask[e[i]].all()]
    if not order:
        return None
    on_boundary = boundary(faces, len(points))
    border = {tuple(b) for b in boundary_edges(faces).tolist()}
    nbr = [set() for _ in points]
    fans = [set() for _ in points]
    for f, (a, b, c) in enumerate(faces):
        for x, y, z in ((a, b, c), (b, c, a), (c, a, b)):
            nbr[x].update((int(y), int(z)))
            fans[x].add(f)
    fn = face_normals(points, faces)
    busy, merge, place = set(), {}, {}
    for i in order:
        x, y = int(e[i, 0]), int(e[i, 1])
        if x in busy or y in busy:
            continue
        shared = len(fans[x] & fans[y])
        if len(nbr[x] & nbr[y]) != shared:
            continue
        if on_boundary[x] and on_boundary[y] and (x, y) not in border:
            continue
        target = points[x] if on_boundary[x] and not on_boundary[y] else points[y] if on_boundary[y] and not on_boundary[x] \
            else 0.5 * (points[x] + points[y])
        affected = sorted((fans[x] | fans[y]) - (fans[x] & fans[y]))
        tri = faces[affected].copy()
        pos = points[tri].copy()
        pos[(tri == x) | (tri == y)] = target
        new = np.cross(pos[:, 1] - pos[:, 0], pos[:, 2] - pos[:, 0])
        size = np.linalg.norm(new, axis=1)
        old = fn[affected]
        if np.any(size < 2e-8) or np.any(np.einsum("ij,ij->i", new, old) <= 0.1 * size * np.linalg.norm(old, axis=1)):
            continue
        if max(np.linalg.norm(points[u] - target) for u in (nbr[x] | nbr[y]) - {x, y}) > longest:
            continue
        merge[y] = x
        place[x] = target
        busy.update(nbr[x] | nbr[y] | {x, y})
    if not merge:
        return None
    keep = np.array([i for i in range(len(points)) if i not in merge])
    index = np.full(len(points), -1)
    index[keep] = np.arange(len(keep))
    for y, x in merge.items():
        index[y] = index[x]
    new_points = points[keep].copy()
    sources = [{int(i): 1.0} for i in keep]
    for y, x in merge.items():
        new_points[index[x]] = place[x]
        sources[index[x]] = {x: 0.5, y: 0.5}
    tri = index[faces]
    tri = tri[(tri[:, 0] != tri[:, 1]) & (tri[:, 1] != tri[:, 2]) & (tri[:, 2] != tri[:, 0])]
    return new_points, tri, _rows(sources, len(points))


def density(points, faces, mask, edge_m, limit=512):
    """Bring the masked region's edges toward edge_m: split edges longer than 4/3 of it and collapse edges shorter
    than 4/5 of it (the isotropic remeshing bounds of Botsch and Kobbelt), up to `limit` vertices."""
    mask = np.asarray(mask, bool)
    P = identity(len(points), format="csr")
    for _ in range(64):
        changed = False
        if len(points) < limit:
            step = _split_once(points, faces, mask, 4 / 3 * edge_m)
            if step is not None:
                points, faces, S = step
                mask = (S @ mask.astype(float)) > 0.5 - 1e-9
                P, changed = S @ P, True
        step = _collapse_once(points, faces, mask, 4 / 5 * edge_m, 4 / 3 * edge_m)
        if step is not None:
            points, faces, S = step
            mask = (S @ mask.astype(float)) > 0.5 - 1e-9
            P, changed = S @ P, True
        if not changed:
            break
    return points, faces, P.tocsr()


def relax(points, faces, mask, iterations=4):
    """Even out the masked interior vertices: each moves halfway to its neighbours' mean within its tangent plane and
    back onto the surface it started on. Boundary vertices stay, so a hem or a cut keeps its line."""
    source = np.asarray(points, float)
    x = source.copy()
    movable = np.asarray(mask, bool) & ~boundary(faces, len(x))
    adjacency = neighbours(faces, len(x)).astype(float)
    count = np.asarray(adjacency.sum(1)).ravel()
    for _ in range(iterations):
        mean = (adjacency @ x) / np.maximum(count, 1)[:, None]
        n = vertex_normals(x, faces)
        step = mean - x
        step -= np.einsum("ij,ij->i", step, n)[:, None] * n
        moved = x + 0.5 * step
        face, bary, _ = closest(moved[movable], source, faces)
        x[movable] = np.einsum("nk,nkj->nj", bary, source[faces[face]])
    return x


def offset(points, faces, mask, distance_m):
    """Move the masked vertices along their normals, outward (away from the body) for a positive distance (a number or
    one per vertex)."""
    x = np.asarray(points, float).copy()
    mask = np.asarray(mask, bool)
    distance = np.broadcast_to(np.asarray(distance_m, float), (len(x),))
    x[mask] += distance[mask, None] * vertex_normals(x, faces)[mask]
    return x
