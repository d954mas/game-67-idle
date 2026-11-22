"""Topology-agnostic proxy utilities: checkerboard quad triangulation, geodesic depth, warp/weft classification and bind-pose checks."""
from __future__ import annotations

import heapq
import math

import numpy as np

EDGE_WARP = 0
EDGE_WEFT = 1
EDGE_DIAG = 2


def triangulate_quads_checkerboard(quads, *, grid_coords=None):
    """Split quad faces (i0, i1, i2, i3) into triangles with alternating diagonals to eliminate twist bias."""
    triangles = []
    for idx, q in enumerate(quads):
        if len(q) != 4:
            raise ValueError(f"quad {idx} must have 4 vertex indices")
        a, b, c, d = (int(v) for v in q)
        flip = ((grid_coords[idx][0] + grid_coords[idx][1]) % 2 != 0) if grid_coords is not None else (idx % 2 != 0)
        if not flip:
            triangles.append((a, b, c))
            triangles.append((a, c, d))
        else:
            triangles.append((a, b, d))
            triangles.append((b, c, d))
    return np.asarray(triangles, dtype=np.int32)


def _unique_edges(faces):
    f = np.asarray(faces, dtype=np.int64)
    pairs = np.concatenate([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]], axis=0)
    return np.unique(np.sort(pairs, axis=1), axis=0)


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


def round_cone_signed_distance(point, a, b, ra, rb):
    """Exact closed-form round-cone SDF matching contact.c (negative inside)."""
    p = np.asarray(point, dtype=np.float64)
    pa = p - a
    ba = b - a
    len2 = float(ba @ ba)
    dr = float(ra - rb)
    if len2 <= 1e-12 or dr * dr >= len2:
        return float(np.linalg.norm(p - a) - ra) if ra >= rb else float(np.linalg.norm(p - b) - rb)
    length = math.sqrt(len2)
    u = ba / length
    y = float(pa @ u)
    w = pa - y * u
    x = float(np.linalg.norm(w))
    side = math.sqrt(max(0.0, len2 - dr * dr))
    s = (-x * dr + y * side) / length
    if s <= 0.0:
        return float(np.linalg.norm(p - a) - ra)
    if s >= side:
        return float(np.linalg.norm(p - b) - rb)
    return (x * side + y * dr) / length - ra


def validate_proxy(points, faces, pins, *, max_edge_m=None, capsules=None, margin_m=0.0):
    """Validate proxy edge lengths and bind-pose clearance against tapered capsules."""
    pts = np.asarray(points, dtype=np.float64)
    edges = _unique_edges(faces)
    lengths = np.linalg.norm(pts[edges[:, 1]] - pts[edges[:, 0]], axis=1)
    min_edge = float(lengths.min())
    med_edge = float(np.median(lengths))
    max_edge = float(lengths.max())
    penetrations = []
    if capsules:
        pin_set = {int(p) for p in pins}
        for ci, cap in enumerate(capsules):
            a = np.asarray(cap["a"], dtype=np.float64)
            b = np.asarray(cap["b"], dtype=np.float64)
            ra = float(cap["radius"]) + margin_m
            rb = float(cap.get("radius_b", cap["radius"]) or cap["radius"]) + margin_m
            for vi, pt in enumerate(pts):
                sd = round_cone_signed_distance(pt, a, b, ra, rb)
                if sd < -1e-5:
                    penetrations.append({
                        "vertex": vi,
                        "pinned": vi in pin_set,
                        "capsule": ci,
                        "depth_mm": round(-sd * 1000.0, 3),
                    })
    ok_edge = max_edge_m is None or max_edge <= max_edge_m + 1e-9
    return {
        "vertices": int(len(pts)),
        "faces": int(len(faces)),
        "edges": int(len(edges)),
        "min_edge_mm": round(min_edge * 1000.0, 3),
        "median_edge_mm": round(med_edge * 1000.0, 3),
        "max_edge_mm": round(max_edge * 1000.0, 3),
        "edge_limit_ok": ok_edge,
        "bind_penetrations": penetrations,
        "pinned_penetrations": [p for p in penetrations if p["pinned"]],
    }


def subdivide_long_edges(points, faces, max_edge_m, *, max_vertices=512):
    """Bisect the longest triangle edges until all edges <= max_edge_m or vertex budget is reached."""
    if max_edge_m <= 0.0:
        raise ValueError("max_edge_m must be positive")
    pts = [np.asarray(p, dtype=np.float64) for p in points]
    tris = [list(map(int, t)) for t in faces]
    while len(pts) < max_vertices:
        best_e = None
        best_len2 = max_edge_m * max_edge_m
        edge_tris = {}
        for ti, (a, b, c) in enumerate(tris):
            for u, v in ((a, b), (b, c), (c, a)):
                key = (u, v) if u < v else (v, u)
                edge_tris.setdefault(key, []).append(ti)
        for key in edge_tris:
            u, v = key
            d = pts[v] - pts[u]
            len2 = float(d @ d)
            if len2 > best_len2:
                best_len2 = len2
                best_e = key
        if best_e is None:
            break
        u, v = best_e
        mid = len(pts)
        pts.append(0.5 * (pts[u] + pts[v]))
        new_tris = []
        to_split = set(edge_tris[best_e])
        for ti, tri in enumerate(tris):
            if ti not in to_split:
                new_tris.append(tri)
                continue
            a, b, c = tri
            if (a, b) == (u, v) or (a, b) == (v, u):
                new_tris.append([a, mid, c])
                new_tris.append([mid, b, c])
            elif (b, c) == (u, v) or (b, c) == (v, u):
                new_tris.append([a, b, mid])
                new_tris.append([a, mid, c])
            else:
                new_tris.append([a, b, mid])
                new_tris.append([mid, b, c])
        tris = new_tris
    return np.asarray(pts, dtype=np.float64), np.asarray(tris, dtype=np.int32)


def _tri_min_angle_deg(p0, p1, p2):
    e0, e1, e2 = p1 - p0, p2 - p1, p0 - p2
    l0, l1, l2 = np.linalg.norm(e0), np.linalg.norm(e1), np.linalg.norm(e2)
    if l0 < 1e-12 or l1 < 1e-12 or l2 < 1e-12:
        return 0.0
    c0 = max(-1.0, min(1.0, float(np.dot(e0, -e2)) / (l0 * l2)))
    c1 = max(-1.0, min(1.0, float(np.dot(-e0, e1)) / (l0 * l1)))
    c2 = max(-1.0, min(1.0, float(np.dot(-e1, e2)) / (l1 * l2)))
    return math.degrees(min(math.acos(c0), math.acos(c1), math.acos(c2)))


def improve_mesh_quality(points, faces, *, source_points=None, iterations=8, min_crease_cos=0.65):
    """Improve triangle aspect ratios via quality-driven edge flips and tangential smoothing while preserving boundaries."""
    pts = np.asarray(points, dtype=np.float64).copy()
    tris = np.asarray(faces, dtype=np.int32).copy()
    if len(tris) < 2:
        return pts, tris

    n_verts = len(pts)
    src_pts = np.asarray(source_points, dtype=np.float64) if source_points is not None else None
    src_tree = None
    if src_pts is not None and len(src_pts) >= 3:
        from scipy.spatial import cKDTree
        src_tree = cKDTree(src_pts)

    for _ in range(iterations):
        # 1. Quality-driven Delaunay edge flips on interior manifold edges.
        edge_adj = {}
        edge_set = set()
        for ti, (a, b, c) in enumerate(tris):
            for u, v, opp in ((int(a), int(b), int(c)), (int(b), int(c), int(a)), (int(c), int(a), int(b))):
                key = (u, v) if u < v else (v, u)
                edge_set.add(key)
                edge_adj.setdefault(key, []).append((ti, u, v, opp))

        flipped_tris = set()
        for (u, v), sides in edge_adj.items():
            if len(sides) != 2:
                continue
            (t1, a1, b1, c), (t2, a2, b2, d) = sides
            if t1 in flipped_tris or t2 in flipped_tris or c == d:
                continue
            if a1 != b2 or b1 != a2:
                continue
            new_key = (c, d) if c < d else (d, c)
            if new_key in edge_set:
                continue
            n1 = np.cross(pts[b1] - pts[a1], pts[c] - pts[a1])
            n2 = np.cross(pts[b2] - pts[a2], pts[d] - pts[a2])
            l1, l2 = float(np.linalg.norm(n1)), float(np.linalg.norm(n2))
            if l1 < 1e-12 or l2 < 1e-12:
                continue
            u1, u2 = n1 / l1, n2 / l2
            if float(np.dot(u1, u2)) < min_crease_cos:
                continue
            old_q = min(_tri_min_angle_deg(pts[a1], pts[b1], pts[c]), _tri_min_angle_deg(pts[a2], pts[b2], pts[d]))
            # Flipped pair preserving winding: (a1, d, c) and (d, b1, c)
            nn1 = np.cross(pts[d] - pts[a1], pts[c] - pts[a1])
            nn2 = np.cross(pts[b1] - pts[d], pts[c] - pts[d])
            nl1, nl2 = float(np.linalg.norm(nn1)), float(np.linalg.norm(nn2))
            if nl1 < 1e-12 or nl2 < 1e-12:
                continue
            navg = u1 + u2
            navg_l = float(np.linalg.norm(navg))
            if navg_l < 1e-8:
                continue
            navg /= navg_l
            if float(np.dot(nn1 / nl1, navg)) < 0.35 or float(np.dot(nn2 / nl2, navg)) < 0.35:
                continue
            new_q = min(_tri_min_angle_deg(pts[a1], pts[d], pts[c]), _tri_min_angle_deg(pts[d], pts[b1], pts[c]))
            if new_q > old_q + 1.0:
                tris[t1] = (a1, d, c)
                tris[t2] = (d, b1, c)
                edge_set.discard((u, v))
                edge_set.add(new_key)
                flipped_tris.add(t1)
                flipped_tris.add(t2)

        # 2. Tangential Laplacian smoothing on interior vertices (boundary & slit vertices stay fixed).
        inc_tris = [[] for _ in range(n_verts)]
        neighbors = [set() for _ in range(n_verts)]
        edge_counts = {}
        v_norm = np.zeros((n_verts, 3), dtype=np.float64)
        for ti, (a, b, c) in enumerate(tris):
            a, b, c = int(a), int(b), int(c)
            fn = np.cross(pts[b] - pts[a], pts[c] - pts[a])
            v_norm[a] += fn
            v_norm[b] += fn
            v_norm[c] += fn
            for x, y, z in ((a, b, c), (b, c, a), (c, a, b)):
                inc_tris[x].append((x, y, z))
                neighbors[x].add(y)
                neighbors[x].add(z)
                k = (x, y) if x < y else (y, x)
                edge_counts[k] = edge_counts.get(k, 0) + 1

        boundary_edges = [(u, v_e) for (u, v_e), cnt in edge_counts.items() if cnt == 1]
        boundary = {v for (u, v_e) in boundary_edges for v in (u, v_e)}
        b_adj = {}
        for u, v_e in boundary_edges:
            b_adj.setdefault(u, []).append(v_e)
            b_adj.setdefault(v_e, []).append(u)

        for v in range(n_verts):
            if not neighbors[v]:
                continue
            if v in boundary:
                # Redistribute vertices along smooth boundary segments (preserving corners/slit tips).
                bnbs = b_adj.get(v, [])
                if len(bnbs) != 2:
                    continue
                d1, d2 = pts[bnbs[0]] - pts[v], pts[bnbs[1]] - pts[v]
                l1, l2 = float(np.linalg.norm(d1)), float(np.linalg.norm(d2))
                if l1 < 1e-8 or l2 < 1e-8 or float(np.dot(d1, d2)) / (l1 * l2) > -0.92:
                    continue
                chord = pts[bnbs[1]] - pts[bnbs[0]]
                clen2 = float(np.dot(chord, chord))
                if clen2 < 1e-12:
                    continue
                mid = 0.5 * (pts[bnbs[0]] + pts[bnbs[1]])
                cand = pts[v] + 0.35 * (mid - pts[v])
                # Project back onto the chord so the boundary silhouette never shrinks.
                t_c = max(0.1, min(0.9, float(np.dot(cand - pts[bnbs[0]], chord)) / clen2))
                cand = pts[bnbs[0]] + t_c * chord
            else:
                nlen = float(np.linalg.norm(v_norm[v]))
                if nlen < 1e-12:
                    continue
                vn = v_norm[v] / nlen
                avg = np.mean([pts[u] for u in neighbors[v]], axis=0)
                disp = avg - pts[v]
                tang = disp - float(np.dot(disp, vn)) * vn
                cand = pts[v] + 0.40 * tang
                if src_tree is not None:
                    _, nn_idx = src_tree.query(cand, k=1)
                    # Keep vertex close to the original surface along the local normal.
                    delta_s = src_pts[int(nn_idx)] - cand
                    cand = cand + float(np.dot(delta_s, vn)) * vn
            old_min = min(_tri_min_angle_deg(pts[x], pts[y], pts[z]) for (x, y, z) in inc_tris[v])
            ok = True
            new_min = 180.0
            for (x, y, z) in inc_tris[v]:
                old_fn = np.cross(pts[y] - pts[x], pts[z] - pts[x])
                new_fn = np.cross(pts[y] - cand, pts[z] - cand)
                ol, nl = float(np.linalg.norm(old_fn)), float(np.linalg.norm(new_fn))
                if nl < 1e-12 or (ol > 1e-12 and float(np.dot(old_fn, new_fn)) < 0.5 * ol * nl):
                    ok = False
                    break
                new_min = min(new_min, _tri_min_angle_deg(cand, pts[y], pts[z]))
            if ok and new_min > old_min + 0.1:
                pts[v] = cand

    return pts, tris
