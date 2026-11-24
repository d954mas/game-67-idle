"""The body surface behind the cloth: the backstop maps it gives a sim mesh at rest, and its signed distance in a
pose for the reach gates.

The declared body of B.3 is complete per body model. Until it exists the fallback is the model's own body
primitives, which have holes where the outfit generator deleted skin under the cloth. Every rule here says what it
does at a hole: a closest body point on an open boundary of the body surface is a hole's rim (or the body's own
edge), behind which the body is unknown."""
import math

import numpy as np

from garment_cook import BodySurface
from garment_proxy import closest_surface_points

from . import sim

# Below this a cloth vertex sits on the body and its direction to the closest point is noise; the body normal
# stands in for it.
ON_BODY_M = 1e-4


class Body:
    """A skinned body surface: rest points, triangles, vertex normals and dense weights on the model's joints.
    complete: True for a declared full body; False for the fallback, whose reach gates are reported, not enforced."""

    def __init__(self, points, faces, normals, weights, joints, complete):
        self.points, self.faces = np.asarray(points, float), np.asarray(faces, np.int64)
        self.normals = np.asarray(normals, float)
        self.weights, self.joints, self.complete = np.asarray(weights, float), list(joints), complete
        self.surface = BodySurface(self.points, self.faces, self.normals)
        rim = sim.boundary_edges(self.surface.welded)
        self._rim_edges = {tuple(e) for e in rim.tolist()}
        self._rim_points = np.zeros(len(self.surface.points), bool)
        self._rim_points[rim.ravel()] = True

    def closest(self, points):
        """Per point: (face, barycentrics, distance, closest point, interpolated unit normal, on_rim)."""
        face, bary, distance = closest_surface_points(np.asarray(points, float).reshape(-1, 3), self.points, self.faces)
        point = np.einsum("nk,nkj->nj", bary, self.points[self.faces[face]])
        normal = np.einsum("nk,nkj->nj", bary, self.normals[self.faces[face]])
        normal /= np.maximum(np.linalg.norm(normal, axis=1, keepdims=True), 1e-12)
        welded = self.surface.welded[face]
        rim = np.zeros(len(face), bool)
        for i, (corners, b) in enumerate(zip(welded, bary)):
            zero = b == 0
            if zero.sum() == 2:
                rim[i] = self._rim_points[corners[~zero][0]]
            elif zero.sum() == 1:
                k = int(np.flatnonzero(zero)[0])
                rim[i] = tuple(sorted((int(corners[(k + 1) % 3]), int(corners[(k + 2) % 3])))) in self._rim_edges
        return face, bary, distance, point, normal, rim

    def radius(self, face, point, normal):
        """The body radius behind a closest point (BodySurface.radius): +inf where the body is flat or concave there,
        0 where no geodesic disc determines it."""
        found = self.surface.radius(face, point, normal)
        return 0.0 if found is None else found


def backstop(points, faces, r, body, thickness):
    """Body direction b, backstop distance d and radius R per sim vertex at rest (spec B.4), and the hole mask.

    b points from the vertex to the closest body point, or along -n where the vertex lies on or inside the body,
    smoothed over the sim mesh; d is that distance less the fabric thickness (0 if negative); R is the body radius
    behind the vertex, set to 0 where d >= r (such a backstop never acts), on a pin, and at a hole: where the closest
    body point lies on an open boundary of the body, the body behind the vertex is unknown, the backstop is off and b
    points at the rim. body None: b is -n of the sim mesh and the backstop is off everywhere."""
    points, r = np.asarray(points, float), np.asarray(r, float)
    if body is None:
        return -sim.vertex_normals(points, faces), np.zeros(len(points)), np.zeros(len(points)), np.zeros(len(points), bool)
    face, _, distance, closest, normal, rim = body.closest(points)
    signed = np.einsum("nj,nj->n", points - closest, normal)
    toward = (closest - points) / np.maximum(distance, 1e-12)[:, None]
    direction = np.where(((signed > ON_BODY_M) & (distance > ON_BODY_M))[:, None], toward, -normal)
    direction = smooth_directions(direction, faces)
    d = np.maximum(np.maximum(signed, 0) - thickness, 0)
    R = np.zeros(len(points))
    for i in np.flatnonzero((r > 0) & (d < r) & ~rim):
        R[i] = body.radius(face[i], closest[i], normal[i])
    return direction, d, R, rim


def smooth_directions(directions, faces, iterations=4):
    """Average each direction with its sim-mesh neighbours, so the folds of a coarse mesh do not flip it."""
    n = len(directions)
    near = [set() for _ in range(n)]
    for a, b, c in np.asarray(faces):
        near[a] |= {b, c}
        near[b] |= {a, c}
        near[c] |= {a, b}
    out = np.asarray(directions, float).copy()
    for _ in range(iterations):
        mean = np.array([out[list(k)].mean(0) if k else out[i] for i, k in enumerate(near)])
        out = 0.5 * out + 0.5 * mean
        out /= np.maximum(np.linalg.norm(out, axis=1, keepdims=True), 1e-12)
    return out


def winding(points, triangles, chunk=24):
    """Generalised winding number of each point against a triangle soup (F, 3, 3) whose fronts face out: about 1
    inside a closed surface, 0 outside, in between near a hole (Jacobson et al. 2013)."""
    points = np.asarray(points, float).reshape(-1, 3)
    tri = np.asarray(triangles, float)
    out = np.zeros(len(points))
    for s in range(0, len(points), chunk):
        p = points[s:s + chunk, None, None, :]
        v = tri[None] - p
        a, b, c = v[:, :, 0], v[:, :, 1], v[:, :, 2]
        la, lb, lc = (np.linalg.norm(x, axis=2) for x in (a, b, c))
        det = np.einsum("pfi,pfi->pf", a, np.cross(b, c))
        den = la * lb * lc + np.einsum("pfi,pfi->pf", a, b) * lc + np.einsum("pfi,pfi->pf", b, c) * la \
            + np.einsum("pfi,pfi->pf", c, a) * lb
        out[s:s + chunk] = np.arctan2(det, den).sum(1) / (2 * math.pi)
    return out


def signed_distance(points, vertices, faces, inside=None):
    """Distance to the surface, negative inside (winding number above one half). inside: a precomputed mask."""
    points = np.asarray(points, float).reshape(-1, 3)
    if inside is None:
        inside = winding(points, vertices[faces]) > 0.5
    _, _, distance = closest_surface_points(points, vertices, faces)
    return np.where(inside, -distance, distance)
