"""Fit a convex cloth support to the connected GLB component nearest its top center."""
import argparse
import json
from pathlib import Path
import sys

import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
from scipy.spatial import ConvexHull, HalfspaceIntersection

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "ai_studio/assets/tools/blender/lib"))
from glb import Glb


def component_points(asset, top):
    triangles = []
    for i, transform in asset.world_rest().items():
        node = asset.nodes[i]
        if "mesh" not in node:
            continue
        for primitive in asset.json["meshes"][node["mesh"]]["primitives"]:
            points = asset.accessor(primitive["attributes"]["POSITION"])
            points = points @ transform[:3, :3].T + transform[:3, 3]
            triangles.extend(points[asset.primitive_faces(primitive)])
    points, indices = np.unique(np.reshape(triangles, (-1, 3)).round(6), axis=0, return_inverse=True)
    edges = indices.reshape(-1, 3)[:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2)
    graph = coo_matrix((np.ones(len(edges)), edges.T), shape=(len(points), len(points)))
    _, components = connected_components(graph, directed=False)
    nearest = np.linalg.norm(points - top, axis=1).argmin()
    return points[components == components[nearest]]


def fit_planes(points, top, error):
    # A subset of tangent half-spaces always encloses the source component.
    normals = np.unique(ConvexHull(points).equations[:, :3].round(5), axis=0)
    hull = np.column_stack((normals, -(points @ normals.T).max(axis=0)))
    axes = np.vstack((np.eye(3), -np.eye(3)))
    planes = np.column_stack((axes, -(points @ axes.T).max(axis=0)))
    interior = points.mean(axis=0)
    while True:
        vertices = HalfspaceIntersection(planes, interior).intersections
        gaps = vertices @ hull[:, :3].T + hull[:, 3]
        _, face = np.unravel_index(gaps.argmax(), gaps.shape)
        if gaps.max() <= error:
            break
        planes = np.vstack((planes, hull[face]))
    active = np.abs(vertices @ planes[:, :3].T + planes[:, 3]) < 1e-7
    planes = planes[active.sum(axis=0) >= 3]
    if len(planes) > 48:
        raise ValueError(f"{len(planes)} support faces exceed 48; increase --error")
    planes[:, 3] += planes[:, :3] @ top
    return planes.round(7).tolist()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("glb", type=Path)
    parser.add_argument("--top", nargs=3, type=float, required=True, metavar=("X", "Y", "Z"))
    parser.add_argument("--error", type=float, default=0.01, help="maximum excess distance per face, in meters")
    args = parser.parse_args()
    if not np.isfinite(args.error) or args.error <= 0:
        parser.error("--error must be finite and positive")
    print(json.dumps(fit_planes(component_points(Glb.load(args.glb), args.top), args.top, args.error)))
