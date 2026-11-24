"""The body surface behind the cloth: the backstop maps it gives a sim mesh at rest, and its signed distance in a
pose for the reach gates.

The declared body of B.3 is complete per body model. Until it exists the fallback is the model's own body
primitives, which have holes where the outfit generator deleted skin under the cloth. Every rule here says what it
does at a hole: a closest body point on an open boundary of the body surface is a hole's rim (or the body's own
edge), behind which the body is unknown."""
import math

import numpy as np
from scipy.sparse import csr_matrix
from scipy.sparse.csgraph import dijkstra

from . import sim
from .sim import closest_surface_points

# m: below a micron a gate's violation is the float32 storage of the sidecar (its step near 1 m is 6e-8), not content.
RESOLUTION = 1e-6
# Below this a cloth vertex sits on the body and its direction to the closest point is noise; the body normal
# stands in for it.
ON_BODY_M = 1e-4


# Smallest singular value over the largest of a curvature fit's design on steps scaled to its disc: below it the
# vertices barely span two tangent directions and the fitted curvature is noise.
FIT_CONDITION = 1e-3


class BodySurface:
    """The body behind the cloth at rest: triangles welded across attribute seams, so geodesic patches cross them."""

    def __init__(self, vertices, faces, normals):
        self.vertices, self.faces, self.normals = (np.asarray(vertices, float), np.asarray(faces), np.asarray(normals, float))
        _, first, weld = np.unique(np.round(self.vertices, 6), axis=0, return_index=True, return_inverse=True)
        weld = weld.ravel()
        self.points = self.vertices[first]
        self.welded = weld[self.faces]
        edges = np.unique(np.sort(np.concatenate([self.welded[:, [0, 1]], self.welded[:, [1, 2]],
                                                  self.welded[:, [2, 0]]]), axis=1), axis=0)
        edges = edges[edges[:, 0] != edges[:, 1]]
        length = np.linalg.norm(self.points[edges[:, 0]] - self.points[edges[:, 1]], axis=1)
        n = len(self.points)
        self.graph = csr_matrix((np.r_[length, length], (np.r_[edges[:, 0], edges[:, 1]], np.r_[edges[:, 1], edges[:, 0]])),
                                shape=(n, n))
        normals = np.zeros_like(self.points)
        np.add.at(normals, weld, self.normals)
        self.point_normals = normals / np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-12)

    def radius(self, face, point, normal):
        """The body radius behind point: 1 / the largest principal curvature there, from the shape operator S that
        carries a tangent step to the turn of the normal (dn = S dp), fitted to the body vertices nearest to point
        along the surface whose normals face the side of normal, on the smallest geodesic disc that determines S.
        The backstop is a local model of the body behind the vertex: a wider disc wraps past the side of a thin limb.
        The normals carry the smooth surface, so the tessellation does not set the radius. Returns +inf where the
        body is flat or concave there and None where no disc determines the fit."""
        seeds = self.welded[face]
        offset = np.linalg.norm(self.points[seeds] - point, axis=1)
        u = np.cross(normal, [1.0, 0.0, 0.0] if abs(normal[0]) < 0.9 else [0.0, 1.0, 0.0])
        u /= np.linalg.norm(u)
        v = np.cross(normal, u)
        limit = 2 * offset.max() + 1e-3
        while True:
            reach = dijkstra(self.graph, indices=seeds, limit=limit)
            geodesic = (reach + offset[:, None]).min(0)
            near = np.flatnonzero(np.isfinite(geodesic) & (self.point_normals @ normal > 0))
            near = near[np.argsort(geodesic[near], kind="stable")]
            rel, turn = self.points[near] - point, self.point_normals[near]
            x, y = rel @ u, rel @ v
            zero = np.zeros(len(x))
            # Unknowns (s11, s12, s22): rows of dn.u = s11 x + s12 y, then of dn.v = s12 x + s22 y.
            design = np.stack([np.column_stack([x, y, zero]), np.column_stack([zero, x, y])], axis=1)
            target = np.column_stack([turn @ u, turn @ v])
            for n in range(2, len(x) + 1):
                rows = design[:n].reshape(-1, 3)
                # Judged on steps scaled to the disc, so the units do not decide.
                span = max(float(np.abs(rows).max()), 1e-12)
                singular = np.linalg.svd(rows / span, compute_uv=False)
                if singular[-1] > FIT_CONDITION * singular[0]:
                    s11, s12, s22 = np.linalg.lstsq(rows, target[:n].reshape(-1), rcond=None)[0]
                    curvature = np.linalg.eigvalsh([[s11, s12], [s12, s22]]).max()
                    # A turn of the normal across the disc below what the fit resolves is a flat body; it also keeps
                    # R within the range a float sphere centre can carry.
                    return 1.0 / curvature if curvature * span > FIT_CONDITION else math.inf
            if limit > 1.0 or len(near) == len(self.points):
                return None
            limit *= 2


class Body:
    """A skinned body surface: rest points, triangles, vertex normals and dense weights on the model's joints.
    complete: True for a declared full body; False for the fallback, whose reach gates are reported, not enforced."""

    def __init__(self, points, faces, normals, weights, joints, complete):
        self.points, self.faces = np.asarray(points, float), np.asarray(faces, np.int64)
        self.normals = np.asarray(normals, float)
        self.weights, self.joints, self.complete = np.asarray(weights, float), list(joints), complete
        self.surface = BodySurface(self.points, self.faces, self.normals)
        # A declared body is complete: its open edges (the neck under the head, say) are not holes over the cloth.
        rim = sim.boundary_edges(self.surface.welded) if not complete else np.zeros((0, 2), np.int64)
        self._rim_edges = {tuple(e) for e in rim.tolist()}
        self._rim_points = np.zeros(len(self.surface.points), bool)
        self._rim_points[rim.ravel()] = True
        # Per body face: it touches a hole (always False on a complete body).
        self.rim_faces = self._rim_points[self.surface.welded].any(1)

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
        None where no geodesic disc determines it."""
        return self.surface.radius(face, point, normal)


def backstop(points, faces, pins, body, thickness):
    """Body direction b, backstop distance d and radius R per sim vertex at rest (spec B.4), the hole mask and the
    mask of free vertices whose body radius no disc determines.

    b points from the vertex to the closest body point, or along -n where the vertex lies on or inside the body,
    smoothed over the sim mesh; d is that distance less the fabric thickness (0 if negative); R is the body radius
    behind every free vertex, whatever its authored r, since the runtime may grow r (max-distance scale and bias). R is
    0 on a pin (it never moves) and at a hole: where the closest body point lies on an open boundary of the body, the
    body behind the vertex is unknown, the backstop is off and b points at the rim."""
    points, pins = np.asarray(points, float), np.asarray(pins, bool)
    direction, (face, closest, normal, rim, signed) = _directions(points, faces, body)
    d = np.maximum(np.maximum(signed, 0) - thickness, 0)
    R = np.zeros(len(points))
    unfit = np.zeros(len(points), bool)
    for i in np.flatnonzero(~pins & ~rim):
        found = body.radius(face[i], closest[i], normal[i])
        if found is None:
            unfit[i] = True
        else:
            R[i] = found
    return direction, d, R, rim, unfit


def directions(points, faces, body):
    """The body direction b of each sim vertex by backstop's rule, against any pose of the body (a posed Body): for a
    corrective, from the settled cloth to the body posed in the key."""
    return _directions(np.asarray(points, float), faces, body)[0]


def _directions(points, faces, body):
    face, _, distance, closest, normal, rim = body.closest(points)
    signed = np.einsum("nj,nj->n", points - closest, normal)
    toward = (closest - points) / np.maximum(distance, 1e-12)[:, None]
    direction = np.where(((signed > ON_BODY_M) & (distance > ON_BODY_M))[:, None], toward, -normal)
    return smooth_directions(direction, faces), (face, closest, normal, rim, signed)


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


def reach_violation(points, vertices, faces, r, thickness, rim_faces=None):
    """Gate G4 per point (m, > 0 fails): how far a reference lies deeper in the posed body than its max distance
    allows, sd(ref, body + thickness) < -r; a pin (r = 0) fails only inside the body itself, since it follows
    skinning and no body collider acts on it. The side is the generalised winding number (exact on a closed body).
    rim_faces: per body face, True where it touches a hole of a fallback body (Body.rim_faces); a point whose closest
    body point lies there has no known side and passes. A free vertex with r >= thickness fails only deeper than
    r - thickness, so one whose nearest body vertex (an upper bound of its distance) is nearer passes unsigned."""
    from scipy.spatial import cKDTree
    points, r = np.asarray(points, float), np.asarray(r, float)
    pins = r == 0
    near = cKDTree(vertices).query(points)[0]
    need = pins | (r < thickness) | (near > r - thickness)
    out = np.zeros(len(points))
    if need.any():
        q = points[need]
        inside = winding(q, vertices[faces]) > 0.5
        face, _, distance = closest_surface_points(q, vertices, faces)
        sd = np.where(inside, -distance, distance)
        v = np.where(pins[need], -sd, -(sd - thickness) - r[need])
        if rim_faces is not None:
            v = np.where(np.asarray(rim_faces)[face], 0.0, v)
        out[need] = v
    return out


def _segments_cross(p, q, a, b, c):
    """Per row: the open segment p->q passes through the inside of triangle abc (touching an edge or corner does not
    count, so triangles that share a corner do not cross there)."""
    d = q - p
    e1, e2 = b - a, c - a
    h = np.cross(d, e2)
    det = np.einsum("ij,ij->i", e1, h)
    scale = np.linalg.norm(e1, axis=1) * np.linalg.norm(e2, axis=1) * np.linalg.norm(d, axis=1)
    ok = np.abs(det) > 1e-12 * np.maximum(scale, 1e-300)
    inv = np.where(ok, 1 / np.where(ok, det, 1), 0)
    s = p - a
    u = np.einsum("ij,ij->i", s, h) * inv
    k = np.cross(s, e1)
    v = np.einsum("ij,ij->i", d, k) * inv
    t = np.einsum("ij,ij->i", e2, k) * inv
    eps = 1e-9
    return ok & (u > eps) & (v > eps) & (u + v < 1 - eps) & (t > eps) & (t < 1 - eps)


def prove(points, faces):
    """What the reach gates need from a body to be exact, as a list of failures ([] when it holds): the generalised
    winding number is 1 inside and 0 outside only on closed surfaces whose triangles face out consistently and do not
    pass through each other. Vertices are welded by exact position, as the winding number sees one surface.
    Checked per connected part: no degenerate triangle, every edge shared by at most two triangles and, where two,
    walked in opposite directions; closed (no open edge); positive enclosed volume (faces out); no two triangles of
    the part crossing. Two closed parts may overlap: the winding number there is 2 and still inside."""
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import connected_components
    from scipy.spatial import cKDTree
    points = np.asarray(points, float)
    _, node = np.unique(points, axis=0, return_inverse=True)
    node = node.ravel()
    tri = node[np.asarray(faces, np.int64)]
    failures = []
    degenerate = (tri[:, 0] == tri[:, 1]) | (tri[:, 1] == tri[:, 2]) | (tri[:, 2] == tri[:, 0])
    if degenerate.any():
        failures.append(f"{int(degenerate.sum())} triangles with a repeated corner")
        tri = tri[~degenerate]
    position = np.zeros((node.max() + 1, 3))
    position[node] = points
    directed = np.concatenate([tri[:, [0, 1]], tri[:, [1, 2]], tri[:, [2, 0]]])
    owner = np.tile(np.arange(len(tri)), 3)
    key = np.sort(directed, axis=1)
    edges, inverse, count = np.unique(key, axis=0, return_inverse=True, return_counts=True)
    inverse = inverse.ravel()
    if (count > 2).any():
        failures.append(f"{int((count > 2).sum())} edges shared by more than two triangles")
    forward = (directed[:, 0] < directed[:, 1]).astype(int)
    same = np.bincount(inverse, weights=forward, minlength=len(edges))
    flipped = (count == 2) & (same != 1)
    if flipped.any():
        failures.append(f"{int(flipped.sum())} edges walked the same way by both triangles (orientation flips)")
    pairs = np.flatnonzero(count == 2)
    sides = np.argsort(inverse, kind="stable")
    start = np.r_[0, np.cumsum(count)[:-1]]
    a_face, b_face = owner[sides[start[pairs]]], owner[sides[start[pairs] + 1]]
    graph = coo_matrix((np.ones(len(pairs)), (a_face, b_face)), shape=(len(tri), len(tri)))
    _, part = connected_components(graph, directed=False)
    open_parts = np.unique(part[owner[np.isin(inverse, np.flatnonzero(count == 1))]])
    if len(open_parts):
        failures.append(f"{len(open_parts)} parts with open edges ({int((count == 1).sum())} open edges)")
    corners = position[tri]
    volume = np.bincount(part, weights=np.einsum("ij,ij->i", corners[:, 0], np.cross(corners[:, 1], corners[:, 2])) / 6)
    inverted = np.setdiff1d(np.flatnonzero(volume <= 0), open_parts)
    if len(inverted):
        failures.append(f"{len(inverted)} closed parts enclose a negative volume (they face in)")
    centre = corners.mean(1)
    reach = np.linalg.norm(corners - centre[:, None], axis=2).max(1)
    tree = cKDTree(centre)
    near = tree.query_ball_point(centre, reach + reach.max())
    i = np.repeat(np.arange(len(tri)), [len(n) for n in near])
    j = np.concatenate([np.asarray(n, np.int64) for n in near]) if len(near) else np.zeros(0, np.int64)
    keep = (j > i) & (part[i] == part[j])
    i, j = i[keep], j[keep]
    keep = np.linalg.norm(centre[i] - centre[j], axis=1) <= reach[i] + reach[j]
    i, j = i[keep], j[keep]
    shared = (tri[i][:, :, None] == tri[j][:, None, :]).any(2).sum(1)
    i, j = i[shared < 2], j[shared < 2]
    crossing = np.zeros(len(i), bool)
    for s in range(0, len(i), 200000):
        a, b = corners[i[s:s + 200000]], corners[j[s:s + 200000]]
        hit = np.zeros(len(a), bool)
        for x, y in ((a, b), (b, a)):
            for k in range(3):
                hit |= _segments_cross(x[:, k], x[:, (k + 1) % 3], y[:, 0], y[:, 1], y[:, 2])
        crossing[s:s + 200000] = hit
    if crossing.any():
        failures.append(f"{int(crossing.sum())} pairs of triangles of one part cross each other")
    return failures


def signed_distance(points, vertices, faces, inside=None):
    """Distance to the surface, negative inside (winding number above one half). inside: a precomputed mask."""
    points = np.asarray(points, float).reshape(-1, 3)
    if inside is None:
        inside = winding(points, vertices[faces]) > 0.5
    _, _, distance = closest_surface_points(points, vertices, faces)
    return np.where(inside, -distance, distance)
