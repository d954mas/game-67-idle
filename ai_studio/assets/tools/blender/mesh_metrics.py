"""Topology metrics shared by Blender audit scripts, mesh_check and tests (stdlib + numpy only)."""

from __future__ import annotations

from collections import Counter
from math import sqrt
from typing import Iterable, Sequence

try:
    import numpy as np
except ImportError:  # measure_mesh stays pure Python for game scripts without numpy
    np = None

Vector3 = Sequence[float]


def polygon_area(points: Sequence[Vector3]) -> float:
    """Return the area of a planar 3D polygon using Newell's method."""
    if len(points) < 3:
        return 0.0
    nx = ny = nz = 0.0
    for index, current in enumerate(points):
        following = points[(index + 1) % len(points)]
        nx += (current[1] - following[1]) * (current[2] + following[2])
        ny += (current[2] - following[2]) * (current[0] + following[0])
        nz += (current[0] - following[0]) * (current[1] + following[1])
    return 0.5 * sqrt(nx * nx + ny * ny + nz * nz)


def weld(points, tolerance):
    """(welded points, old -> new index): one point per position within `tolerance` grid cells.
    glTF splits a vertex along UV, normal and material seams; welding keeps those splits from
    being counted as openings."""
    points = np.asarray(points, float).reshape(-1, 3)
    if not tolerance or not len(points):
        return points, np.arange(len(points))
    _, first, inverse = np.unique(np.round(points / tolerance), axis=0, return_index=True,
                                  return_inverse=True)
    return points[first], inverse.reshape(-1)


def face_components(faces, count):
    """Component label per face: faces sharing a vertex are connected."""
    parent = list(range(count))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    rows = np.asarray(faces, np.int64).reshape(-1, 3).tolist()
    for a, b, c in rows:
        for u, v in ((a, b), (b, c)):
            ru, rv = find(u), find(v)
            if ru != rv:
                parent[ru] = rv
    roots = np.array([find(row[0]) for row in rows], np.int64)
    return np.unique(roots, return_inverse=True)[1].reshape(-1) if rows else roots


def triangle_topology(points, faces, *, weld_tolerance=0.0, area_epsilon=1.0e-10):
    """Counts and per-face/per-edge detail of one triangle soup.

    Returned keys are counts plus arrays for callers that locate problems: `points` (welded),
    `faces` (welded indices), `component` per face, `boundary` (welded vertex pairs)."""
    points, remap = weld(points, weld_tolerance)
    faces = remap[np.asarray(faces, np.int64).reshape(-1, 3)] if len(faces) else np.zeros((0, 3), np.int64)
    n, f = len(points), len(faces)
    a = np.concatenate([faces[:, 0], faces[:, 1], faces[:, 2]])
    b = np.concatenate([faces[:, 1], faces[:, 2], faces[:, 0]])
    real = a != b
    undirected = np.minimum(a, b) * n + np.maximum(a, b)
    keys, inverse, counts = np.unique(undirected[real], return_inverse=True, return_counts=True)
    # Miswinding is judged on two-face edges only: an edge of 3+ faces repeats a direction by necessity.
    directed_keys, directed = np.unique((a * n + b)[real], return_counts=True)
    pair = np.minimum(directed_keys // n, directed_keys % n) * n + np.maximum(directed_keys // n, directed_keys % n)
    two_face = np.isin(pair, keys[counts == 2])
    cross = np.cross(points[faces[:, 1]] - points[faces[:, 0]], points[faces[:, 2]] - points[faces[:, 0]])
    degenerate = 0.5 * np.linalg.norm(cross, axis=1) <= area_epsilon
    duplicate = f - len(np.unique(np.sort(faces, axis=1), axis=0)) if f else 0
    component = face_components(faces, n) if f else np.zeros(0, np.int64)
    occurrence_face = np.tile(np.arange(f), 3)[real]
    open_face = occurrence_face[counts[inverse] == 1]
    open_components = np.unique(component[open_face]) if len(open_face) else np.zeros(0, np.int64)
    volume_per_face = np.einsum("ij,ij->i", points[faces[:, 0]], np.cross(points[faces[:, 1]],
                                points[faces[:, 2]])) / 6.0 if f else np.zeros(0)
    components = int(component.max()) + 1 if f else 0
    boundary = keys[counts == 1]
    return {
        "vertices": int(len(remap)),
        "welded_vertices": int(n),
        "triangles": int(f),
        "edges": int(len(keys)),
        "boundary_edges": int(np.sum(counts == 1)),
        "non_manifold_edges": int(np.sum(counts > 2)),
        "miswound_edges": int(np.sum((directed > 1) & two_face)),
        "degenerate_faces": int(np.sum(degenerate)),
        "duplicate_faces": int(duplicate),
        "components": components,
        "open_components": int(len(open_components)),
        "signed_volume": float(volume_per_face.sum()),
        "component_volume": np.bincount(component, volume_per_face, components) if f else np.zeros(0),
        "component_open": np.isin(np.arange(components), open_components),
        "component": component,
        "points": points,
        "faces": faces,
        "boundary": np.stack([boundary // n, boundary % n], axis=1) if len(boundary) else np.zeros((0, 2), np.int64),
    }


def measure_mesh(
    vertices: Sequence[Vector3],
    polygons: Iterable[Sequence[int]],
    *,
    area_epsilon: float = 1.0e-10,
) -> dict:
    """Measure face degeneracy, duplicate faces, edge manifoldness, winding, shells and volume.

    Pure Python on the given vertex indices. miswound_edges counts two-face edges whose faces run
    the same way; signed_volume is meaningful for closed meshes (negative means inside out)."""
    polygon_list = [tuple(int(vertex) for vertex in polygon) for polygon in polygons]
    edge_use: Counter[tuple[int, int]] = Counter()
    directed: Counter[tuple[int, int]] = Counter()
    face_keys: Counter[tuple[int, ...]] = Counter()
    degenerate_faces = 0
    parent = list(range(len(vertices)))
    volume = 0.0

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for polygon in polygon_list:
        if len(polygon) < 3:
            degenerate_faces += 1
            continue
        points = [vertices[index] for index in polygon]
        if polygon_area(points) <= area_epsilon:
            degenerate_faces += 1
        face_keys[tuple(sorted(polygon))] += 1
        for index, start in enumerate(polygon):
            end = polygon[(index + 1) % len(polygon)]
            edge_use[tuple(sorted((start, end)))] += 1
            directed[(start, end)] += 1
            root_a, root_b = find(start), find(end)
            if root_a != root_b:
                parent[root_a] = root_b
        a = points[0]
        for b, c in zip(points[1:-1], points[2:]):
            volume += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0])
                       + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6.0

    shells = {find(polygon[0]) for polygon in polygon_list if len(polygon) >= 3}
    open_shells = {find(edge[0]) for edge, count in edge_use.items() if count == 1}
    return {
        "vertices": len(vertices),
        "faces": len(polygon_list),
        "edges": len(edge_use),
        "boundary_edges": sum(1 for count in edge_use.values() if count == 1),
        "non_manifold_edges": sum(1 for count in edge_use.values() if count > 2),
        "degenerate_faces": degenerate_faces,
        "duplicate_faces": sum(count - 1 for count in face_keys.values() if count > 1),
        "miswound_edges": sum(1 for (u, v), count in directed.items()
                              if count > 1 and u < v and edge_use[(u, v)] == 2 or
                              count > 1 and u > v and edge_use[(v, u)] == 2),
        "components": len(shells),
        "open_components": len(open_shells),
        "signed_volume": volume,
    }
