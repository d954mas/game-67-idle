"""Pure-Python topology metrics shared by Blender audit scripts and tests."""

from __future__ import annotations

from collections import Counter
from math import sqrt
from typing import Iterable, Sequence


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


def measure_mesh(
    vertices: Sequence[Vector3],
    polygons: Iterable[Sequence[int]],
    *,
    area_epsilon: float = 1.0e-10,
) -> dict[str, int]:
    """Measure face degeneracy, duplicate faces, and edge manifoldness."""
    polygon_list = [tuple(int(vertex) for vertex in polygon) for polygon in polygons]
    edge_use: Counter[tuple[int, int]] = Counter()
    face_keys: Counter[tuple[int, ...]] = Counter()
    degenerate_faces = 0

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

    return {
        "vertices": len(vertices),
        "faces": len(polygon_list),
        "edges": len(edge_use),
        "boundary_edges": sum(1 for count in edge_use.values() if count == 1),
        "non_manifold_edges": sum(1 for count in edge_use.values() if count > 2),
        "degenerate_faces": degenerate_faces,
        "duplicate_faces": sum(count - 1 for count in face_keys.values() if count > 1),
    }
