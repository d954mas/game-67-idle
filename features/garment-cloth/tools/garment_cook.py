"""Cook an oriented triangle surface in meters into the arrays of a garment/cloth.h ClothMesh."""
import math
import heapq
from numbers import Integral

import numpy as np


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
              edge_kinds=None, anim_drive=None):
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
    drive = None
    if anim_drive is not None:
        drive = np.asarray(anim_drive, dtype=np.float64)
        if drive.shape != (len(points),) or not np.isfinite(drive).all() or np.any((drive < 0) | (drive > 1)):
            raise ValueError(f"{name}: anim_drive must be finite [0, 1] per vertex")
    return dict(rest=points, inv_area=inv, faces=[tuple(f) for f in faces], edges=edges, tethers=tethers, anim_drive=drive)
