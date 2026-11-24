"""The sim mesh a piece starts from: the outer sewn sheet of its labelled render region, reduced to a vertex count by
edge collapse, split up to it where too coarse, and evened out by edge flips and tangential smoothing."""
import math
from collections import defaultdict

import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
from scipy.spatial import cKDTree


def outer_faces(positions, faces, shell):
    if shell is None:
        return faces
    a, b, c = positions[faces].transpose(1, 0, 2)
    normal = np.cross(b-a, c-a)
    if "axis_xz_m" in shell:
        center = np.asarray(shell["axis_xz_m"], dtype=float)
        if center.shape != (2,) or not np.isfinite(center).all():
            raise ValueError("invalid skirt axis")
        outward = (a+b+c)/3
        outward[:, [0,2]] -= center
        outward[:, 1] = 0
    else:
        axis = np.asarray(shell["outward"], dtype=float)
        if axis.shape != (3,) or not np.isfinite(axis).all() or np.linalg.norm(axis) == 0:
            raise ValueError("invalid outer surface direction")
        outward = np.broadcast_to(axis, normal.shape)
    projection = np.einsum("ij,ij->i", normal, outward)
    cutoff = shell.get("min_cosine", 0.0)
    if type(cutoff) not in (float, int) or not 0 <= cutoff < 1:
        raise ValueError("outer surface min_cosine must be a number in [0, 1)")
    if cutoff:
        # The narrow rim of a sloping hem can face slightly outwards.
        projection = projection - cutoff * np.linalg.norm(normal, axis=1) * np.linalg.norm(outward, axis=1)
    return faces[projection > 0]


def sewn_sheet(positions, triangles):
    # Split-edge fans must stay separate at a vent even when their positions coincide.
    welded, inverse = np.unique(np.round(positions, 7), axis=0, return_inverse=True)
    ids = inverse[triangles]
    area = np.cross(welded[ids[:, 1]] - welded[ids[:, 0]], welded[ids[:, 2]] - welded[ids[:, 0]])
    keep = np.linalg.norm(area, axis=1) >= 2e-8
    ids, source = ids[keep], triangles[keep]
    parent = list(range(3 * len(ids)))

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    def join(a, b):
        parent[root(a)] = root(b)

    edges = defaultdict(list)
    for face, values in enumerate(ids):
        for k, a in enumerate(values):
            q = (k + 1) % 3
            b = values[q]
            edges[min(a, b), max(a, b)].append((int(a), int(b), 3 * face + k, 3 * face + q))

    face_pairs = []
    for sides in edges.values():
        if len(sides) == 2:
            left, right = sides
            if left[0] == right[1]:
                join(left[2], right[3])
                join(left[3], right[2])
                face_pairs.append((left[2] // 3, right[2] // 3))
    if not face_pairs:
        raise ValueError("garment has no connected triangle sheet")
    pairs = np.asarray(face_pairs)
    graph = coo_matrix((np.ones(len(pairs)), pairs.T), shape=(len(ids), len(ids)))
    _, parts = connected_components(graph, directed=False)
    # Decorative piping and separate overlaid details follow their painted joints.
    sizes = np.bincount(parts, weights=np.linalg.norm(area[keep], axis=1))
    selected = np.where(parts == np.argmax(sizes))[0]
    corners = (3 * selected[:, None] + np.arange(3)).ravel()
    _, first, vertices = np.unique([root(int(i)) for i in corners], return_index=True, return_inverse=True)
    originals = source.ravel()[corners[first]]
    return positions[originals], vertices.reshape(-1, 3), originals, source[selected]


def simplify(points, triangles, budget):
    source_points = points.copy()
    owners = np.arange(len(points))
    while len(points) > budget:
        a, b, c = points[triangles].transpose(1, 0, 2)
        face_normals = np.cross(b-a, c-a)
        face_sets = [set() for _ in points]
        neighbors = [set() for _ in points]
        for i, (a, b, c) in enumerate(triangles):
            for x, y, z in ((a,b,c), (b,c,a), (c,a,b)):
                face_sets[x].add(i)
                neighbors[x].update((y,z))
        all_edges = np.sort(np.concatenate([triangles[:, [0,1]], triangles[:, [1,2]], triangles[:, [2,0]]]), axis=1)
        edges, counts = np.unique(all_edges, return_counts=True, axis=0)
        boundary = set(edges[counts == 1].ravel())
        if max(counts) > 2:
            raise ValueError("nonmanifold reduction input")
        lengths = np.linalg.norm(points[edges[:, 0]] - points[edges[:, 1]], axis=1)
        occupied, moved = set(), {}
        mapping = np.arange(len(points))
        for i in np.argsort(lengths, kind="stable"):
            x, y = edges[i]
            if x in occupied or y in occupied or (x in boundary and y in boundary and counts[i] != 1):
                continue
            if len(neighbors[x] & neighbors[y]) != counts[i]:
                continue
            middle = (points[x] + points[y]) * 0.5
            if (x in boundary) != (y in boundary):
                middle = points[x if x in boundary else y]
            affected = sorted(face_sets[x] ^ face_sets[y])
            if not affected:
                continue
            tr = triangles[affected]
            pos = points[tr].copy()
            pos[(tr == x) | (tr == y)] = middle
            new = np.cross(pos[:,1]-pos[:,0], pos[:,2]-pos[:,0])
            length = np.linalg.norm(new, axis=1)
            old = face_normals[affected]
            if np.any(length < 2e-8) or np.any(np.einsum("ij,ij->i", new, old) <= 0.1 * length * np.linalg.norm(old, axis=1)):
                continue
            moved[x] = middle
            mapping[y] = x
            occupied.update(neighbors[x] | neighbors[y] | {x,y})
            if len(points) - len(moved) == budget:
                break
        if not moved:
            raise ValueError(f"cannot reach {budget} vertices while preserving seams and face orientation ({len(points)} remain)")
        points = points.copy()
        for i, v in moved.items():
            points[i] = v
        triangles = mapping[triangles]
        valid = (triangles[:,0] != triangles[:,1]) & (triangles[:,1] != triangles[:,2]) & (triangles[:,0] != triangles[:,2])
        used, triangles = np.unique(triangles[valid], return_inverse=True)
        owners = np.searchsorted(used, mapping[owners])
        points, triangles = points[used], triangles.reshape(-1, 3)
    if len(points) < budget:
        points, triangles = subdivide_long_edges(points, triangles, max_edge_m=0.070, max_vertices=budget)
    points, triangles = improve_mesh_quality(points, triangles, source_points=source_points, iterations=10)
    return points, triangles, owners


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
