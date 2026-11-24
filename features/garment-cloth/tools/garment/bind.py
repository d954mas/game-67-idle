"""The render binding grown over the render mesh (spec B.7): every labelled render vertex follows one sim triangle,
chosen so that render neighbours bind to places the sim mesh keeps close.

A render vertex binds to the closest sim triangle among those whose sim-mesh path from its parent's bound triangle
is at most (2/sqrt 3)|e| + la + lb: |e| is the render edge, l the longest edge of each triangle, and 2/sqrt 3 the
largest ratio of path to straight distance on an equilateral mesh. A path between two triangles is the shortest
edge path between their corners, so it never crosses an open boundary of the sim mesh: the binding cannot jump a
slit or reach another layer of a fold. The same bound is gate G3 for every render edge."""
from collections import deque

import numpy as np
from scipy.sparse.csgraph import dijkstra
from scipy.spatial import cKDTree

from . import sim
from .sim import _closest_on_triangles

PATH_STRETCH = 2 / np.sqrt(3)


class Binding:
    """Per render vertex of a primitive: the bound sim triangle (-1: not this piece) and barycentrics.

    sim_points, sim_faces: the piece's sim mesh; tri_path: (F, F) shortest corner-to-corner path between sim
    triangles; longest: the longest edge of each sim triangle."""

    def __init__(self, sim_points, sim_faces, triangle, bary):
        self.sim_points, self.sim_faces = sim_points, sim_faces
        self.triangle, self.bary = triangle, bary
        self.tri_path, self.longest = triangle_paths(sim_points, sim_faces)

    def allowed(self, edge_length, a, b):
        """The bound of G3 for render edges of these lengths between triangles a and b."""
        return PATH_STRETCH * edge_length + self.longest[a] + self.longest[b]

    def amount(self, kinematic):
        """The interpolated 'not kinematic' share of each bound render vertex (0 off the piece)."""
        out = np.zeros(len(self.triangle))
        on = self.triangle >= 0
        out[on] = np.einsum("nk,nk->n", self.bary[on], (~kinematic[self.sim_faces[self.triangle[on]]]).astype(float))
        return out


def triangle_paths(points, faces):
    graph = sim.edge_graph(points, faces)
    vertex = dijkstra(graph, directed=False)
    out = np.full((len(faces), len(faces)), np.inf)
    for i in range(3):
        for j in range(3):
            out = np.minimum(out, vertex[np.ix_(faces[:, i], faces[:, j])])
    side = np.stack([np.linalg.norm(points[faces[:, k]] - points[faces[:, (k + 1) % 3]], axis=1) for k in range(3)], 1)
    return out, side.max(1)


def render_links(positions, faces, take):
    """Edges of the render growth graph between vertices the piece takes: the triangle edges, plus a zero-length
    link between two vertices at one position where their triangles are sewn along an edge (a UV or normal split
    of one surface; a vent or a cut whose lips only touch at a point stays apart)."""
    pairs = set()
    keys = [tuple(k) for k in np.round(positions, 7)]
    directed = {}
    for tri in faces:
        for k in range(3):
            u, v = int(tri[k]), int(tri[(k + 1) % 3])
            if take[u] and take[v]:
                pairs.add((min(u, v), max(u, v)))
                directed.setdefault((keys[u], keys[v]), []).append((u, v))
    for (ku, kv), rows in directed.items():
        for u, v in rows:
            for v2, u2 in directed.get((kv, ku), []):
                for a, b in ((u, u2), (v, v2)):
                    if a != b:
                        pairs.add((min(a, b), max(a, b)))
    return np.array(sorted(pairs), np.int64).reshape(-1, 2)


def grow(positions, faces, take, sim_points, sim_faces, kinematic):
    """The Binding of one primitive's render vertices `take` to a piece's sim mesh."""
    positions = np.asarray(positions, float)
    triangle = np.full(len(positions), -1)
    bary = np.zeros((len(positions), 3))
    binding = Binding(sim_points, sim_faces, triangle, bary)
    links = render_links(positions, faces, take)
    near = [[] for _ in positions]
    for a, b in links:
        near[a].append(b)
        near[b].append(a)
    tri = sim_points[sim_faces]
    pinned = np.flatnonzero(kinematic)
    tree = cKDTree(sim_points[pinned]) if len(pinned) else cKDTree(sim_points)
    gap = tree.query(positions)[0]

    def place(v, candidates):
        q, b = _closest_on_triangles(np.repeat(positions[v][None], len(candidates), 0), tri[candidates, 0],
                                     tri[candidates, 1], tri[candidates, 2])
        k = int(np.argmin(np.linalg.norm(q - positions[v], axis=1)))
        triangle[v], bary[v] = candidates[k], b[k]

    everyone = np.arange(len(sim_faces))
    remaining = set(np.flatnonzero(take).tolist())
    while remaining:
        # A new component of the render growth graph starts at its vertex closest to a kinematic sim vertex.
        component, queue = {min(remaining)}, deque([min(remaining)])
        while queue:
            u = queue.popleft()
            for v in near[u]:
                if v not in component:
                    component.add(v)
                    queue.append(v)
        seed = min(component, key=lambda v: (gap[v], v))
        remaining -= component
        place(seed, everyone)
        queue = deque([seed])
        while queue:
            u = queue.popleft()
            for v in near[u]:
                if triangle[v] >= 0:
                    continue
                length = np.linalg.norm(positions[v] - positions[u])
                limit = binding.allowed(length, triangle[u], everyone)
                place(v, np.flatnonzero(binding.tri_path[triangle[u]] <= limit))
                queue.append(v)
    return binding


def torn(binding, positions, faces, take):
    """Render edges (G3) whose bound triangles lie farther apart along the sim mesh than the bound allows, with that
    excess in metres: (edges (n, 2), excess (n,))."""
    links = render_links(positions, faces, take)
    a, b = binding.triangle[links[:, 0]], binding.triangle[links[:, 1]]
    length = np.linalg.norm(positions[links[:, 0]] - positions[links[:, 1]], axis=1)
    excess = binding.tri_path[a, b] - binding.allowed(length, a, b)
    bad = excess > 1e-9
    return links[bad], excess[bad]


def seam_breaks(binding, must, kinematic):
    """Render vertices that must follow skinning (`must`: the seam beside unlabelled vertices and the split copies of
    them) bound to a triangle with a free corner (G2): they would move away from the skinned vertex beside them.
    Returns (vertices, their amount)."""
    amount = binding.amount(kinematic)
    bad = np.flatnonzero(np.asarray(must, bool) & (binding.triangle >= 0) & (amount > 0))
    return bad, amount[bad]


def pack(v0, v1, v2, w0, w1, w2, amount=1.0):
    """One `_GARMENT` row: three 1-based sim vertex ids (11 bits each) and three weights (10 bits each) that sum to
    `amount` x 1023, the cloth's share against plain skinning; bit 31 of the high word marks the 3-vertex form. A
    vertex whose quantised weight is 0 is written as id 0."""
    if not (0 <= v0 <= 2047 and 0 <= v1 <= 2047 and 0 <= v2 <= 2047):
        raise ValueError("sim vertex id exceeds the 11-bit render palette (2047)")
    total = int(round(float(np.clip(amount, 0.0, 1.0)) * 1023.0))
    ws = np.maximum(np.array([w0, w1, w2], float), 0.0)
    if total <= 0 or ws.sum() <= 1e-12:
        return (0, 0, 0, 0)
    raw = ws / ws.sum() * total
    q = np.floor(raw).astype(int)
    for k in np.argsort(-(raw - q))[:total - int(q.sum())]:
        q[k] += 1
    v0, v1, v2 = (v if w else 0 for v, w in zip((v0, v1, v2), q))
    lo = (v0 & 0x7FF) | ((v1 & 0x7FF) << 11) | ((int(q[0]) & 0x3FF) << 22)
    hi = (v2 & 0x7FF) | ((int(q[1]) & 0x3FF) << 11) | ((int(q[2]) & 0x3FF) << 21) | (1 << 31)
    return (lo & 0xFFFF, (lo >> 16) & 0xFFFF, hi & 0xFFFF, (hi >> 16) & 0xFFFF)
