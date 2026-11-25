"""Authoring of one outfit's cloth: an `Outfit` on a render model and its `Piece`s, each a sim mesh held as numpy
arrays with its maps and skin weights, edited by plain functions and written as the sidecar `<model>.cloth.glb`.

An outfit script is the source of truth; the sidecar is its built artifact. A piece holds per vertex:
- x: rest position (model space, metres, Y-up); faces: triangles whose front faces away from the body;
- r: max distance (m); 0 is kinematic, so the pins are exactly r == 0;
- d, R, b: backstop distance and radius (0 off, +inf the plane form) and the unit body direction;
- w: dense skin weights (vertices, model skin joints).
A value not yet authored is NaN (weights: an all-zero row); the writer names the map and the vertices left so.

The script authors the mesh, the pins, r and its own weights, which change at once, and declares the rules that
derive maps from other maps. The build resolves those in one fixed order, whatever order the script declared them in
(see the feature README, "Build order"): the body-following weights, the separation of layers, the backstop of every
piece from the body, the layer rule, the script's overrides of derived maps, the correctives. So no derived map can
be computed from a map that a later script line changes.

Masks are numpy boolean arrays over `piece.f` fields; every map and weight write records the script line that made
it, so `piece.why(i)` and the cards answer why a vertex is pinned, held or weighted as it is.

Paths are relative to the game root. GARMENT_SIDECAR_DIR, when set, is where `write` puts the sidecar instead of
beside the model (a rebuild check, a scratch build); a corrective whose recorded inputs all match is taken from the
sidecar beside the model instead of baked again (GARMENT_REBAKE=1 bakes every corrective)."""
import copy
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
from . import build as Bd
from . import sidecar as sc
from . import sim
from .sheet import simplify
from .sim import geodesic_depth
from . import model as Mo
from .model import RenderModel, Surface

PACKAGE = Path(__file__).resolve().parent
FEATURE = PACKAGE.parents[1]
LIBRARY_VERSION = Bd.LIBRARY_VERSION
INFLUENCES = 4
MAX_DISTANCE_MAX = 1.0  # m: the blob refuses more; a larger r is no longer motion around a reference
sha256 = Bd.sha256


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
    """Per-vertex fields an agent builds masks from (all arrays over the piece's current vertices). The derived maps
    (d, R, b) are not among them: the build computes them after the script."""

    def __init__(self, piece):
        self._p = piece

    x = property(lambda s: s._p.x[:, 0])
    y = property(lambda s: s._p.x[:, 1])
    z = property(lambda s: s._p.x[:, 2])
    r = property(lambda s: s._p.r)
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
        self.follows_body = False
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
        # Script writes of derived maps, applied by the build after it derives them: dicts of
        # name ("d", "R"), kind ("paint", "smooth"), mask, value, iterations, site.
        self.overrides = []

    def _mark(self, kind, mask, site=None):
        site = site or _site()
        if site not in self.sites:
            self.sites.append(site)
        self.origin[kind][np.asarray(mask, bool)] = self.sites.index(site)

    def _mask(self, mask):
        mask = self.f.all if mask is None else np.asarray(mask)
        if mask.dtype != bool or mask.shape != (len(self.x),):
            raise ValueError(f"{self.id}: a mask is a boolean array over the {len(self.x)} sim vertices")
        return mask

    def _weights_authored(self):
        if self.follows_body:
            raise ValueError(f"{self.id}: the piece follows the body, so the build derives its weights; author "
                             "weights before weights_follow_body() or not at all")

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
        """Adopt a new mesh: weights follow by interpolation, a new vertex is a pin when all its sources are, an
        override covers a new vertex when it covers all its sources, and the other maps are unset until a map rule
        writes them again."""
        P = P.tocsr()
        pin_source = (P @ (~self.pins).astype(float)) == 0
        w = P @ self.w
        origin = {k: v[np.asarray(P.argmax(axis=1)).ravel()] for k, v in self.origin.items()}
        overrides = []
        for o in self.overrides:
            moved = dict(o, mask=(P @ (~o["mask"]).astype(float)) == 0)
            if o["value"] is not None:
                moved["value"] = P @ np.where(o["mask"], o["value"], 0.0)
            overrides.append(moved)
        self.x, self.faces = np.asarray(points, float), np.asarray(faces, np.int64)
        self._fresh()
        self.w = w / np.maximum(w.sum(1, keepdims=True), 1e-30)
        self.origin.update(origin)
        self.overrides = overrides
        self.r[pin_source] = 0
        self._mark("mesh", self.f.all)
        return P

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
        """Bring the edges inside the mask toward edge_mm by splitting long and collapsing short ones. A collapse
        that would merge a pin with a free vertex is refused: keep the pins out of the mask, or unpin them first."""
        pins = self.pins.copy()
        points, faces, P = sim.density(self.x, self.faces, self._mask(mask), edge_mm / 1000)
        P = P.tocsr()
        # A pin survives when some new vertex comes from it alone.
        alone = np.asarray((P > 0).sum(1)).ravel() == 1
        kept = np.zeros(len(pins), bool)
        kept[np.unique(P[alone].indices)] = True
        lost = np.flatnonzero(pins & ~kept)
        if len(lost):
            raise ValueError(f"{self.id}: density would merge pins {lost[:12].tolist()} with free vertices")
        self._topology(points, faces, P)

    def relax(self, mask, iterations=4):
        """Even out the masked interior vertices on the surface; boundaries keep their line. Maps stay."""
        self.x = sim.relax(self.x, self.faces, self._mask(mask), iterations)
        self._mark("mesh", self._mask(mask))

    def offset(self, mask, mm):
        """Move the masked vertices along their normals, outward for positive mm (a number or one per vertex). Maps
        stay; the build measures the backstop on the mesh as it ends."""
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
        so the seam stays exact. Repeats until the grown binding, which starts from the pins, adds no pin; every
        pass that goes on adds one, so it ends. Returns the count of pins it added."""
        from . import bind
        model = self.outfit.model
        before = self.pins.copy()
        added = True
        while added:
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
        (depth_of_hip: p.f.geo at the hip line, e.g. its median over the vertices there). It reads the pins as they
        are now: pin first, then grade."""
        mask = self._mask(mask) & ~self.pins
        if not self.pins.any():
            raise ValueError(f"{self.id}: pin the piece before its max-distance gradient")
        geo, _ = geodesic_depth(self.x, self.faces, np.flatnonzero(self.pins))
        mask &= geo >= start
        self.r[mask] = np.minimum(base + rate * (geo - start), cap)[mask]
        self._mark("r", mask)

    def paint(self, name, mask, value):
        """Set map `name` to value (a number or a per-vertex array) on the mask. "r" is authored and changes at once;
        "d" and "R" are derived by the build, so painting them is an override the build applies after deriving them,
        in the order the script painted."""
        mask = self._mask(mask)
        if name == "r":
            self.r[mask] = np.broadcast_to(np.asarray(value, float), self.r.shape)[mask]
            self._mark("r", mask)
            return
        if name not in ("d", "R"):
            raise ValueError(f"{self.id}: paint takes r, d or R, not {name!r}")
        value = np.broadcast_to(np.asarray(value, float), (len(self.x),)).copy()
        self.overrides.append(dict(name=name, kind="paint", mask=mask.copy(), value=value, iterations=0, site=_site()))
        self._mark("backstop", mask)

    def smooth(self, name, mask, iterations=1):
        """Average map `name` with its sim-mesh neighbours on the mask; pins stay pins when smoothing r. Smoothing
        d or R is an override the build applies after deriving them (see `paint`)."""
        mask = self._mask(mask)
        if name in ("d", "R"):
            self.overrides.append(dict(name=name, kind="smooth", mask=mask.copy(), value=None, iterations=int(iterations),
                                       site=_site()))
            self._mark("backstop", mask)
            return
        if name != "r":
            raise ValueError(f"{self.id}: smooth takes r, d or R, not {name!r}")
        _smooth(self, self.r, mask & ~self.pins, iterations)
        self._mark("r", mask & ~self.pins)

    # Weights -------------------------------------------------------------------------------------------------
    def weights_from(self, surface, nearest=None):
        """Weights a transfer from `surface` (a model.Surface on the model's skin) would give, without assigning.

        nearest=None: the skin weights at the closest surface point, sampled from each boundary vertex nudged into
        its own triangles, so a vertex on one lip of a cut reads its own side. nearest=k: the inverse-square blend of
        the k nearest surface vertices."""
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
        self._weights_authored()
        mask = self._mask(mask)
        self.w[mask] = self.weights_from(surface, nearest)[mask]
        self._mark("w", mask)

    def weights_set(self, mask, joint, weight):
        """Give `joint` the weight on the mask and scale the vertex's other joints to the rest."""
        self._weights_authored()
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
        self._weights_authored()
        mask = self._mask(mask)
        by = np.broadcast_to(np.asarray(by, float), (len(self.x),))[:, None]
        blended = (1 - by) * np.asarray(a, float) + by * np.asarray(b, float)
        self.w[mask] = (blended / np.maximum(blended.sum(1, keepdims=True), 1e-30))[mask]
        self._mark("w", mask)

    def weights_smooth(self, mask, iterations=1):
        """Average the weights with their sim-mesh neighbours on the mask, `iterations` times."""
        self._weights_authored()
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
        self._weights_authored()
        mask = self._mask(mask)
        _average(self, mask, radius)
        self._mark("w", mask)

    def weights_follow_body(self):
        """Declare the default weights of a piece with a body behind it; the build applies them once the pins and r
        are final: every vertex takes the skin weights of the closest point of the outfit's body surface, each free
        vertex then averages them within its own max distance r (the reference then changes across the cloth no
        faster than the cloth may move, so two limbs cannot pull an edge apart beyond what its two spheres allow), and
        the pins keep the weights the script gave them (the seam with the skinned garment). Later weight writes on the
        piece are refused. A piece with no body behind it (a ribbon) keeps its own weights: do not call this for it."""
        self.follows_body = True
        self._mark("w", self.f.all)

    # Built form ----------------------------------------------------------------------------------------------
    def built(self):
        """The sidecar record, after the checks every value must pass."""
        problems = []
        for name in ("r", "d", "R"):
            bad = np.flatnonzero(np.isnan(getattr(self, name)))
            if len(bad):
                problems.append(f"{name} unset on {len(bad)} vertices (first {bad[:5].tolist()})")
        if np.isnan(self.b).any():
            problems.append("b unset")
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
                        correctives={k: tuple(single(a) for a in shape) for k, shape in self.correctives.items()})


def _overrides(piece, d, R):
    """The script's paint and smooth of d and R, in the order it wrote them (README, "Build order": the script's
    overrides of derived maps)."""
    for o in piece.overrides:
        target = {"d": d, "R": R}[o["name"]]
        if o["kind"] == "paint":
            target[o["mask"]] = o["value"][o["mask"]]
        else:
            _smooth(piece, target, o["mask"], o["iterations"])


def _smooth(piece, target, mask, iterations):
    adjacency = sim.neighbours(piece.faces, len(piece.x)).astype(float)
    count = np.maximum(np.asarray(adjacency.sum(1)).ravel(), 1)
    for _ in range(iterations):
        target[mask] = 0.5 * target[mask] + 0.5 * (adjacency @ target / count)[mask]


def _average(piece, mask, radius):
    radius = np.broadcast_to(np.asarray(radius, float), (len(piece.x),))
    reach = sim.dijkstra_all(piece.x, piece.faces, limit=float(radius[mask].max(initial=0)))
    source = piece.w.copy()
    for i in np.flatnonzero(mask):
        piece.w[i] = source[reach[i] <= radius[i]].mean(0)


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


def load_body(model, body, read=lambda path: path):
    """The Body of an outfit: a declared body file (B.3: on the model's own skeleton and proven complete, see
    body.prove), or the model's own primitives by material name (the fallback, with holes). `read` maps a path to the
    file actually opened (a build records its hash)."""
    if isinstance(body, (str, Path)):
        source = RenderModel(read(body))
        if source.joints != model.joints or not np.array_equal(
                np.asarray(source.glb.inverse_binds(), np.float32), np.asarray(model.glb.inverse_binds(), np.float32)):
            raise ValueError(f"{body}: the body's joints or inverse binds differ from the model's")
        materials, complete = set(source.materials), True
    else:
        source, materials, complete = model, set(body), False
    surface = source.surface(materials)
    if complete:
        failures = body_maps.prove(surface.points, surface.faces)
        if failures:
            raise ValueError(f"{body}: not a proven complete body (" + "; ".join(failures) + "); the reach gates' "
                             "winding number would be wrong on it")
    return body_maps.Body(surface.points, surface.faces, source.normals(materials), surface.weights, surface.joints,
                          complete=complete)


SEPARATE_RESOLUTION = body_maps.RESOLUTION


class Outfit:
    """One outfit model and its pieces.

    script: the authoring script's path (its hash goes into the sidecar), or None; root: the game root; model: the
    render model (relative); body: the declared body file (B.3: complete, on the model's skeleton, its hash a build
    input), or the model's own primitives by material name as a fallback with holes (reach gates against it are
    reported, not enforced); fabrics: the fabrics file ({"fabrics": {name: ClothMaterial fields}}) or a dict;
    colliders, clips, looks: the body shapes file, the clip manifest and the look models that share this sidecar
    (the checks read them; a build reads the clips for the layer rule and a bake reads all but the looks); seats:
    {seated clip: the game's seat convex as (n, d) plane rows in actor space, or {"planes": rows, "source": the model
    the seat is measured on}}, the input of the game's gate G10 and of a seated bake; or a function that returns them
    from a reader of game files (`read(relative path) -> path`), so every file the seats come from is a build input;
    places: the clips the game's own places (a directed shot) show this outfit in besides the manifest's gameplay
    clips (`game.gameplay`). The build, its bakes, the collider fit and the gates read exactly the clips the game can
    show the outfit in (`shown`, recorded in `extras.check`): the gameplay clips, the places' clips and the seated clips
    the declaration clears. The game's cook compares every place with that record.

    The outfit's declaration (`<script>.json` beside the script, the stage the game ships) is the one source of its
    colliders: a build input, so a changed declaration makes the sidecar stale. Its `colliders` are the body shapes
    the build, the bakes and the gates read (a `colliders` argument must name the same file), and its `script` and
    `sidecar`, where it names them, must be this script and the sidecar this build writes beside the model."""

    instances = []

    def __init__(self, script, root, model, body, fabrics, colliders=None, clips=None, looks=(), seats=None,
                 seated=None, places=()):
        self.root = Path(root).resolve()
        self.script = None if script is None else Path(script).resolve()
        self.inputs = {}
        declared = self._declaration()
        if declared is not None:
            if colliders is not None and declared.get("colliders") != Path(colliders).as_posix():
                raise ValueError(f"{self.declaration.name} declares colliders {declared.get('colliders')}, the script passes "
                                 f"{colliders}: the declaration is the one source of the outfit's body shapes")
            colliders = declared.get("colliders")
            if seated is not None and sorted(seated) != sorted(declared.get("seated", [])):
                raise ValueError(f"{self.declaration.name} clears the seated clips {declared.get('seated', [])}, the "
                                 f"script passes {list(seated)}: the declaration is the one source of them")
            seated = declared.get("seated", [])
        self.model_path = self._input(model)
        if declared is not None and declared.get("sidecar") not in (None, self._committed_path().relative_to(
                self.root).as_posix()):
            raise ValueError(f"{self.declaration.name} ships the sidecar {declared['sidecar']}; this script builds "
                             f"{self._committed_path().relative_to(self.root).as_posix()}")
        self.model = RenderModel(self.model_path)
        if isinstance(fabrics, dict):
            self.fabrics = fabrics
        else:
            self.fabrics = json.loads(self._input(fabrics).read_text(encoding="utf-8"))["fabrics"]
        self.body_spec = Path(body).as_posix() if isinstance(body, (str, Path)) else sorted(body)
        self.body = load_body(self.model, body, self._input)
        self.colliders = None if colliders is None else self.root / colliders
        self.clips = None if clips is None else self.root / clips
        self.seated = sorted(seated or [])
        if self.seated:
            known = set() if self.clips is None else Mo.seated(self.clips)
            if set(self.seated) - known:
                raise ValueError(f"the outfit is cleared for {sorted(set(self.seated) - known)}, which the clip manifest "
                                 f"does not mark seated (its seated clips: {sorted(known)})")
        if places and self.clips is None:
            raise ValueError("places name clips of a clip manifest; declare the outfit's clips")
        # The clips the game can show the outfit in, what its build, bakes, fits and gates read.
        self.shown = [] if self.clips is None else Mo.gated(self.clips, self.seated, places)
        self.looks = [self.root / look for look in looks]
        given = dict((seats(self._input) if callable(seats) else seats) or {})
        if self.clips is not None:
            # A seat of a seated clip the outfit is not cleared for is no input of it: nothing it shows sits there.
            hidden = Mo.seated(self.clips) - set(self.seated)
            given = {clip: s for clip, s in given.items() if clip not in hidden}
        self.seats = {clip: (s["planes"] if isinstance(s, dict) else s) for clip, s in given.items()}
        self.seat_sources = {clip: s.get("source") for clip, s in given.items() if isinstance(s, dict)}
        self.pieces = []
        self.layers = []
        self.separations = []
        self.layer_rules = []
        self.corrective_requests = []
        self.corrective_records = {}
        self._resolved = None
        # GARMENT_STAGES=n builds the script's content up to its stage n (`if o.stages >= 2:` blocks), to compare a
        # content fix with what came before it.
        self.stages = stages(script)
        Outfit.instances.append(self)

    def _declaration(self):
        """The outfit's declaration, read as a build input, or None when the script has none."""
        self.declaration = None if self.script is None else self.script.with_suffix(".json")
        if self.declaration is None or not self.declaration.is_file():
            return None
        declared = json.loads(self._input(self.declaration).read_text(encoding="utf-8"))
        named = declared.get("script")
        if named is not None and self.script.is_relative_to(self.root) and \
                named != self.script.relative_to(self.root).as_posix():
            raise ValueError(f"{self.declaration.name} declares the script {named}, not "
                             f"{self.script.relative_to(self.root).as_posix()}")
        return declared

    def _committed_path(self):
        return self.model_path.with_name(self.model_path.name.removesuffix(".glb") + ".cloth.glb")

    def _input(self, relative):
        path = Path(relative)
        path = self.root / (path.resolve().relative_to(self.root) if path.is_absolute() else path)
        self.inputs[path.relative_to(self.root).as_posix()] = sha256(path)
        return path

    def input(self, path):
        """Declare a file (relative to the game root, or under it) a build input: its hash goes into the sidecar, so
        the cook refuses the sidecar once the file changes. A script calls it for a file whose content it reads into
        the cloth by its own code."""
        return self._input(path)

    def clip_paths(self):
        """{name: path} of the clips the outfit is shown in (`shown`: its manifest's gameplay clips, the clips of its
        places and the seated clips it is cleared for; the manifest is read as a build input)."""
        if self.clips is None:
            return {}
        self._input(self.clips)
        return dict(Mo.shown(self.clips, self.shown))

    def clip_entries(self):
        if self.clips is None:
            return {}
        return {c["name"]: c for c in json.loads(self.clips.read_text(encoding="utf-8"))["animations"]}

    def corrective(self, name, clip=None, t=None, pose=None, convexes=None, max_passes=None, convex_source=None):
        """Declare the named corrective of every piece (spec B.5, E.1); the build bakes it through the feature's own
        runtime once every map is final, and stores it: the drape the runtime settles to in the key pose, as rest-space
        deltas of the reference and the body direction. The game weights it by name while a clip holds that pose (the
        clip manifest's `game.corrective`).

        The key is a clip of the outfit's manifest and a time t (s), or a pose ((joints, 4, 4) model space, the skin's
        joints in skeleton order). convexes are what the cloth rests on besides the floor, as (n, d) plane rows in
        actor space; by default the outfit's declared seat for that clip. Convexes feed the drape, so the model they are
        measured on (the seat's `source`, or convex_source) is a source the sidecar's record lists; convexes without it
        are refused. A clip the manifest marks as seated with no
        declared seat is refused: a bake without a chair says so with `convexes=[]`. A bake that does not converge
        stops the build. The bake needs the garment_host library built from the current runtime
        (`node ai_studio/feature_testing/run.mjs`) and the outfit's colliders; a corrective whose recorded inputs
        all match the committed sidecar is taken from it without a bake."""
        from . import corrective as C
        if any(r["name"] == name for r in self.corrective_requests):
            raise ValueError(f"corrective {name!r} is already declared")
        if (clip is None) == (pose is None) or (clip is not None and t is None):
            raise ValueError(f"corrective {name!r}: give the key as clip and t, or as a pose")
        if self.colliders is None:
            raise ValueError(f"corrective {name!r}: the bake collides with the outfit's body shapes; declare its colliders")
        if clip is not None:
            entries = self.clip_entries()
            if clip not in entries:
                raise ValueError(f"corrective {name!r}: no clip {clip!r} in {self.clips}")
            seated = entries[clip].get("game", {}).get("state") == "seated"
            if seated and clip not in self.seated:
                raise ValueError(f"corrective {name!r}: the outfit is not cleared for the seated clip {clip!r}; clear "
                                 "it in the declaration's seated")
            if clip not in self.shown:
                raise ValueError(f"corrective {name!r}: the game does not show the outfit in {clip!r} (no gameplay clip, "
                                 "no place of the outfit plays it), so no gate would read it")
            if convexes is None:
                if seated and clip not in self.seats:
                    raise ValueError(f"corrective {name!r}: {clip!r} is a seated clip and the outfit declares no seat "
                                     "for it; declare the seat, or bake without one by convexes=[]")
                convexes = [self.seats[clip]] if clip in self.seats else []
                convex_source = self.seat_sources.get(clip)
        elif convexes is None:
            convexes = []
        request = dict(name=name, convexes=[np.asarray(c, float).reshape(-1, 4).tolist() for c in convexes],
                       max_passes=int(max_passes or C.MAX_PASSES))
        if convexes:
            if convex_source is None:
                raise ValueError(f"corrective {name!r} rests on convexes whose model the outfit does not name: give the "
                                 "seat as {'planes': rows, 'source': model}, or convex_source=, so the sidecar's record "
                                 "lists it")
            request["convex_source"] = Path(convex_source).as_posix()
        if clip is not None:
            request.update(clip=clip, t=float(t))
        else:
            pose = np.asarray(pose, float)
            request["pose"] = dict(matrices=pose.tolist())
        self.corrective_requests.append(request)

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
        """Declare that where the outer piece's free vertices lie over the inner piece closer than the two fabrics are
        thick at rest, the build moves them outward by the difference (spec B.6: a gap thinner than the fabrics is
        fixed by offsetting the outer sim mesh), before it measures the backstop and applies the layer rule. A gap that
        closes only in motion is the skinning's, not the mesh's: `layer` reads it from the clips."""
        self.get(outer), self.get(inner)
        if (outer, inner) not in self.separations:
            self.separations.append((outer, inner))

    def layer(self, outer, inner):
        """Declare the layer rule of spec B.6 for an outer piece over an inner one, without cloth-to-cloth contact;
        the build applies it after the backstop.

        Both references first ride on one skinning: under the outer the inner takes the outer's weights, and where
        the inner is held (a pin) the outer takes the inner's. Then, with g the narrowest gap the two references
        leave in any frame of the outfit's clips (the rest gap without clips), r_in = d_out = (g - t_in - t_out) / 2,
        so neither may reach the other. The inner r only shrinks (the gradient stays an upper bound). Where the gap is
        thinner than the two fabrics the rule leaves no room (an inner vertex becomes kinematic): the build reports it
        (`separate` the outer mesh there)."""
        self.over(outer, inner)
        if (outer, inner) not in self.layer_rules:
            self.layer_rules.append((outer, inner))

    # Build ---------------------------------------------------------------------------------------------------
    def _digest(self):
        """Hash of everything the script authored and declared: the resolution is a function of it."""
        h = hashlib.sha256()
        for p in self.pieces:
            h.update(p.id.encode())
            h.update(p.fabric.encode())
            h.update(bytes([p.follows_body]))
            for array in (p.x, p.faces, p.r, p.w):
                h.update(np.ascontiguousarray(array).tobytes())
            for o in p.overrides:
                h.update(repr((o["name"], o["kind"], o["iterations"])).encode())
                h.update(o["mask"].tobytes())
                if o["value"] is not None:
                    h.update(o["value"].tobytes())
        h.update(json.dumps([self.layers, self.separations, self.layer_rules, self.corrective_requests]).encode())
        return h.hexdigest()

    def resolve(self):
        """The pieces as the build makes them (copies; the authored pieces stay as the script left them), the build
        findings and the corrective records. The order, by dependency:
        1. body-following weights (need the final pins and r);
        2. separation of declared layers (moves the outer mesh, so it comes before anything measured on the mesh);
        3. the backstop b, d, R of every piece from the body (needs the final mesh and pins);
        4. the layer rule (needs the weights, b, d and r; it shares weights, sets the outer d and limits the inner r);
        5. the script's overrides of d and R, in the order it wrote them;
        6. the correctives, in the order declared (each needs every map final; GARMENT_CORRECTIVES=0 leaves them out).
        """
        digest = self._digest() + os.environ.get("GARMENT_CORRECTIVES", "")
        if self._resolved is not None and self._resolved[0] == digest:
            return self._resolved[1]
        pieces = {p.id: copy.copy(p) for p in self.pieces}
        for p in pieces.values():
            for name in ("x", "r", "d", "R", "b", "w"):
                setattr(p, name, getattr(p, name).copy())
            p.origin = {k: v.copy() for k, v in p.origin.items()}
            p.sites = list(p.sites)
            p.correctives = {}
            p.layer_d = None
            p.f = Fields(p)
        findings = []
        for p in pieces.values():
            if np.isnan(p.r).any():
                raise ValueError(f"{p.id}: r unset on {int(np.isnan(p.r).sum())} vertices: author r on every vertex")
            if p.render is not None and p.render.sheet_dropped:
                findings.append(dict(kind="sheet_dropped", piece=p.id, count=p.render.sheet_dropped, vertices=[]))
        for p in pieces.values():
            if p.follows_body:
                self._follow_body(p)
        for outer, inner in self.separations:
            self._separate(pieces[outer], pieces[inner])
        for p in pieces.values():
            p.b, p.d, p.R, hole, unfit = body_maps.backstop(p.x, p.faces, p.pins, self.body, p.thickness)
            p._mark("backstop", p.f.all, "build: backstop from the body")
            if unfit.any():
                findings.append(dict(kind="backstop_unfit", piece=p.id, count=int(unfit.sum()),
                                     vertices=np.flatnonzero(unfit)[:12].tolist()))
        for outer, inner in self.layer_rules:
            findings += self._layer(pieces[outer], pieces[inner])
        for p in pieces.values():
            _overrides(p, p.d, p.R)
        ordered = [pieces[p.id] for p in self.pieces]
        records = []
        # GARMENT_CORRECTIVES=0: the maps alone, for the collider fit; a bake collides with the colliders it makes.
        if self.corrective_requests and os.environ.get("GARMENT_CORRECTIVES") != "0":
            from . import corrective as C
            committed = self._committed()
            for request in self.corrective_requests:
                records.append(C.resolve(self, ordered, records, request, committed))
        self.corrective_records = {r["name"]: r for r in records}
        result = (ordered, findings, records)
        self._resolved = (digest, result)
        return result

    def backstop_at(self, p, points, body):
        """(b, d, R) of a resolved piece in another configuration of its reference (a corrective's settled drape)
        against the body posed there, by the rule the build applies at rest (README, "Build order"): the backstop from
        the body, the outer d the layer rule set (its gap is already the narrowest over every clip frame), then the
        script's overrides."""
        b, d, R, _, _ = body_maps.backstop(points, p.faces, p.pins, body, p.thickness)
        if p.layer_d is not None:
            hit, values = p.layer_d
            d[hit] = values
        _overrides(p, d, R)
        return b, d, R

    def resolved_piece(self, piece_id):
        return next(p for p in self.resolve()[0] if p.id == piece_id)

    def _committed(self):
        """The sidecar beside the model (the committed one), whose correctives a build may keep, or None."""
        path = self._committed_path()
        if os.environ.get("GARMENT_REBAKE") == "1" or not path.is_file():
            return None
        try:
            return sc.read(path.read_bytes())
        except ValueError:
            return None

    def _follow_body(self, p):
        own, pins = p.w.copy(), p.pins
        body = self.body
        p.w = p.weights_from(Surface(body.points, body.faces, body.weights, body.joints))
        _average(p, ~pins, p.r)
        p.w[pins] = own[pins]
        p._mark("w", p.f.all, "build: weights follow the body")

    def _separate(self, o, i):
        t = o.thickness + i.thickness
        deficit = np.inf
        while True:
            o_b, g, (g_in, face_in) = self._layer_gaps(o, i)
            # The rule `layer` applies, from both sides: thinner than the two fabrics in any frame leaves no room. An
            # inner vertex's gap is opened by moving the free corners of the outer triangle its ray meets.
            want = np.where(np.isfinite(g) & ~o.pins, t - g, 0.0)
            short = np.flatnonzero(np.isfinite(g_in) & ~i.pins & (g_in < t))
            for v in short:
                corners = o.faces[face_in[v]]
                want[corners] = np.maximum(want[corners], np.where(o.pins[corners], 0.0, t - g_in[v]))
            thin = want > 0
            if not thin.any():
                break
            # The move goes outward along the body direction, the line the gap is measured on, so neither the mesh's
            # winding nor a fold of a coarse sheet decides it; the direction is measured again from the moved mesh, so
            # a pass may leave a remainder, and each pass must shrink what is missing.
            missing = float(np.sum(want[thin]))
            if missing >= deficit or (short.size and not thin[o.faces[face_in[short]]].any()):
                raise ValueError(f"separate {o.id} over {i.id}: moving out along the body direction does not open the gap "
                                 f"at vertices {np.flatnonzero(thin)[:12].tolist()} ({missing * 1000:.1f} mm missing in all)")
            deficit = missing
            o.x = o.x - np.where(thin, want + SEPARATE_RESOLUTION, 0)[:, None] * o_b
            o._mark("mesh", thin, "build: separate the layers")

    def _layer_gaps(self, o, i):
        """(outer body direction, per outer vertex the gap `layer` will read, (per inner vertex the gap it reads
        outward, the outer triangle that ray meets)): along each piece's body direction to the other mesh, the
        narrowest over the rest pose and the clips with both pieces on the skinning the rule gives them (inf where the
        ray misses). Measured on copies, so nothing of the pieces changes."""
        a, b = copy.copy(o), copy.copy(i)
        for p in (a, b):
            p.w = p.w.copy()
            p.b = body_maps.directions(p.x, p.faces, self.body)
            p.origin = {k: v.copy() for k, v in p.origin.items()}
            p.sites = list(p.sites)
        (gap_o, face_o, bary_o), (gap_i, face_i, bary_i) = self._share_weights(a, b)
        return (a.b, self._narrowest(a, b, gap_o, face_o, bary_o),
                (self._narrowest(b, a, gap_i, face_i, bary_i, sign=-1.0), face_i))

    def _layer(self, o, i):
        t = o.thickness + i.thickness
        (gap_o, face_o, bary_o), (gap_i, face_i, bary_i) = self._share_weights(o, i)
        g_o = self._narrowest(o, i, gap_o, face_o, bary_o)
        g_i = self._narrowest(i, o, gap_i, face_i, bary_i, sign=-1.0)
        hit_o = np.isfinite(g_o) & ~o.pins
        o.d[hit_o] = np.maximum((g_o[hit_o] - t) / 2, 0)
        o.layer_d = (hit_o, o.d[hit_o].copy())
        o._mark("backstop", hit_o, "build: layer rule")
        was_free = ~i.pins
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
        i._mark("r", hit_i, "build: layer rule")
        out = []
        held = np.flatnonzero(was_free & i.pins)
        if len(held):
            out.append(dict(kind="layer_kinematic", piece=i.id, pair=f"{o.id}>{i.id}", count=int(len(held)),
                            vertices=held[:12].tolist()))
        thin = np.flatnonzero(hit_o & (g_o < t))
        if len(thin):
            out.append(dict(kind="layer_too_thin", piece=o.id, pair=f"{o.id}>{i.id}", count=int(len(thin)),
                            vertices=thin[:12].tolist()))
        return out

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
        i._mark("w", ride, "build: layer rule")
        o.w[follow_inner] = np.einsum("nk,nkj->nj", bary_o[follow_inner], i.w[i.faces[face_o[follow_inner]]])
        o._mark("w", np.isin(np.arange(len(o.x)), follow_inner), "build: layer rule")
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
            # Summed, as the runtime skins: an empty lane names the strongest joint with weight 0 and adds nothing.
            np.add.at(w, (np.arange(len(w))[:, None], order), values)
            dense.append(w)
        glb, joints = self.model.glb, self.model.joints
        ibm = glb.inverse_binds()
        for _, path in self.clip_paths().items():
            clip = Mo.load(self._input(path))
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

    def build_record(self):
        """The build record (`extras.build`): every input of the sidecar by content (see garment.build)."""
        return {"script_sha256": sha256(self.script) if self.script and self.script.is_file() else None,
                "inputs": dict(sorted(self.inputs.items())),
                "modules": Bd.module_hashes(self.script, self.root) if self.script and self.script.is_file() else {},
                "versions": Bd.versions(), "library": LIBRARY_VERSION, "stages": self.stages,
                "model": self.model_path.relative_to(self.root).as_posix(), "body": self.body_spec}

    def check_inputs(self):
        """What the gates read besides the sidecar (`extras.check`), so a cook can run them without the script."""
        relative = lambda p: None if p is None else Path(p).resolve().relative_to(self.root).as_posix()  # noqa: E731
        return {"colliders": relative(self.colliders), "clips": relative(self.clips),
                "fabrics": {p.id: self.fabrics[p.fabric] for p in self.pieces},
                "looks": [relative(p) for p in self.looks], "seated": list(self.seated), "shown": list(self.shown),
                "seats": {clip: np.asarray(planes, float).reshape(-1, 4).tolist() for clip, planes in sorted(self.seats.items())}}

    def sidecar(self, pieces, records, findings=()):
        nodes, joints, ibm = sc.skeleton(self.model.glb)
        return sc.Sidecar(nodes=nodes, joints=joints, inverse_binds=ibm, pieces=[p.built() for p in pieces],
                          build=self.build_record(), layers=list(self.layers), correctives=list(records),
                          check=self.check_inputs(), findings=list(findings))

    def built(self):
        """The Sidecar of the current state, with every derived map resolved."""
        pieces, findings, records = self.resolve()
        return self.sidecar(pieces, records, findings)

    def sources(self):
        """Every file whose content feeds the sidecar's cloth and so its record lists: the model, the declared body
        file, each clip a corrective was baked on and the model of each convex a bake rested on."""
        from . import model as Mo
        out = [self.model_path]
        if isinstance(self.body_spec, str):
            out.append(self.root / self.body_spec)
        baked_on_clips = any(r.get("clip") is not None for r in self.corrective_requests)
        clips = dict(Mo.clips(self.clips)) if baked_on_clips else {}
        for request in self.corrective_requests:
            if request.get("clip") is not None:
                out.append(Path(clips[request["clip"]]))
            if request.get("convex_source"):
                out.append(self.root / request["convex_source"])
        return out

    def write(self):
        """Write the sidecar and its asset record and return the sidecar's path; a file with the same bytes is left
        untouched. A sidecar derived from an account-gated or purchased export is refused, and nothing is written,
        unless the project owner's grant lists it (garment.build.derived_sources)."""
        data = sc.write(self.built())
        sources = Bd.derived_sources(self.root, self.sources(), self._committed_path())
        path = self.sidecar_path()
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.is_file() or path.read_bytes() != data:
            path.write_bytes(data)
        script = self.script.relative_to(self.root).as_posix() if self.script and self.script.is_relative_to(self.root) \
            else (str(self.script) if self.script else None)
        Bd.write_record(path, self.root if path.resolve().is_relative_to(self.root) else path.parent,
                        what="The outfit's sim meshes, pins, skin weights and maps (garment-cloth sidecar, spec B.1): a "
                             "built artifact of its script; the cook refuses it once an input in extras.build changes.",
                        producer=script, rebuild=f"python {script}" if script else None, source=self.model_path,
                        sources=sources)
        return path
