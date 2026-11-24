"""Cook an outfit from its sidecar (spec B.7, B.8): the blob v3 the runtime loads and the `_GARMENT` render binding of
each model that wears it.

The sidecar holds everything authored (sim meshes, pins as r = 0, weights, maps, fabric names, layer pairs); the
cook adds only what is derived: edges with their rest angles and warp/weft kind, tethers to the nearest pin, the
outfit's bones and body shapes, and the binding grown over the render mesh. `stale` compares the sidecar's build
hash with the current script and inputs."""
import hashlib
import heapq
import math
from numbers import Integral
from pathlib import Path

import numpy as np

from garment_blob import MATERIAL_FIELDS, MAX_DISTANCE_MAX, encode

from . import bind as B
from . import model as M
from .sidecar import read
from .sim import classify_edges

PALETTE_MAX = 2047  # 11-bit sim vertex ids of the render binding


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
    faces = [tuple(int(i) for i in f) for f in faces]
    pins = [int(i) for i in pins]

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
    if (backstop.shape != (len(points), 2) or not np.isfinite(backstop[:, 0]).all() or np.isnan(backstop).any() or
            np.any(backstop < 0)):
        raise ValueError(f"{name}: backstop must be (d, R) per vertex, d finite, both non-negative (R = inf is a plane)")
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


def depfile_text(target, dependencies):
    """One Make depfile rule with absolute paths; `$`, `#` and spaces are escaped the way Ninja reads them."""
    def escape(path):
        return Path(path).resolve().as_posix().replace("$", "$$").replace("#", "\\#").replace(" ", "\\ ")
    return escape(target) + ": " + " ".join(escape(p) for p in sorted(dependencies, key=str)) + "\n"


def stale(sidecar, root, script):
    """The build inputs whose hash no longer matches the sidecar's record ([] when it is current)."""
    build = sidecar.build
    out = []
    if build.get("script_sha256") != hashlib.sha256(Path(script).read_bytes()).hexdigest():
        out.append(str(script))
    for relative, digest in build.get("inputs", {}).items():
        path = Path(root) / relative
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            out.append(relative)
    return out


def shapes(colliders, joints, inverse_binds):
    """Spheres (joint index, centre in bone space, radius) and cones of a garment.colliders.v1 document."""
    spheres = []
    for s in colliders["spheres"]:
        j = joints.index(s["bone"])
        centre = inverse_binds[j][:3] @ np.append(np.asarray(s["center"], float), 1.0)
        spheres.append((j, centre, float(s["radius"])))
    return spheres, [tuple(c) for c in colliders["cones"]]


def cook(outfit_id, sidecar_data, model, fabrics, colliders, inertia):
    """(blob, streams, report): the blob v3 of one outfit, the `_GARMENT` stream of each primitive of the model that
    wears it (None where no piece binds) and a report row per piece.

    sidecar_data: the sidecar bytes; model: a model.RenderModel whose skin the sidecar was built on; fabrics:
    {name: ClothMaterial fields}; colliders: a garment.colliders.v1 document or None; inertia: (linear, angular)
    share of the root's motion the reference frame passes to the cloth."""
    sc = read(sidecar_data)
    data, report = cook_blob(outfit_id, sc, model, fabrics, colliders, inertia)
    return data, bind_model(model, sc), report


def cook_blob(outfit_id, sc, model, fabrics, colliders, inertia):
    """(blob, report) of a read Sidecar, without the render binding: what the runtime loads, for the game's cook and for
    a bake that steps the outfit through the host. The sidecar's corrective morph targets become the blob's
    correctives, in the sidecar's order."""
    joints = sc.joint_names
    if joints != model.joints:
        raise ValueError(f"{outfit_id}: the sidecar's skeleton is not the model's skin")
    ibm = sc.inverse_bind_matrices()
    if not np.array_equal(ibm.astype(np.float32), np.asarray(model.glb.inverse_binds()).astype(np.float32)):
        raise ValueError(f"{outfit_id}: the sidecar's inverse binds differ from the model's")
    spheres, cones = shapes(colliders, joints, ibm) if colliders else ([], [])
    used = {s[0] for s in spheres}
    names = sc.corrective_names
    records, report = [], []
    for p in sc.pieces:
        name = f"{outfit_id}/{p.id}"
        material = {k: v for k, v in fabrics[p.fabric].items() if k in MATERIAL_FIELDS}
        points = p.points.astype(np.float64)
        r = p.r.astype(np.float64)
        pins = np.flatnonzero(r == 0)
        # The tether lets the fabric reach its own strain limit along the path to the pin, no further.
        mesh = cook_mesh(name, points, p.faces, pins, wearable=True,
                         tether_slack=1 + material.get("max_edge_strain", 0.0),
                         edge_kinds=classify_edges(points, p.faces, pins), max_distance=r,
                         backstop=np.column_stack([p.d, p.R]).astype(np.float64))
        lanes = p.joints.astype(int)
        used |= set(np.unique(lanes[p.weights > 0]).tolist())
        render = sum(int(np.count_nonzero(label == p.label)) for label in model.labels)
        shapes_of = (np.stack([np.concatenate(p.correctives[k], axis=1) for k in names]).astype(np.float64)
                     if names else None)
        records.append(dict(id=p.id, mesh=mesh, skin=dict(bone=lanes, weight=p.weights.astype(np.float64)),
                            direction=p.b.astype(np.float64), material=material, render_vertices=render,
                            linear_inertia=inertia[0], angular_inertia=inertia[1], correctives=shapes_of))
        report.append(dict(id=p.id, sim_vertices=len(points), pins=len(pins), render_vertices=render,
                           correctives=list(p.correctives)))
    if sum(len(p.points) for p in sc.pieces) > PALETTE_MAX:
        raise ValueError(f"{outfit_id}: sim vertices exceed the {PALETTE_MAX}-entry render palette")
    bone_names = sorted(joints[j] for j in used)
    index = np.full(len(joints), -1)
    for i, n in enumerate(bone_names):
        index[joints.index(n)] = i
    for record in records:
        record["skin"]["bone"] = index[record["skin"]["bone"]]
    bones = [(n, ibm[joints.index(n)][:3].ravel()) for n in bone_names]
    data = encode(outfit_id, records, bones=bones, spheres=[(int(index[j]), c[:3], rad) for j, c, rad in spheres],
                  cones=cones, correctives=names)
    return data, report


def bind_model(model, sc):
    """The `_GARMENT` stream of every primitive of a model that wears the sidecar (None where no piece binds)."""
    offsets = np.cumsum([0] + [len(p.points) for p in sc.pieces])
    streams = [None] * len(model.primitives)
    for p, offset in zip(sc.pieces, offsets):
        kinematic = p.r == 0
        for k, label in enumerate(model.labels):
            take = label == p.label
            if not take.any():
                continue
            binding = B.grow(model.positions[k], model.faces[k], take, p.points.astype(float), p.faces, kinematic)
            if streams[k] is None:
                streams[k] = np.zeros((len(label), 4), "<u2")
            amount = binding.amount(kinematic)
            for v in np.flatnonzero(take):
                corners = p.faces[binding.triangle[v]]
                streams[k][v] = B.pack(*(int(offset) + corners + 1), *binding.bary[v], amount[v])
    return streams


def stage_model(path, streams):
    """GLB bytes of the model with the `_GARMENT` stream added to every bound primitive."""
    document, binary = M.G.parse_glb(Path(path).read_bytes(), str(path))
    binary = bytearray(binary)
    for primitive, stream in zip(document["meshes"][0]["primitives"], streams):
        if stream is None or not stream.any():
            continue
        if "_GARMENT" in primitive["attributes"]:
            raise ValueError(f"{path}: the model already carries a cooked _GARMENT attribute")
        data = np.ascontiguousarray(stream, "<u2").tobytes()
        binary.extend(b"\0" * (-len(binary) % 4))
        document["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(data),
                                        "target": 34962})
        primitive["attributes"]["_GARMENT"] = len(document["accessors"])
        document["accessors"].append({"bufferView": len(document["bufferViews"]) - 1, "componentType": 5123,
                                      "count": len(stream), "type": "VEC4"})
        binary.extend(data)
    return M.G.encode_glb(document, bytes(binary))
