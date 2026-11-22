"""Cook an oriented triangle surface in meters into immutable garment/cloth.h data."""
import math
import heapq
import re
from numbers import Integral

import numpy as np


def vector(values):
    return "{" + ",".join(f"{float(x):.9e}F" for x in values) + "}"


def array(out, decl, rows):
    out.write(f"static const {decl} = {{\n")
    out.write(",\n".join(rows))
    out.write("\n};\n")


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


def cook_mesh(out, name, points, faces, pins, *, wearable=False, tether_slack=None,
              edge_kinds=None, anim_drive=None):
    """Emit one ClothMesh. Wearables need a pin on every piece and a movable vertex."""
    if not re.fullmatch(r"[a-zA-Z_][a-zA-Z0-9_]*", name):
        raise ValueError(f"{name!r}: invalid C symbol")
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
        edges.append(f"{{{a},{b},{c},{d},{length:.9e}F,{angle:.9e}F,{kind}}}")
    array(out, f"float {name}_rest[][3]", map(vector, points))
    array(out, f"ClothFace {name}_faces[]", ("{%d,%d,%d}" % tuple(f) for f in faces))
    array(out, f"ClothEdge {name}_edges[]", edges)
    out.write(f"static const float {name}_inv[] = {vector(inv)};\n")
    tethers = "0"
    if tether_slack is not None and pins:
        tethers = name + "_tethers"
        limits = _tethers(points, adjacent, pins, tether_slack)
        array(out, f"ClothTether {tethers}[]", (f"{{{pin},{length:.9e}F}}" for pin, length in limits))
    drive_sym = "0"
    if anim_drive is not None:
        drive_arr = np.asarray(anim_drive, dtype=np.float64)
        if drive_arr.shape != (len(points),) or not np.isfinite(drive_arr).all() or np.any((drive_arr < 0) | (drive_arr > 1)):
            raise ValueError(f"{name}: anim_drive must be finite [0, 1] per vertex")
        drive_sym = name + "_anim_drive"
        out.write(f"static const float {drive_sym}[] = {vector(drive_arr)};\n")
    out.write(f"static const ClothMesh {name} = {{{len(points)}, {len(faces)}, {len(edges)}, "
              f"{name}_rest, {name}_inv, {name}_faces, {name}_edges, {tethers}, {drive_sym}}};\n")
