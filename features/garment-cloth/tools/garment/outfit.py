"""Authoring of one outfit's cloth: an `Outfit` on a render model and its `Piece`s, each a sim mesh held as numpy
arrays with its maps and skin weights, edited by plain functions and written as the sidecar `<model>.cloth.glb`.

An outfit script is the source of truth; the sidecar is its built artifact. A piece holds per vertex:
- x: rest position (model space, metres, Y-up); faces: triangles whose front faces away from the body;
- r: max distance (m); 0 is kinematic, so the pins are exactly r == 0;
- d, R, b: backstop distance and radius (0 off, +inf the plane form) and the unit body direction;
- w: dense skin weights (vertices, model skin joints).
A value not yet authored is NaN (weights: an all-zero row); the writer names the map and the vertices left so.
Masks are numpy boolean arrays over `piece.f` fields; every map and weight write records the script line that made
it, so `piece.why(i)` and the cards answer why a vertex is pinned, held or weighted as it is.

Paths are relative to the game root. GARMENT_SIDECAR_DIR, when set, is where `write` puts the sidecar instead of
beside the model (a rebuild check, a scratch build)."""
import hashlib
import json
import linecache
import os
import sys
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree

from garment_blob import single
from . import body as body_maps
from . import sidecar as sc
from . import sim
from .sheet import simplify
from .sim import geodesic_depth
from .model import RenderModel, Surface

PACKAGE = Path(__file__).resolve().parent
FEATURE = PACKAGE.parents[1]
LIBRARY_VERSION = json.loads((FEATURE / "feature.json").read_text(encoding="utf-8"))["version"]
INFLUENCES = 4
MAX_DISTANCE_MAX = 1.0  # m: the blob refuses more; a larger r is no longer motion around a reference


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _site():
    """The first caller line outside this package: what an agent reads as the reason for a value."""
    frame = sys._getframe(1)
    while frame is not None and Path(frame.f_code.co_filename).resolve().parent == PACKAGE:
        frame = frame.f_back
    if frame is None:
        return "?"
    name = frame.f_code.co_filename
    return f"{Path(name).name}:{frame.f_lineno}: {linecache.getline(name, frame.f_lineno).strip()}"


def top_influences(weights, count=INFLUENCES):
    """The `count` largest weights per row, renormalized; ties keep the lower joint first, and an empty lane names
    the strongest joint, so no lane addresses a joint the vertex does not use."""
    weights = np.asarray(weights, float)
    if weights.shape[1] < count:
        weights = np.pad(weights, ((0, 0), (0, count - weights.shape[1])))
    order = np.argsort(-weights, axis=1, kind="stable")[:, :count]
    values = np.take_along_axis(weights, order, axis=1)
    values /= np.maximum(values.sum(1, keepdims=True), 1e-30)
    order = np.where(values == 0, order[:, :1], order)
    return order, values


class Fields:
    """Per-vertex fields an agent builds masks from (all arrays over the piece's current vertices)."""

    def __init__(self, piece):
        self._p = piece

    x = property(lambda s: s._p.x[:, 0])
    y = property(lambda s: s._p.x[:, 1])
    z = property(lambda s: s._p.x[:, 2])
    r = property(lambda s: s._p.r)
    d = property(lambda s: s._p.d)
    R = property(lambda s: s._p.R)
    all = property(lambda s: np.ones(len(s._p.x), bool))
    boundary = property(lambda s: sim.boundary(s._p.faces, len(s._p.x)))

    @property
    def geo(self):
        """Geodesic distance over the sim mesh to the nearest pin (inf where none is reached)."""
        return sim.geodesic(self._p.x, self._p.faces, self._p.pins)

    @property
    def seam(self):
        """Distance to the nearest render vertex the piece is sewn to (an unlabelled corner of its region)."""
        render = self._p.render
        sewn = np.unique(render.region)
        sewn = sewn[~render.labelled[sewn]]
        if not len(sewn):
            return np.full(len(self._p.x), np.inf)
        return cKDTree(render.positions[sewn]).query(self._p.x)[0]

    @property
    def gap(self):
        """Distance to the body surface at rest."""
        return self._p.outfit.body.closest(self._p.x)[2]

    @property
    def w(self):
        """Weight columns by joint name, with or without the rig prefix: `f.w["Hips"]`."""
        return _Columns(self._p)

    @property
    def dominant(self):
        """The short name of each vertex's strongest joint."""
        names = np.array([j.split(":")[-1] for j in self._p.joints])
        return names[np.argmax(self._p.w, axis=1)]

    def sector(self, start_deg, end_deg, center_xz=(0.0, 0.0)):
        """Vertices whose azimuth about a vertical axis lies in [start, end): 0 at the front (+Z), 90 at her left (+X)."""
        x, z = self._p.x[:, 0] - center_xz[0], self._p.x[:, 2] - center_xz[1]
        a = np.degrees(np.arctan2(x, z)) % 360
        start, end = start_deg % 360, end_deg % 360
        return (a >= start) & (a < end) if start <= end else (a >= start) | (a < end)


class _Columns:
    def __init__(self, piece):
        self._p = piece

    def __getitem__(self, name):
        return self._p.w[:, self._p.joint(name)]


class Piece:
    """One sim mesh with its maps and weights. Build it with `create` (from the labelled render region) or
    `set_mesh` (points and triangles an agent drew), then edit it with the operations below."""

    def __init__(self, outfit, piece_id, label, fabric, thickness, joints):
        self.outfit, self.id, self.label, self.fabric = outfit, piece_id, int(label), fabric
        self.thickness, self.joints = float(thickness), list(joints)
        self.render = None
        self.sites = []
        self.f = Fields(self)
        self.correctives = {}
        self.x = np.zeros((0, 3))
        self.faces = np.zeros((0, 3), np.int64)
        self._fresh()

    # Bookkeeping ---------------------------------------------------------------------------------------------
    def _fresh(self):
        n = len(self.x)
        self.r, self.d, self.R = (np.full(n, np.nan) for _ in range(3))
        self.b = np.full((n, 3), np.nan)
        self.w = np.zeros((n, len(self.joints)))
        self.origin = {k: np.full(n, -1) for k in ("mesh", "r", "backstop", "w")}

    def _mark(self, kind, mask):
        site = _site()
        if site not in self.sites:
            self.sites.append(site)
        self.origin[kind][np.asarray(mask, bool)] = self.sites.index(site)

    def _mask(self, mask):
        mask = self.f.all if mask is None else np.asarray(mask)
        if mask.dtype != bool or mask.shape != (len(self.x),):
            raise ValueError(f"{self.id}: a mask is a boolean array over the {len(self.x)} sim vertices")
        return mask

    def joint(self, name):
        """Index of a joint by its full or short name."""
        for j, full in enumerate(self.joints):
            if name in (full, full.split(":")[-1]):
                return j
        raise KeyError(f"{self.id}: no joint {name!r} in the model's skin")

    @property
    def pins(self):
        return self.r == 0

    def why(self, vertex):
        """The script lines behind one vertex: its mesh, max distance, backstop and weights."""
        out = {}
        for kind, rows in self.origin.items():
            out[kind] = self.sites[rows[vertex]] if rows[vertex] >= 0 else "not set"
        out["values"] = dict(r=float(self.r[vertex]), d=float(self.d[vertex]), R=float(self.R[vertex]),
                             dominant=self.f.dominant[vertex] if self.w[vertex].any() else None)
        return out

    def _topology(self, points, faces, P):
        """Adopt a new mesh: weights follow by interpolation, a new vertex is a pin when all its sources are, and the
        other maps are unset until a map rule writes them again."""
        P = P.tocsr()
        pin_source = (P @ (~self.pins).astype(float)) == 0
        w = P @ self.w
        origin = {k: v[np.asarray(P.argmax(axis=1)).ravel()] for k, v in self.origin.items()}
        self.x, self.faces = np.asarray(points, float), np.asarray(faces, np.int64)
        self._fresh()
        self.w = w / np.maximum(w.sum(1, keepdims=True), 1e-30)
        self.origin.update(origin)
        self.r[pin_source] = 0
        self._mark("mesh", self.f.all)

    # Sim mesh ------------------------------------------------------------------------------------------------
    def create(self, source, vertices, outer=None):
        """Make the sim mesh from the render region of this piece's label in the `source` primitive: its outer sewn
        sheet (`outer`: {"axis_xz_m": [x, z]} keeps triangles facing away from a vertical axis, {"outward": v} those
        facing v; "min_cosine" drops a rim that barely faces out), welded where the render mesh is sewn and open at
        boundaries and slits, reduced to `vertices` by edge collapse (or split up to it) and evened out."""
        self.render = self.outfit.model.piece(self.label, source, outer)
        points, faces, _ = simplify(self.render.sheet_points, self.render.sheet_faces, vertices)
        self.x, self.faces = np.asarray(points, float), np.asarray(faces, np.int64)
        self._fresh()
        self._mark("mesh", self.f.all)

    def set_mesh(self, points, faces):
        """Use points (V, 3) and counter-clockwise triangles (F, 3) as the sim mesh; every map starts unset."""
        self.x, self.faces = np.asarray(points, float).reshape(-1, 3), np.asarray(faces, np.int64).reshape(-1, 3)
        self._fresh()
        self._mark("mesh", self.f.all)

    def cut(self, path):
        """Open the mesh along a path of vertex ids joined by edges (see `path`); a path ending inside the mesh
        leaves its end whole, the tip of a slit."""
        self._topology(*sim.cut(self.x, self.faces, path))

    def density(self, mask, edge_mm):
        """Bring the edges inside the mask toward edge_mm by splitting long and collapsing short ones."""
        self._topology(*sim.density(self.x, self.faces, self._mask(mask), edge_mm / 1000))

    def relax(self, mask, iterations=4):
        """Even out the masked interior vertices on the surface; boundaries keep their line. Maps stay."""
        self.x = sim.relax(self.x, self.faces, self._mask(mask), iterations)
        self._mark("mesh", self._mask(mask))

    def offset(self, mask, mm):
        """Move the masked vertices along their normals, outward for positive mm (a number or one per vertex). Maps
        stay; rerun `backstop_from_body` when the gap to the body matters."""
        self.x = sim.offset(self.x, self.faces, self._mask(mask), mm / 1000)
        self._mark("mesh", self._mask(mask))

    def delete(self, mask):
        """Remove the masked vertices and their triangles."""
        self._topology(*sim.delete(self.x, self.faces, self._mask(mask)))

    def path(self, start, end):
        """Vertex ids of the shortest edge path between two vertices, for `cut`."""
        return sim.shortest_path(self.x, self.faces, start, end)

    def nearest(self, points):
        """The sim vertex nearest to each point."""
        return cKDTree(self.x).query(np.asarray(points, float).reshape(-1, 3))[1]

    def from_render(self, mask):
        """Carry a mask over the render vertices of the source primitive to the sim mesh: a sim vertex is selected
        when every render corner of the sheet triangle closest to it (each corner it depends on) is selected."""
        face, bary, _ = sim.closest(self.x, self.render.sheet_points, self.render.sheet_faces)
        corner = np.asarray(mask, bool)[self.render.sheet_render][self.render.sheet_faces[face]]
        return ~((bary * ~corner).sum(1) > 0)

    # Maps ----------------------------------------------------------------------------------------------------
    def pin(self, mask):
        """Make the masked vertices kinematic: r = 0."""
        mask = self._mask(mask)
        self.r[mask] = 0
        self._mark("r", mask)

    def pin_bound(self, height):
        """Pin the sim vertices that carry render vertices which must follow skinning, in every primitive with the
        label: the seam (labelled vertices beside unlabelled ones), their split copies and the band within `height`
        of the top of the piece's height. Afterwards each of them binds only to kinematic corners (amount 0, gate G2),
        so the seam stays exact. Repeats until the grown binding, which starts from the pins, adds no pin."""
        from . import bind
        model = self.outfit.model
        before = self.pins.copy()
        for _ in range(8):
            added = False
            for k, marks in enumerate(model.marks(self.label)):
                if marks is None:
                    continue
                must = marks["labelled"] & (marks["seam"] | marks["split"] | (marks["height"] <= height))
                # Bound as the cook will bind it: on the sim mesh as the sidecar stores it.
                binding = bind.grow(model.positions[k], model.faces[k], marks["labelled"], single(self.x).astype(float),
                                    self.faces, self.pins)
                carried = self.faces[binding.triangle[must]][binding.bary[must] > 0]
                new = np.setdiff1d(carried, np.flatnonzero(self.pins))
                if len(new):
                    self.r[new] = 0
                    added = True
            if not added:
                break
        self._mark("r", self.pins & ~before)
        return int((self.pins & ~before).sum())

    def unpin(self, mask):
        """Free the masked pins; their r is unset until `gradient` or `paint` writes it."""
        mask = self._mask(mask) & self.pins
        self.r[mask] = np.nan
        self._mark("r", mask)

    def gradient(self, rate, cap, mask=None, start=0.0, base=0.0):
        """r = min(base + rate x (geodesic distance to the pins - start), cap) on the masked free vertices where that
        distance is past `start`: the cloth may move `rate` metres per metre of fabric from where it is held, up to
        `cap` at the hem. The defaults give the plain gradient; start and base continue it from a depth on, so a
        piece can stay tight to a line and open below it:
            p.gradient(rate=0.1, cap=0.04)                                    # tight down to the hip line
            p.gradient(rate=0.5, cap=0.16, mask=p.f.y < HIP, start=depth_of_hip, base=0.04)   # then opening
        (depth_of_hip: p.f.geo at the hip line, e.g. its median over the vertices there)."""
        mask = self._mask(mask) & ~self.pins
        if not self.pins.any():
            raise ValueError(f"{self.id}: pin the piece before its max-distance gradient")
        geo, _ = geodesic_depth(self.x, self.faces, np.flatnonzero(self.pins))
        mask &= geo >= start
        self.r[mask] = np.minimum(base + rate * (geo - start), cap)[mask]
        self._mark("r", mask)

    def paint(self, name, mask, value):
        """Set map `name` ("r", "d" or "R") to value (a number or a per-vertex array) on the mask."""
        mask = self._mask(mask)
        target = {"r": self.r, "d": self.d, "R": self.R}[name]
        target[mask] = np.broadcast_to(np.asarray(value, float), target.shape)[mask]
        self._mark("r" if name == "r" else "backstop", mask)

    def smooth(self, name, mask, iterations=1):
        """Average map `name` with its sim-mesh neighbours on the mask; pins stay pins when smoothing r."""
        mask = self._mask(mask)
        target = {"r": self.r, "d": self.d, "R": self.R}[name]
        if name == "r":
            mask = mask & ~self.pins
        adjacency = sim.neighbours(self.faces, len(self.x)).astype(float)
        count = np.maximum(np.asarray(adjacency.sum(1)).ravel(), 1)
        for _ in range(iterations):
            target[mask] = 0.5 * target[mask] + 0.5 * (adjacency @ target / count)[mask]
        self._mark("r" if name == "r" else "backstop", mask)

    def backstop_from_body(self):
        """b, d and R from the outfit's body surface at rest (spec B.4; at a hole the backstop is off, see
        body.backstop). Needs r, since a backstop with d >= r never acts. Returns the count of hole vertices."""
        if np.isnan(self.r).any():
            raise ValueError(f"{self.id}: author r on every vertex before the backstop")
        self.b, self.d, self.R, hole = body_maps.backstop(self.x, self.faces, self.r, self.outfit.body, self.thickness)
        self._mark("backstop", self.f.all)
        return int((hole & ~self.pins).sum())

    # Weights -------------------------------------------------------------------------------------------------
    def weights_from(self, surface, nearest=None):
        """Weights a transfer from `surface` (a model.Surface on the model's skin) would give, without assigning.

        nearest=None: the skin weights at the closest surface point, sampled from each boundary vertex nudged into
        its own triangles, so a vertex on one lip of a cut reads its own side. nearest=k: the inverse-square blend of
        the k nearest surface vertices (the rule the cook used before the sidecar)."""
        if surface.joints != self.joints:
            raise ValueError(f"{self.id}: the surface is skinned on other joints than the model")
        if nearest:
            distance, near = cKDTree(surface.points).query(self.x, k=min(nearest, len(surface.points)))
            distance, near = distance.reshape(len(self.x), -1), near.reshape(len(self.x), -1)
            inverse = 1 / np.maximum(distance, 1e-4) ** 2
            return np.einsum("nk,nkj->nj", inverse, surface.weights[near]) / inverse.sum(1, keepdims=True)
        query = self.x.copy()
        edge = sim.boundary(self.faces, len(self.x))
        if edge.any():
            ring = sim.neighbours(self.faces, len(self.x)).astype(float)
            mean = (ring @ self.x) / np.maximum(np.asarray(ring.sum(1)), 1)
            query[edge] += 0.25 * (mean[edge] - self.x[edge])
        face, bary, _ = sim.closest(query, surface.points, surface.faces)
        return np.einsum("nk,nkj->nj", bary, surface.weights[surface.faces[face]])

    def weights_transfer(self, surface, mask=None, nearest=None):
        """Assign the weights of `weights_from(surface, nearest)` on the mask."""
        mask = self._mask(mask)
        self.w[mask] = self.weights_from(surface, nearest)[mask]
        self._mark("w", mask)

    def weights_set(self, mask, joint, weight):
        """Give `joint` the weight on the mask and scale the vertex's other joints to the rest."""
        mask, j = self._mask(mask), self.joint(joint)
        others = self.w[mask].copy()
        others[:, j] = 0
        total = others.sum(1, keepdims=True)
        if weight < 1 and np.any(total == 0):
            raise ValueError(f"{self.id}: a masked vertex has no other joint to take the remaining weight")
        others *= (1 - weight) / np.maximum(total, 1e-30)
        others[:, j] = weight
        self.w[mask] = others
        self._mark("w", mask)

    def weights_blend(self, mask, a, b, by):
        """w = (1 - by) a + by b on the mask: a and b are weight arrays (from `weights_from` or a copy of `w`), by a
        number or a per-vertex array in [0, 1]."""
        mask = self._mask(mask)
        by = np.broadcast_to(np.asarray(by, float), (len(self.x),))[:, None]
        blended = (1 - by) * np.asarray(a, float) + by * np.asarray(b, float)
        self.w[mask] = (blended / np.maximum(blended.sum(1, keepdims=True), 1e-30))[mask]
        self._mark("w", mask)

    def weights_smooth(self, mask, iterations=1):
        """Average the weights with their sim-mesh neighbours on the mask, `iterations` times."""
        mask = self._mask(mask)
        adjacency = sim.neighbours(self.faces, len(self.x)).astype(float)
        count = np.maximum(np.asarray(adjacency.sum(1)), 1)
        for _ in range(iterations):
            self.w[mask] = (0.5 * self.w + 0.5 * (adjacency @ self.w) / count)[mask]
        self._mark("w", mask)

    def weights_average(self, mask, radius):
        """Replace each masked vertex's weights by the mean of the weights of every vertex within `radius` of it along
        the sim mesh (itself included): a number in metres, or a per-vertex array such as `r`. Unlike
        `weights_smooth`, whose one-ring passes blur by the same amount everywhere, this bounds how fast the weights
        may change across the cloth by each vertex's own radius."""
        mask = self._mask(mask)
        radius = np.broadcast_to(np.asarray(radius, float), (len(self.x),))
        reach = sim.dijkstra_all(self.x, self.faces, limit=float(radius[mask].max(initial=0)))
        source = self.w.copy()
        for i in np.flatnonzero(mask):
            self.w[i] = source[reach[i] <= radius[i]].mean(0)
        self._mark("w", mask)

    def weights_follow_body(self):
        """The default weights of a piece with a body behind it: every vertex takes the skin weights of the closest
        point of the outfit's body surface, each free vertex then averages them within its own max distance r (the
        reference then changes across the cloth no faster than the cloth may move, so two limbs cannot pull an edge
        apart beyond what its two spheres allow), and the pins keep the weights they had (the seam with the skinned
        garment). Needs the pins and r. A piece with no body behind it (a ribbon) keeps its own weights: do not call
        this for it."""
        if np.isnan(self.r).any():
            raise ValueError(f"{self.id}: author r on every vertex before weights_follow_body")
        own, pins = self.w.copy(), self.pins
        body = self.outfit.body
        self.weights_transfer(Surface(body.points, body.faces, body.weights, body.joints))
        self.weights_average(~pins, self.r)
        self.weights_blend(pins, own, own, 0.0)

    # Built form ----------------------------------------------------------------------------------------------
    def built(self):
        """The sidecar record, after the checks every value must pass."""
        n = len(self.x)
        problems = []
        for name in ("r", "d", "R"):
            bad = np.flatnonzero(np.isnan(getattr(self, name)))
            if len(bad):
                problems.append(f"{name} unset on {len(bad)} vertices (first {bad[:5].tolist()})")
        if np.isnan(self.b).any():
            problems.append("b unset: run backstop_from_body")
        if np.any(self.r < 0) or np.any(self.r > MAX_DISTANCE_MAX) or np.any(self.d < 0) or np.any(self.R < 0):
            problems.append(f"r must lie in [0, {MAX_DISTANCE_MAX}] m, d and R must be non-negative")
        empty = np.flatnonzero(self.w.sum(1) <= 0)
        if len(empty):
            problems.append(f"no weights on {len(empty)} vertices (first {empty[:5].tolist()})")
        if problems:
            raise ValueError(f"{self.id}: " + "; ".join(problems))
        order, values = top_influences(self.w)
        return sc.Piece(id=self.id, label=self.label, fabric=self.fabric, points=single(self.x),
                        faces=self.faces.copy(), joints=order.astype("<u2"), weights=single(values),
                        r=single(self.r), d=single(self.d), R=single(self.R), b=single(self.b),
                        correctives={k: (single(dx), single(db)) for k, (dx, db) in self.correctives.items()})


def layer_rays(outer, inner, reach, sign=1.0):
    """Per outer vertex the first hit of its body direction (times sign) on the inner mesh within reach:
    (t, face, bary)."""
    t, f, b = np.full(len(outer.x), np.inf), np.full(len(outer.x), -1), np.zeros((len(outer.x), 3))
    for v in range(len(outer.x)):
        hit = sim.ray_hits(outer.x[v:v + 1], sign * outer.b[v:v + 1], inner.x, inner.faces, reach[v])
        t[v], f[v], b[v] = hit[0][0], hit[1][0], hit[2][0]
    return t, f, b


def overlap(outer_x, outer_faces, outer_b, inner_x, inner_faces, inner_b, outer_reach=np.inf, inner_reach=np.inf):
    """Where an outer layer lies over an inner one at rest: per outer vertex the gap g along its body direction to
    the inner mesh (inf where the ray misses it within outer_reach, the distance to the body), and per inner vertex
    the gap along -b to the outer mesh within inner_reach."""
    reach = np.broadcast_to(np.asarray(outer_reach, float), (len(outer_x),))
    g_out = np.array([sim.ray_hits(x[None], b[None], inner_x, inner_faces, t)[0][0]
                      for x, b, t in zip(outer_x, outer_b, reach)])
    reach = np.broadcast_to(np.asarray(inner_reach, float), (len(inner_x),))
    g_in = np.array([sim.ray_hits(x[None], -b[None], outer_x, outer_faces, t)[0][0]
                     for x, b, t in zip(inner_x, inner_b, reach)])
    return g_out, g_in


def stages(script=None):
    """The content stages a build includes: GARMENT_STAGES when set (a check or an experiment asks for another
    stage), else the stage the outfit ships, declared as "stage" in `<script>.json` beside the script, else all of
    them. A script reads it to choose an input that a later stage changes (the declared body)."""
    if "GARMENT_STAGES" in os.environ:
        return int(os.environ["GARMENT_STAGES"])
    if script is not None:
        declaration = Path(script).with_suffix(".json")
        if declaration.is_file():
            return int(json.loads(declaration.read_text(encoding="utf-8"))["stage"])
    return 1000000


class Outfit:
    """One outfit model and its pieces.

    script: the authoring script's path (its hash goes into the sidecar), or None; root: the game root; model: the
    render model (relative); body: the declared body file (B.3: complete, on the model's skeleton, its hash a build
    input), or the model's own primitives by material name as a fallback with holes (reach gates against it are
    reported, not enforced); fabrics: the fabrics file ({"fabrics": {name: ClothMaterial fields}}) or a dict;
    colliders, clips, looks: inputs of the checks only (the body shapes file, the clip manifest and the look models
    that share this sidecar); seats: {seated clip: the game's seat convex as (n, d) plane rows in actor space}, the
    input of the game's gate G10."""

    instances = []

    def __init__(self, script, root, model, body, fabrics, colliders=None, clips=None, looks=(), seats=None):
        self.root = Path(root).resolve()
        self.script = None if script is None else Path(script).resolve()
        self.inputs = {}
        self.model_path = self._input(model)
        self.model = RenderModel(self.model_path)
        if isinstance(fabrics, dict):
            self.fabrics = fabrics
        else:
            self.fabrics = json.loads(self._input(fabrics).read_text(encoding="utf-8"))["fabrics"]
        if isinstance(body, (str, Path)):
            # The declared body (B.3): complete, on the model's own skeleton.
            source = RenderModel(self._input(body))
            if source.joints != self.model.joints or not np.array_equal(
                    np.asarray(source.glb.inverse_binds(), np.float32), np.asarray(self.model.glb.inverse_binds(), np.float32)):
                raise ValueError(f"{body}: the body's joints or inverse binds differ from the model's")
            materials, complete = set(source.materials), True
        else:
            source, materials, complete = self.model, set(body), False
        surface = source.surface(materials)
        self.body = body_maps.Body(surface.points, surface.faces, source.normals(materials), surface.weights,
                                   surface.joints, complete=complete)
        self.colliders = None if colliders is None else self.root / colliders
        self.clips = None if clips is None else self.root / clips
        self.looks = [self.root / look for look in looks]
        self.seats = dict(seats or {})
        self.pieces = []
        self.layers = []
        # GARMENT_STAGES=n builds the script's content up to its stage n (`if o.stages >= 2:` blocks), to compare a
        # content fix with what came before it.
        self.stages = stages(script)
        Outfit.instances.append(self)

    def _input(self, relative):
        path = self.root / relative
        self.inputs[Path(relative).as_posix()] = sha256(path)
        return path

    def piece(self, piece_id, label, fabric):
        """A new piece moving the render vertices labelled `label`, made of the named fabric."""
        if any(p.id == piece_id or p.label == label for p in self.pieces):
            raise ValueError(f"piece {piece_id!r} or label {label} already exists")
        if fabric not in self.fabrics:
            raise ValueError(f"unknown fabric {fabric!r}; known: {sorted(self.fabrics)}")
        piece = Piece(self, piece_id, label, fabric, self.fabrics[fabric]["thickness"], self.model.joints)
        self.pieces.append(piece)
        return piece

    def joint_rest(self, name):
        """Rest position (model space) of a joint of the model's skin, by its full or short name."""
        glb = self.model.glb
        full = next(j for j in self.model.joints if name in (j, j.split(":")[-1]))
        return glb.world_rest()[glb.node_index(full)][:3, 3].copy()

    def get(self, piece_id):
        return next(p for p in self.pieces if p.id == piece_id)

    def over(self, outer, inner):
        """Declare that the outer piece lies over the inner one, so gate G8 checks their order in every clip frame.
        `layer` declares it too and also applies the rule that keeps the order."""
        self.get(outer), self.get(inner)
        if (outer, inner) not in self.layers:
            self.layers.append((outer, inner))

    def separate(self, outer, inner):
        """Where the outer piece's free vertices lie over the inner piece closer than the two fabrics are thick at
        rest, move them outward by the difference (spec B.6: a gap thinner than the fabrics is fixed by offsetting
        the outer sim mesh), then recompute the outer backstop. Returns the count moved. Run it before `layer`. A gap
        that closes only in motion is the skinning's, not the mesh's: `layer` reads it from the clips."""
        o, i = self.get(outer), self.get(inner)
        t = o.thickness + i.thickness
        moved = np.zeros(len(o.x), bool)
        # The gap is measured along the body direction and the offset goes along the normal, so a tilted vertex
        # needs a second look.
        for _ in range(8):
            g, _ = overlap(o.x, o.faces, o.b, i.x, i.faces, i.b, self.body.closest(o.x)[2], np.inf)
            thin = np.isfinite(g) & (g < t) & ~o.pins
            if not thin.any():
                break
            o.offset(thin, np.where(thin, t - np.where(np.isfinite(g), g, t), 0) * 1000)
            o.backstop_from_body()
            moved |= thin
        return int(moved.sum())

    def layer(self, outer, inner):
        """The layer rule of spec B.6 for an outer piece over an inner one, without cloth-to-cloth contact.

        Both references first ride on one skinning: under the outer the inner takes the outer's weights, and where
        the inner is held (a pin) the outer takes the inner's. Then, with g the narrowest gap the two references
        leave in any frame of the outfit's clips (the rest gap without clips), r_in = d_out = (g - t_in - t_out) / 2,
        so neither may reach the other. The inner r only shrinks (the gradient stays an upper bound); the outer
        backstop takes the body radius where it can now act. Run it after the weights. Returns counts: outer and
        inner vertices in the overlap, and vertices whose narrowest gap is thinner than the two fabrics (left at 0:
        `separate` the outer mesh there)."""
        o, i = self.get(outer), self.get(inner)
        t = o.thickness + i.thickness
        (gap_o, face_o, bary_o), (gap_i, face_i, bary_i) = self._share_weights(o, i)
        g_o = self._narrowest(o, i, gap_o, face_o, bary_o)
        g_i = self._narrowest(i, o, gap_i, face_i, bary_i, sign=-1.0)
        hit_o = np.isfinite(g_o) & ~o.pins
        o.d[hit_o] = np.maximum((g_o[hit_o] - t) / 2, 0)
        face, _, _, point, normal, rim = self.body.closest(o.x)
        for v in np.flatnonzero(hit_o & (o.R == 0) & (o.d < o.r) & ~rim):
            o.R[v] = self.body.radius(face[v], point[v], normal[v])
        o.R[o.d >= o.r] = 0
        o._mark("backstop", hit_o)
        hit_i = np.isfinite(g_i) & ~i.pins
        i.r[hit_i] = np.minimum(i.r[hit_i], np.maximum((g_i[hit_i] - t) / 2, 0))
        # Where any outer vertex's ray lands on an inner triangle (a pin's too), every free corner of it keeps within
        # what the outer leaves of the gap: g - t - its reach toward the inner, so G8 holds at the hit as well.
        reach = np.minimum(o.d, o.r)
        for v in np.flatnonzero(np.isfinite(g_o)):
            corners = i.faces[face_o[v]]
            free = corners[i.r[corners] > 0]
            i.r[free] = np.minimum(i.r[free], max(g_o[v] - t - reach[v], 0.0))
            hit_i[free] = True
        i.R[i.d >= i.r] = 0
        i._mark("r", hit_i)
        self.over(outer, inner)
        return dict(outer=int(hit_o.sum()), inner=int(hit_i.sum()),
                    too_thin=int(((g_o < t) & hit_o).sum() + ((g_i < t) & hit_i).sum()))

    def _share_weights(self, o, i):
        """Both references on one skinning where they overlap (see `layer`); returns the rest rays of both sides,
        ((gap, face, bary) of the outer's, (gap, face, bary) of the inner's outward)."""
        rays_o = layer_rays(o, i, self.body.closest(o.x)[2])
        rays_i = layer_rays(i, o, np.full(len(i.x), np.inf), sign=-1.0)
        _, face_o, bary_o = rays_o
        _, face_i, bary_i = rays_i
        hit = np.flatnonzero(face_o >= 0)
        r_at = np.einsum("nk,nk->n", bary_o[hit], i.r[i.faces[face_o[hit]]])
        follow_inner = hit[(r_at == 0) & ~o.pins[hit]]
        total, count = np.zeros_like(i.w), np.zeros(len(i.x))
        for v in hit[r_at > 0]:
            corners = i.faces[face_o[v]]
            free = corners[~i.pins[corners]]
            total[free] += o.w[v]
            count[free] += 1
        under = np.flatnonzero((face_i >= 0) & ~i.pins)
        total[under] += np.einsum("nk,nkj->nj", bary_i[under], o.w[o.faces[face_i[under]]])
        count[under] += 1
        ride = count > 0
        i.w[ride] = total[ride] / count[ride, None]
        i._mark("w", ride)
        o.w[follow_inner] = np.einsum("nk,nkj->nj", bary_o[follow_inner], i.w[i.faces[face_o[follow_inner]]])
        o._mark("w", np.isin(np.arange(len(o.x)), follow_inner))
        return rays_o, rays_i

    def _narrowest(self, a, b, gap, face, bary, sign=1.0):
        """Per vertex of `a` with a ray hit on `b`: the smallest gap along a's body direction (times sign) between
        the two references over the rest pose and every key frame of the outfit's clips, with the weights as the
        sidecar stores them (four influences); inf where there is no hit."""
        out = np.where(face >= 0, gap, np.inf)
        if self.clips is None or not (face >= 0).any():
            return out
        from . import model as Mo
        hit = np.flatnonzero(face >= 0)
        dense = []
        for piece in (a, b):
            order, values = top_influences(piece.w)
            w = np.zeros_like(piece.w)
            np.put_along_axis(w, order, values, axis=1)
            dense.append(w)
        glb, joints = self.model.glb, self.model.joints
        ibm = glb.inverse_binds()
        for _, path in Mo.clips(self.clips):
            clip = Mo.load(path)
            matrices = Mo.skin_matrices(glb, joints, ibm, Mo.clip_worlds(glb, clip, clip.key_times()))
            pa = Mo.skin_points(a.x[hit], dense[0][hit], matrices)
            da = Mo.skin_directions(a.b[hit], dense[0][hit], matrices)
            corners = b.faces[face[hit]]
            pb = Mo.skin_points(b.x, dense[1], matrices)
            point = np.einsum("nk,fnkj->fnj", bary[hit], pb[:, corners])
            g = np.einsum("fnj,fnj->fn", point - pa, sign * da).min(0)
            out[hit] = np.minimum(out[hit], g)
        return out

    def sidecar_path(self):
        name = self.model_path.name.removesuffix(".glb") + ".cloth.glb"
        folder = os.environ.get("GARMENT_SIDECAR_DIR")
        return Path(folder) / name if folder else self.model_path.with_name(name)

    def built(self):
        """The Sidecar of the current state."""
        nodes, joints, ibm = sc.skeleton(self.model.glb)
        build = {"script_sha256": sha256(self.script) if self.script and self.script.is_file() else None,
                 "inputs": dict(sorted(self.inputs.items())), "library": LIBRARY_VERSION}
        build["stages"] = self.stages
        return sc.Sidecar(nodes=nodes, joints=joints, inverse_binds=ibm, pieces=[p.built() for p in self.pieces],
                          build=build, layers=list(self.layers))

    def write(self):
        """Write the sidecar and return its path; a file with the same bytes is left untouched."""
        data = sc.write(self.built())
        path = self.sidecar_path()
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.is_file() or path.read_bytes() != data:
            path.write_bytes(data)
        return path
