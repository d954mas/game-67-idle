"""Cook a garment model into the blob of garment/asset.h and the render binding of its pieces.

The model carries the author contract: `_CLOTH_PIECE` labels the region of each piece, `_CLOTH` is its freedom
(0 is a pin that follows skinning, a free vertex is driven at drive_hz * (1 - _CLOTH)), and the skin weights
reference body joints only. `cook_mesh` turns one oriented triangle surface into ClothMesh arrays.
"""
import heapq
import math
from numbers import Integral
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree

from garment_binding import bind_render
from garment_blob import encode
from garment_paint import coincident
from garment_proxy import (classify_edges, closest_surface_points, corner_owners, geodesic_depth, outer_faces,
                           round_cone_signed_distance, sewn_sheet, simplify)

LABEL, FREEDOM = "_CLOTH_PIECE", "_CLOTH"
PALETTE_MAX = 2047  # 11-bit vertex ids of the render binding


def top_influences(weights, count=4):
    """The `count` largest weights per row, renormalized; ties keep the lower joint index first."""
    weights = np.asarray(weights, float)
    if weights.shape[1] < count:
        weights = np.pad(weights, ((0, 0), (0, count - weights.shape[1])))
    order = np.argsort(-weights, axis=1, kind="stable")[:, :count]
    values = np.take_along_axis(weights, order, axis=1)
    values /= np.maximum(values.sum(1, keepdims=True), 1e-30)
    # An empty lane names the strongest joint, so a lane never addresses a joint the vertex does not use.
    order = np.where(values == 0, order[:, :1], order)
    return order, values


def _tethers(points, adjacent, pins, slack):
    graph = [[] for _ in points]
    for a, b in adjacent:
        length = np.linalg.norm(points[b] - points[a])
        graph[a].append((b, length))
        graph[b].append((a, length))
    nearest = [(float("inf"), -1)] * len(points)
    queue = []
    for pin in set(pins):
        nearest[pin] = (0, pin)
        heapq.heappush(queue, (0, pin, pin))
    while queue:
        length, pin, i = heapq.heappop(queue)
        if nearest[i] != (length, pin):
            continue
        for j, edge_length in graph[i]:
            candidate = (length + edge_length, pin)
            if candidate < nearest[j]:
                nearest[j] = candidate
                heapq.heappush(queue, (*candidate, j))
    return [(pin, length * slack if pin >= 0 else 0.0) for length, pin in nearest]


def cook_mesh(name, points, faces, pins, *, wearable=False, tether_slack=None,
              edge_kinds=None, drive=None):
    """Return one ClothMesh as garment_blob mesh arrays. Wearables need a pin on every piece and a movable vertex."""
    points = np.asarray(points, dtype=np.float64)
    if points.ndim != 2 or points.shape[1] != 3 or not np.isfinite(points).all() or not 3 <= len(points) <= 512:
        raise ValueError(f"{name}: invalid positions or vertex capacity exceeded")
    if np.any(np.abs(points) > np.finfo(np.float32).max):
        raise ValueError(f"{name}: positions exceed single-precision range")
    faces = list(faces)
    pins = list(pins)

    def vertex_index(i):
        return isinstance(i, Integral) and not isinstance(i, bool) and 0 <= i < len(points)

    if not all(vertex_index(i) for i in pins):
        raise ValueError(f"{name}: invalid pin index")
    if tether_slack is not None and (not math.isfinite(tether_slack) or tether_slack < 1.0):
        raise ValueError(f"{name}: tether_slack must be at least 1")
    areas = np.zeros(len(points))
    adjacent = {}
    seen_faces = set()
    for face in faces:
        if len(face) != 3 or not all(vertex_index(i) for i in face):
            raise ValueError(f"{name}: invalid triangle")
        a, b, c = face
        key = tuple(sorted(face))
        area = np.linalg.norm(np.cross(points[b] - points[a], points[c] - points[a])) * 0.5
        if area < 1e-8 or key in seen_faces:
            raise ValueError(f"{name}: duplicate or degenerate triangle {face}")
        seen_faces.add(key)
        areas[[a, b, c]] += area / 3
        for i, j, k in ((a, b, c), (b, c, a), (c, a, b)):
            sides = adjacent.setdefault(tuple(sorted((i, j))), [])
            if sides and (len(sides) > 1 or sides[0][:2] != (j, i)):
                raise ValueError(f"{name}: nonmanifold edge or inconsistent winding at {i}, {j}")
            sides.append((i, j, k))
    if np.any(areas == 0) or len(adjacent) > 1536:
        raise ValueError(f"{name}: orphan vertex or edge capacity exceeded")
    if wearable:
        graph = [set() for _ in points]
        for i, j in adjacent:
            graph[i].add(j)
            graph[j].add(i)
        reached, stack = set(pins), list(pins)
        while stack:
            new = graph[stack.pop()] - reached
            stack.extend(new)
            reached.update(new)
        if len(reached) != len(points) or len(set(pins)) == len(points):
            raise ValueError(f"{name}: wearable needs a pin on each connected piece and a movable surface")
    inv = 1 / areas
    inv[pins] = 0
    edges = []
    for key, sides in adjacent.items():
        a, b, c = sides[0]
        d = sides[1][2] if len(sides) == 2 else -1
        axis = points[b] - points[a]
        length = np.linalg.norm(axis)
        angle = 0
        if d >= 0:
            n1 = np.cross(axis, points[c] - points[a])
            n2 = np.cross(points[d] - points[a], axis)
            angle = math.atan2(np.dot(axis / length, np.cross(n1, n2)), np.dot(n1, n2))
        kind = int(edge_kinds.get(key, 0)) if edge_kinds is not None else 0
        if kind not in (0, 1, 2):
            raise ValueError(f"{name}: invalid edge kind {kind} at {key}")
        edges.append((a, b, c, d, float(length), float(angle), kind))
    tethers = None
    if tether_slack is not None and pins:
        tethers = _tethers(points, adjacent, pins, tether_slack)
    drive = np.zeros(len(points)) if drive is None else np.asarray(drive, dtype=np.float64)
    if drive.shape != (len(points),) or not np.isfinite(drive).all() or np.any(drive < 0):
        raise ValueError(f"{name}: drive must be a finite non-negative squared frequency per vertex")
    return dict(rest=points, inv_area=inv, faces=[tuple(f) for f in faces], edges=edges, tethers=tethers, drive=drive)


def _labels(model, primitive, where):
    """Labels, freedom and triangles of one primitive, after the contract checks."""
    a = primitive["attributes"]
    count = model.json["accessors"][a["POSITION"]]["count"]
    label = model.accessor(a[LABEL]).ravel() if LABEL in a else np.zeros(count)
    if not np.all((label >= 0) & (label == np.round(label))):
        raise ValueError(f"{where}: {LABEL} must hold non-negative integers")
    freedom = model.accessor(a[FREEDOM]).ravel() if FREEDOM in a else np.zeros(count)
    if not np.all(np.isfinite(freedom) & (freedom >= 0) & (freedom <= 1)):
        raise ValueError(f"{where}: {FREEDOM} must lie in [0, 1]")
    if np.any((label == 0) & (freedom > 0)):
        raise ValueError(f"{where}: {FREEDOM} is set on a vertex without a piece label")
    faces = model.accessor(primitive["indices"], False).reshape(-1, 3)
    return label.astype(np.int64), freedom, faces


def cook_piece(outfit_id, model, piece, rows, joints, body_joints, inverse_binds, tether_slack):
    """One piece's blob record, its proxy sheet for the render binding and its report row."""
    name = f"{outfit_id}/{piece['id']}"
    value = piece["piece"]
    source = [row for row in rows if model.material(row[0]) == piece["source_material"]]
    if len(source) != 1:
        raise ValueError(f"{name}: expected one {piece['source_material']!r} primitive")
    primitive, label, faces, weights, freedom = source[0]
    positions = model.accessor(primitive["attributes"]["POSITION"])
    region = faces[(label == value)[faces].any(1)]
    if not len(region):
        raise ValueError(f"{name}: piece {value} has no {piece['source_material']!r} triangles")
    sheet_p, sheet_f, orig, outer = sewn_sheet(positions, outer_faces(positions, region, piece.get("outer_surface")))
    p, f, surviving = simplify(sheet_p, sheet_f, piece["budget"])
    corners = corner_owners(positions, outer, surviving[sheet_f])
    # A proxy vertex is a pin where every source corner is one; elsewhere it carries the freedom of the free corners
    # only, since the map jumps at a seam and blending across the jump would invent a stiffer drive band.
    face, bary, _ = closest_surface_points(p, sheet_p, sheet_f)
    corner = freedom[orig][sheet_f[face]]
    share = bary * (corner > 0)
    total = share.sum(1)
    proxy_freedom = np.where(total > 0, (share * corner).sum(1) / np.where(total > 0, total, 1), 0.0)
    pins = np.flatnonzero(proxy_freedom == 0)
    if len(pins) < 2:
        raise ValueError(f"{name}: fewer than two proxy vertices are pins")
    # Inverse-square blend of the four nearest sheet vertices: a proxy vertex on a sheet vertex takes its weights.
    distance, near = cKDTree(sheet_p).query(p, k=min(4, len(sheet_p)))
    inverse = 1 / np.maximum(distance, 1e-4) ** 2
    order, influence = top_influences(np.einsum("nk,nkj->nj", inverse, weights[orig][near]) / inverse.sum(1, keepdims=True))
    reach, _ = geodesic_depth(p, f, pins)
    drive_hz = piece.get("drive_hz", 0.0)
    driven = np.any((proxy_freedom > 0) & (proxy_freedom < 1))
    if driven != (drive_hz > 0):
        raise ValueError(f"{name}: drive_hz {drive_hz} does not match the freedom map, which "
                         f"{'drives' if driven else 'leaves undriven'} the free vertices")
    drive = (2 * math.pi * drive_hz * (1 - proxy_freedom)) ** 2
    drive[pins] = 0
    used = sorted({joints[j] for j in np.unique(order[influence > 0])} |
                  {c[k] for c in piece["colliders"] for k in (0, 2)})
    missing = set(used) - set(body_joints)
    if missing:
        raise ValueError(f"{name}: joints outside the body rig: {sorted(missing)}")
    index = {joint: i for i, joint in enumerate(used)}
    bones = [(joint, inverse_binds[joint][:3].ravel()) for joint in used]
    mesh = cook_mesh(name, p, f, pins, wearable=True, tether_slack=tether_slack,
                     edge_kinds=classify_edges(p, f, pins), drive=drive)
    a, b, c = p[f].transpose(1, 0, 2)
    normals = np.zeros_like(p)
    for col in f.T:
        np.add.at(normals, col, np.cross(b - a, c - a))
    length = np.linalg.norm(normals, axis=1)
    if np.min(length) < 1e-10:
        raise ValueError(f"{name}: undefined proxy normals")
    normals /= length[:, None]
    vertices = dict(bone=[[index[joints[j]] for j in row] for row in order], weight=influence, normal=normals,
                    travel=piece["swing"] * reach, backstop=piece["backstop_per_m"] * reach)
    colliders = [(index[ja], index[jb], pa, pb, float(ra), float(rb), int(flags))
                 for ja, pa, jb, pb, ra, rb, flags in piece["colliders"]]
    thickness = piece["material"]["thickness"]
    free = np.flatnonzero(proxy_freedom > 0)
    depth = np.zeros(len(free))
    for _, pa, _, pb, ra, rb, _ in piece["colliders"]:
        a_, b_ = np.asarray(pa, float), np.asarray(pb, float)
        depth = np.maximum(depth, [-round_cone_signed_distance(x, a_, b_, ra + thickness, (rb or ra) + thickness)
                                   for x in p[free]])
    render = sum(int(np.count_nonzero(row[1] == value)) for row in rows)
    record = dict(id=piece["id"], bones=bones, mesh=mesh, vertices=vertices, material=piece["material"],
                  colliders=colliders, render_vertices=render, linear_inertia=piece["linear_inertia"],
                  angular_inertia=piece["angular_inertia"], downward_follow=piece["downward_follow"])
    report = dict(id=piece["id"], proxy_vertices=len(p), proxy_faces=len(f), pins=len(pins), free=len(free),
                  render_vertices=render, rest_inside_colliders=int(np.count_nonzero(depth > 0)),
                  rest_inside_max_mm=round(float(depth.max(initial=0)) * 1000, 1))
    return record, (value, p, f, corners, proxy_freedom == 0, primitive), report


def _rows(model, body_joints):
    where = model.path or "model"
    joints = model.skin_joints()
    rows = []
    for primitive in model.primitives():
        here = f"{where}/{model.material(primitive)}"
        label, freedom, faces = _labels(model, primitive, here)
        weights = model.skin_weights(primitive, len(joints))
        unweighted = np.flatnonzero((label > 0) & (weights.sum(1) <= 0))
        if len(unweighted):
            raise ValueError(f"{here}: {len(unweighted)} piece vertices have no skin weights (JOINTS_0/WEIGHTS_0); "
                             "the cloth target is skinned from them")
        outside = sorted(joints[j] for j in np.flatnonzero(weights.any(0)) if joints[j] not in body_joints)
        if outside:
            raise ValueError(f"{here}: weights on joints outside the body rig: {outside}")
        rows.append((primitive, label, faces, weights, freedom))
    skinned = np.concatenate([model.accessor(r[0]["attributes"]["POSITION"])[r[1] == 0] for r in rows])
    for primitive, label, _, _, freedom in rows:
        free = np.flatnonzero((label > 0) & (freedom > 0))
        torn = free[coincident(model.accessor(primitive["attributes"]["POSITION"])[free], skinned)]
        if len(torn):
            raise ValueError(f"{where}/{model.material(primitive)}: vertex {int(torn[0])} of piece "
                             f"{int(label[torn[0]])} is free at the position of an unlabelled vertex and would open a "
                             "crack; it must be a pin")
    return joints, rows


def cook_outfit(outfit_id, model, pieces, body_joints, *, tether_slack=1.05):
    """Blob bytes, the proxy sheets the render binding needs and a report row per piece.

    model: a garment_glb.Glb. pieces: dicts with id, piece (its `_CLOTH_PIECE` value), source_material (the primitive
    the proxy is built from), optional outer_surface, budget, drive_hz (absent for an undriven piece), swing, backstop_per_m (how far below its
    target a free vertex may sink per metre of fabric to its pin), material (ClothMaterial fields),
    colliders [(joint_a, point_a, joint_b, point_b, radius_a, radius_b, flags)] in rest model space, and the
    linear_inertia, angular_inertia and downward_follow of the reference frame.
    """
    numbers = [piece["piece"] for piece in pieces]
    if (len(set(numbers)) != len(numbers) or
            not all(isinstance(n, int) and not isinstance(n, bool) and n > 0 for n in numbers)):
        raise ValueError(f"{outfit_id}: piece labels must be unique positive integers")
    joints, rows = _rows(model, body_joints)
    found = set().union(*(set(np.unique(row[1][row[1] > 0]).tolist()) for row in rows))
    unknown, lost = found - set(numbers), set(numbers) - found
    if unknown or lost:
        raise ValueError(f"{model.path or outfit_id}: labels without a recipe piece {sorted(unknown)}, "
                         f"recipe pieces without vertices {sorted(lost)}")
    inverse_binds = model.inverse_binds()
    records, sheets, report = [], [], []
    for piece in pieces:
        record, sheet, info = cook_piece(outfit_id, model, piece, rows, joints, body_joints, inverse_binds, tether_slack)
        records.append(record)
        sheets.append(sheet[:5] + (piece["source_material"],))
        report.append(info)
    if sum(len(s[1]) for s in sheets) > PALETTE_MAX:
        raise ValueError(f"{outfit_id}: proxy vertices exceed the {PALETTE_MAX}-entry render palette")
    return encode(outfit_id, records), sheets, report


def bind_outfit(model, sheets, body_joints):
    """The `_GARMENT` stream per primitive of a model wearing the cooked sheets (None where no piece binds)."""
    _, rows = _rows(model, body_joints)
    offsets = np.cumsum([0] + [len(s[1]) for s in sheets])
    streams = []
    for primitive, label, faces, _, freedom in rows:
        if not np.any(label > 0):
            streams.append(None)
            continue
        attributes = primitive["attributes"]
        positions = model.accessor(attributes["POSITION"])
        normals = model.accessor(attributes["NORMAL"]) if "NORMAL" in attributes else None
        material = model.material(primitive)
        bound = [(value, p, f, corners if source == material else None, pins)
                 for value, p, f, corners, pins, source in sheets]
        stream, _ = bind_render(positions, faces, normals, label, freedom, bound, offsets[:-1])
        streams.append(stream)
    return streams


def stage_model(model, streams):
    """GLB bytes of the model with the `_GARMENT` render binding added to every bound primitive."""
    for primitive, stream in zip(model.primitives(), streams):
        if stream is None or not stream.any():
            continue
        if "_GARMENT" in primitive["attributes"]:
            raise ValueError(f"{model.path}: source already contains a cooked _GARMENT attribute")
        model.set_attribute(primitive, "_GARMENT", stream, component_type=5123)
    return model.to_bytes()


def depfile_text(target, dependencies):
    """One Make depfile rule with absolute paths; `$`, `#` and spaces are escaped the way Ninja reads them."""
    def escape(path):
        return Path(path).resolve().as_posix().replace("$", "$$").replace("#", "\\#").replace(" ", "\\ ")
    return escape(target) + ": " + " ".join(escape(p) for p in sorted(dependencies, key=str)) + "\n"
