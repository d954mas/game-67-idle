"""Cook a garment model into the blob of garment/asset.h and the render binding of its pieces.

The model carries the author contract: `_CLOTH_PIECE` labels the region of each piece, `_CLOTH` = 0 marks its pins
(max distance 0, so they follow skinning), and the skin weights reference body joints only. The recipe gives the
max-distance gradient from the pins and names the body primitives the backstop is measured from. `cook_mesh` turns
one oriented triangle surface into ClothMesh arrays.
"""
import heapq
import math
from numbers import Integral
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree

from garment_binding import bind_render
from garment_blob import MAX_DISTANCE_MAX, encode
from garment_paint import coincident
from garment_proxy import (classify_edges, closest_surface_points, corner_owners, geodesic_depth, outer_faces,
                           round_cone_signed_distance, sewn_sheet, simplify)

LABEL, FREEDOM = "_CLOTH_PIECE", "_CLOTH"
PALETTE_MAX = 2047  # 11-bit vertex ids of the render binding
BACKSTOP_DISC_MIN = 0.03  # m: the body fit needs a disc wide enough to see the limb's curvature
BACKSTOP_RADIUS_MIN = 0.01  # m
R_FLAT = 1.0  # m: the backstop radius of a flat or concave body, where no limb stands behind the cloth


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
              edge_kinds=None, max_distance=None, backstop=None):
    """Return one ClothMesh as garment_blob mesh arrays. Wearables need a pin on every piece and a movable vertex.

    max_distance is r per vertex (default: 0 on the pins, MAX_DISTANCE_MAX elsewhere) and must be 0 exactly on the
    pins; backstop is (d, R) per vertex (default off)."""
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
    if max_distance is None:
        max_distance = np.full(len(points), MAX_DISTANCE_MAX)
        max_distance[pins] = 0
    max_distance = np.asarray(max_distance, dtype=np.float64)
    if (max_distance.shape != (len(points),) or not np.isfinite(max_distance).all() or np.any(max_distance < 0) or
            np.any(max_distance[pins] != 0)):
        raise ValueError(f"{name}: max_distance must be finite, non-negative and zero on the pins")
    backstop = np.zeros((len(points), 2)) if backstop is None else np.asarray(backstop, dtype=np.float64)
    if backstop.shape != (len(points), 2) or not np.isfinite(backstop).all() or np.any(backstop < 0):
        raise ValueError(f"{name}: backstop must be a finite non-negative (d, R) per vertex")
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
    return dict(rest=points, inv_area=inv, faces=[tuple(f) for f in faces], edges=edges, tethers=tethers,
                max_distance=max_distance, backstop=backstop)


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


def _sphere_fit(points):
    """Centre and radius of the least-squares sphere |p - c|^2 = R^2 through the points, or None if degenerate."""
    if len(points) < 4:
        return None
    a = np.column_stack([2 * points, np.ones(len(points))])
    rhs = (points ** 2).sum(1)
    solution, _, rank, _ = np.linalg.lstsq(a, rhs, rcond=None)
    if rank < 4:
        return None
    center = solution[:3]
    radius2 = solution[3] + center @ center
    return (center, math.sqrt(radius2)) if radius2 > 0 else None


def _smooth_directions(directions, faces, iterations=4):
    """Average each direction with its sim-mesh neighbours, so folds of a coarse mesh do not flip it."""
    neighbours = [set() for _ in directions]
    for a, b, c in faces:
        neighbours[a] |= {b, c}
        neighbours[b] |= {a, c}
        neighbours[c] |= {a, b}
    out = directions.copy()
    for _ in range(iterations):
        mean = np.array([out[list(n)].mean(0) if n else out[i] for i, n in enumerate(neighbours)])
        out = 0.5 * out + 0.5 * mean
        out /= np.maximum(np.linalg.norm(out, axis=1, keepdims=True), 1e-12)
    return out


def body_maps(p, f, r, body, thickness, enabled=True):
    """Body direction b, backstop distance d and radius R per sim vertex from the body surface at rest (B.4).

    body: (vertices, faces, vertex normals) of the declared body primitives, or None. b points from the cloth to
    the closest body point (along -n where the cloth lies inside the body); d is that distance less the thickness;
    R is the sphere that best fits the body over a disc of radius r around the closest point, 0 where d >= r since
    such a backstop can never act, and R_FLAT where the body is concave there."""
    if body is None:
        a, b_, c = p[f].transpose(1, 0, 2)
        normals = np.zeros_like(p)
        for col in f.T:
            np.add.at(normals, col, np.cross(b_ - a, c - a))
        directions = -normals / np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-12)
        return directions, np.zeros((len(p), 2))
    vertices, faces, vertex_normals = body
    face, bary, distance = closest_surface_points(p, vertices, faces)
    closest = np.einsum("nk,nkj->nj", bary, vertices[faces[face]])
    normal = np.einsum("nk,nkj->nj", bary, vertex_normals[faces[face]])
    normal /= np.maximum(np.linalg.norm(normal, axis=1, keepdims=True), 1e-12)
    signed = np.einsum("nj,nj->n", p - closest, normal)
    toward = (closest - p) / np.maximum(distance, 1e-12)[:, None]
    directions = np.where(((signed > 1e-4) & (distance > 1e-4))[:, None], toward, -normal)
    directions = _smooth_directions(directions, f)
    backstop = np.zeros((len(p), 2))
    backstop[:, 0] = np.maximum(signed, 0) - thickness
    backstop[:, 0] = np.maximum(backstop[:, 0], 0)
    if not enabled:
        return directions, backstop
    tree = cKDTree(vertices)
    for i in np.flatnonzero((r > 0) & (backstop[:, 0] < r)):
        near = vertices[tree.query_ball_point(closest[i], max(float(r[i]), BACKSTOP_DISC_MIN))]
        fit = _sphere_fit(near)
        if fit is None or (fit[0] - closest[i]) @ directions[i] <= 0:
            backstop[i, 1] = R_FLAT
        else:
            backstop[i, 1] = min(max(fit[1], BACKSTOP_RADIUS_MIN), R_FLAT)
    return directions, backstop


def cook_piece(outfit_id, model, piece, rows, joints, inverse_binds, body, shapes_rest, tether_slack):
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
    # A proxy vertex is a pin where every source corner is one.
    face, bary, _ = closest_surface_points(p, sheet_p, sheet_f)
    corner = freedom[orig][sheet_f[face]]
    pinned = ~((bary * (corner > 0)).sum(1) > 0)
    pins = np.flatnonzero(pinned)
    if len(pins) < 2:
        raise ValueError(f"{name}: fewer than two proxy vertices are pins")
    # Inverse-square blend of the four nearest sheet vertices: a proxy vertex on a sheet vertex takes its weights.
    distance, near = cKDTree(sheet_p).query(p, k=min(4, len(sheet_p)))
    inverse = 1 / np.maximum(distance, 1e-4) ** 2
    order, influence = top_influences(np.einsum("nk,nkj->nj", inverse, weights[orig][near]) / inverse.sum(1, keepdims=True))
    geodesic, _ = geodesic_depth(p, f, pins)
    rule = piece["max_distance"]
    r = np.minimum(rule["rate"] * geodesic, rule["cap_m"])
    r[pins] = 0
    thickness = piece["material"]["thickness"]
    directions, backstop = body_maps(p, f, r, body, thickness, piece.get("backstop", True))
    mesh = cook_mesh(name, p, f, pins, wearable=True, tether_slack=tether_slack,
                     edge_kinds=classify_edges(p, f, pins), max_distance=r, backstop=backstop)
    used = {joints[j] for j in np.unique(order[influence > 0])}
    skin = dict(bone=[[joints[j] for j in row] for row in order], weight=influence)
    held = np.zeros(len(p))
    for a_, b_, ra, rb in shapes_rest:
        held = np.maximum(held, [-round_cone_signed_distance(x, a_, b_, ra + thickness, rb + thickness) for x in p])
    render = sum(int(np.count_nonzero(row[1] == value)) for row in rows)
    record = dict(id=piece["id"], mesh=mesh, skin=skin, direction=directions, material=piece["material"],
                  render_vertices=render, linear_inertia=piece["linear_inertia"],
                  angular_inertia=piece["angular_inertia"])
    acting = backstop[:, 1] > 0
    report = dict(id=piece["id"], proxy_vertices=len(p), proxy_faces=len(f), pins=len(pins), free=int((r > 0).sum()),
                  render_vertices=render, hem_max_distance_m=round(float(r.max()), 4),
                  backstop_vertices=int(acting.sum()),
                  backstop_d_p50_mm=round(float(np.median(backstop[acting, 0])) * 1000, 1) if acting.any() else None,
                  rest_inside_shapes=int(np.count_nonzero(held > 0)),
                  rest_held_beyond_r=int(np.count_nonzero(held > r)),
                  rest_held_beyond_r_max_mm=round(float(np.maximum(held - r, 0).max(initial=0)) * 1000, 1))
    return record, (value, p, f, corners, pinned, primitive), report, used


def _body_surface(model, primitives):
    """Rest vertices, triangles and vertex normals of the model primitives the recipe declares as body."""
    vertices, faces, normals, offset = [], [], [], 0
    for primitive in model.primitives():
        if model.material(primitive) not in primitives:
            continue
        a = primitive["attributes"]
        points = model.accessor(a["POSITION"])
        tris = model.accessor(primitive["indices"], False).reshape(-1, 3)
        if "NORMAL" in a:
            n = model.accessor(a["NORMAL"])
        else:
            n = np.zeros_like(points)
            for col in tris.T:
                np.add.at(n, col, np.cross(points[tris[:, 1]] - points[tris[:, 0]], points[tris[:, 2]] - points[tris[:, 0]]))
        vertices.append(points)
        faces.append(tris + offset)
        normals.append(n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12))
        offset += len(points)
    if not vertices:
        return None
    return np.concatenate(vertices), np.concatenate(faces), np.concatenate(normals)


def _shapes(colliders, inverse_binds):
    """Spheres in bone space and the cones joining them, shared where two capsules share an end."""
    spheres, index, cones = [], {}, []
    for ja, pa, jb, pb, ra, rb in colliders:
        ends = []
        for joint, point, radius in ((ja, pa, ra), (jb, pb, rb or ra)):
            key = (joint, tuple(np.round(np.asarray(point, float), 6)), round(float(radius), 6))
            if key not in index:
                index[key] = len(spheres)
                center = inverse_binds[joint][:3] @ np.append(np.asarray(point, float), 1.0)
                spheres.append((joint, center, float(radius), np.asarray(point, float)))
            ends.append(index[key])
        if ends[0] != ends[1] and tuple(ends) not in cones:
            cones.append(tuple(ends))
    return spheres, cones


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


def cook_outfit(outfit_id, model, pieces, body_joints, *, colliders=(), body_primitives=(), tether_slack=1.05):
    """Blob bytes, the proxy sheets the render binding needs and a report row per piece.

    model: a garment_glb.Glb. pieces: dicts with id, piece (its `_CLOTH_PIECE` value), source_material (the primitive
    the proxy is built from), optional outer_surface, budget, max_distance {rate, cap_m} (r grows by rate per metre of
    fabric from the pins up to cap_m), optional backstop (False: no body behind the piece), material (ClothMaterial
    fields) and the linear_inertia and angular_inertia of the reference frame. colliders: the outfit's body shapes
    [(joint_a, point_a, joint_b, point_b, radius_a, radius_b)] in rest model space. body_primitives: the materials of
    the model primitives that are the body behind the cloth.
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
    colliders = list(colliders)
    missing = {c[k] for c in colliders for k in (0, 2)} - set(body_joints)
    if missing:
        raise ValueError(f"{outfit_id}: collider joints outside the body rig: {sorted(missing)}")
    spheres, cones = _shapes(colliders, inverse_binds)
    in_cone = {i for cone in cones for i in cone}
    shapes_rest = [(spheres[a][3], spheres[b][3], spheres[a][2], spheres[b][2]) for a, b in cones]
    shapes_rest += [(s[3], s[3], s[2], s[2]) for i, s in enumerate(spheres) if i not in in_cone]
    body = _body_surface(model, set(body_primitives))
    records, sheets, report, used = [], [], [], {s[0] for s in spheres}
    for piece in pieces:
        record, sheet, info, joints_used = cook_piece(outfit_id, model, piece, rows, joints, inverse_binds, body,
                                                      shapes_rest, tether_slack)
        records.append(record)
        sheets.append(sheet[:5] + (piece["source_material"],))
        report.append(info)
        used |= joints_used
    if sum(len(s[1]) for s in sheets) > PALETTE_MAX:
        raise ValueError(f"{outfit_id}: proxy vertices exceed the {PALETTE_MAX}-entry render palette")
    outside = used - set(body_joints)
    if outside:
        raise ValueError(f"{outfit_id}: joints outside the body rig: {sorted(outside)}")
    names = sorted(used)
    index = {joint: i for i, joint in enumerate(names)}
    for record in records:
        record["skin"]["bone"] = [[index[j] for j in row] for row in record["skin"]["bone"]]
    bones = [(joint, inverse_binds[joint][:3].ravel()) for joint in names]
    blob = encode(outfit_id, records, bones=bones, spheres=[(index[j], c, r) for j, c, r, _ in spheres], cones=cones)
    return blob, sheets, report


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
