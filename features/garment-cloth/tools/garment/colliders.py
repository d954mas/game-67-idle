"""The body shapes of one body model, fitted by one rule (spec C.3), and their file `<body>.cloth-colliders.json`.

The shapes are the second line behind the cloth's own reference (max distance, backstop): they keep loose cloth out of
the body. The rule, per segment of the body (a joint pair: rig data, never outfit data):

1. Belongs. A body vertex belongs to a segment when its dominant skin joint is the segment's first joint and, of that
   joint's segments, this one's rest joint pair is the nearest. The shape rides that joint, the one that moves the
   vertex most, so it follows the vertex through the poses.
2. Covers. The round cone on an axis parallel to the joint pair, its spheres at the first and last belonging vertex
   along it, that holds every belonging vertex with no margin and has the least volume between its spheres: each
   vertex lies in the ball centred at its projection on the axis with the cone's radius there. That test is
   sufficient for the exact round cone; a round cone is convex, so a triangle whose corners are held is held whole.
   The skin inside is fixed, so least volume is least volume outside the skin; beyond the spheres lies the next
   segment or the end of the limb, which the volume does not count.
3. Reach (gate G7). No shape may hold a reference vertex of any garment piece deeper than that vertex's max distance
   r, in any frame of any clip: sd(ref, shape) >= thickness - r, so a pin (r = 0) stays outside the shape plus the
   thickness, as the runtime measures it. G7 is evaluated on the reference samples that pass G4: a sample that is
   itself inside the skin deeper than r (sd(ref, skin + thickness) < -r; a pin inside the skin at all) is content to
   fix, listed by `g4_failing`, and no shape is shrunk around it.
4. Give way. A cone that fails G7 is replaced by the cone that leaves the least skin outside (the skin area times its
   depth outside, so a millimetre of thinning costs a millimetre, not the whole ring) among those that pass: on the
   cover's axis, each end cut back by 0 to half the length in tenths, each end radius no larger than the cover's
   there. Thinning costs least, so a cone thins before it shortens; the full length wins a tie. The section split
   along its long direction gives two halves, each covered and given way the same (spec X3); the pair replaces the
   single cone when it brings the skin closer on average by more than the thinnest fabric is thick, a gain the cloth
   could show. A cone nothing passes for is left out. Shapes do not interact in G7 (the union holds a vertex iff
   one shape does), so each segment is decided alone.

All positions are rest model space, metres, Y up. A shape's spheres ride one joint: a rest centre c is posed by that
joint's skin matrix (joint world x inverse bind)."""
import hashlib
import json
import math
import time
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
from scipy.optimize import minimize

from . import model as M

SCHEMA = "garment.colliders.v1"
REST = "rest"
# The runtime's limit on spheres and on cones per outfit (garment_blob.SHAPES_MAX).
SHAPES_MAX = 64
# Radii are searched on this many steps; a cut in tenths of the length, up to half from each end.
STEPS = 40
CUTS = np.linspace(0.0, 0.5, 6)
# The fit clears every reference vertex by the runtime's float resolution at body scale, so the runtime's float32
# measure and joint matrices a hair off rigid agree with the fit.
RESOLUTION_M = 1e-6


def round_cone_distance(points, a, b, ra, rb):
    """Signed distance, negative inside, of points (..., N, 3) to the round cone that is the convex hull of the balls
    (a, ra) and (b, rb); a and b are (3,) or (..., 1, 3) and broadcast. The closed form of contact.c."""
    p = np.asarray(points, float)
    a, b = np.asarray(a, float), np.asarray(b, float)
    pa = p - a
    ba = b - a
    length = np.sqrt(np.sum(ba * ba, axis=-1))
    u = ba / np.maximum(length, 1e-12)[..., None]
    y = np.sum(pa * u, axis=-1)
    x = np.linalg.norm(pa - y[..., None] * u, axis=-1)
    return profile_distance(y, x, length, ra, rb)


def profile_distance(y, x, length, ra, rb):
    """The same distance from a point's axial coordinate y (from sphere a toward b) and its distance x from the axis:
    a round cone is axially symmetric."""
    da = np.hypot(x, y) - ra
    db = np.hypot(x, y - length) - rb
    len2 = length * length
    dr = ra - rb
    proper = (len2 > 1e-12) & (dr * dr < len2)
    length = np.maximum(length, 1e-12)
    side = np.sqrt(np.maximum(len2 - dr * dr, 0.0))
    s = (-x * dr + y * side) / length
    mid = (x * side + y * dr) / length - ra
    out = np.where(s <= 0, da, np.where(s >= side, db, mid))
    return np.where(proper, out, da if ra >= rb else db)


@dataclass
class Segment:
    """A body segment: its name and joint pair (first joint, the one its shape rides; second, its axis end)."""
    name: str
    joints: tuple


def segments_from(doc):
    """Segments of a body from {"segments": [{"name", "joints": [a, b]}]}."""
    out = []
    for row in doc["segments"]:
        joints = tuple(row["joints"])
        if len(joints) != 2 or joints[0] == joints[1]:
            raise ValueError(f"segment {row.get('name')!r}: needs two different joints")
        out.append(Segment(str(row["name"]), joints))
    names = [s.name for s in out]
    if len(set(names)) != len(names) or any("/" in n or "." in n for n in names):
        raise ValueError("segment names must be unique and hold no '/' or '.'")
    return out


class Rig:
    """The skin joints of a body model: names in skin order and rest joint positions (model space)."""

    def __init__(self, joints, rest_positions):
        self.joints = list(joints)
        self.rest = np.asarray(rest_positions, float).reshape(len(self.joints), 3)

    @classmethod
    def from_glb(cls, glb):
        rest = glb.world_rest()
        joints = glb.skin_joints()
        return cls(joints, [rest[glb.node_index(j)][:3, 3] for j in joints])

    def index(self, joint):
        if joint not in self.joints:
            raise ValueError(f"joint {joint!r} is not a skin joint of the body")
        return self.joints.index(joint)

    def position(self, joint):
        return self.rest[self.index(joint)]


def clip_poses(glb, joints, clips):
    """[(clip name, key times, skin matrices (frames, joints, 4, 4))], the rest pose first as "rest"; clips are
    (name, path) pairs (model.clips of a manifest). Joint world x inverse bind, in the order of `joints`."""
    ibm = glb.inverse_binds()
    out = [(REST, np.zeros(1), M.skin_matrices(glb, joints, ibm, M.rest_worlds(glb)))]
    for name, path in clips:
        animation = M.load(path)
        times = animation.key_times()
        out.append((name, times, M.skin_matrices(glb, joints, ibm, M.clip_worlds(glb, animation, times))))
    return out


@dataclass
class Piece:
    """What G7 needs of a garment piece: rest sim points, triangles, max distance r (m, 0 = pin), dense skin weights on
    the rig's joints and the fabric thickness (m)."""
    id: str
    rest: np.ndarray
    faces: np.ndarray
    r: np.ndarray
    weights: np.ndarray
    thickness: float


def pieces_from_blob(blob, joints):
    """The pieces of a cooked garment blob, weights spread over `joints` by bone name. The blob is only read."""
    from garment_blob import decode
    decoded = decode(blob)
    index = [list(joints).index(name) for name, _ in decoded["bones"]]
    out = []
    for p in decoded["pieces"]:
        rest = np.asarray(p["mesh"]["rest"], float)
        w = np.zeros((len(rest), len(joints)))
        bones = np.asarray(p["skin"]["bone"], int)
        np.add.at(w, (np.arange(len(rest))[:, None], np.asarray(index)[bones]), np.asarray(p["skin"]["weight"], float))
        faces = np.asarray(p["mesh"]["faces"].tolist(), np.int64).reshape(-1, 3)
        out.append(Piece(p["id"], rest, faces, np.asarray(p["mesh"]["max_distance"], float), w,
                         float(p["material"]["thickness"])))
    return out


@dataclass
class Sphere:
    name: str
    bone: str
    center: np.ndarray
    radius: float


@dataclass
class ColliderSet:
    """Spheres and the cones joining pairs of them (a sphere in no cone collides alone). log: what the fit decided per
    segment (not written to the file)."""
    spheres: list
    cones: list
    log: list = field(default_factory=list)

    def shapes(self):
        """(sphere a, sphere b) per shape, a lone sphere as (i, i)."""
        joined = {i for c in self.cones for i in c}
        return [tuple(c) for c in self.cones] + [(i, i) for i in range(len(self.spheres)) if i not in joined]

    def segment(self, shape):
        """The segment name of a shape: its first sphere's name up to the first '/' or '.'."""
        name = self.spheres[shape[0]].name
        return name.split("/")[0].split(".")[0]

    def posed(self, rig, matrices):
        """Sphere centres (frames, spheres, 3) in the poses of skin matrices (frames, joints, 4, 4)."""
        matrices = np.asarray(matrices, float).reshape(-1, len(rig.joints), 4, 4)
        out = np.zeros((len(matrices), len(self.spheres), 3))
        for i, s in enumerate(self.spheres):
            m = matrices[:, rig.index(s.bone)]
            out[:, i] = np.einsum("fab,b->fa", m[:, :3, :], np.r_[s.center, 1.0])
        return out

    def distance(self, points, centres):
        """Union signed distance of points (frames, N, 3) to the posed shapes (centres from `posed`) and the index of
        the nearest shape (frames, N)."""
        points = np.asarray(points, float)
        best = np.full(points.shape[:-1], np.inf)
        which = np.full(points.shape[:-1], -1)
        for k, (i, j) in enumerate(self.shapes()):
            sd = round_cone_distance(points, centres[:, None, i], centres[:, None, j], self.spheres[i].radius,
                                     self.spheres[j].radius)
            closer = sd < best
            best, which = np.where(closer, sd, best), np.where(closer, k, which)
        return best, which


def write(path, shapes, body_sha256):
    """Write a set as garment.colliders.v1: {schema, body_sha256, spheres: [{name, bone, center, radius}], cones}."""
    if len(shapes.spheres) > SHAPES_MAX or len(shapes.cones) > SHAPES_MAX:
        raise ValueError(f"{len(shapes.spheres)} spheres, {len(shapes.cones)} cones: the runtime takes {SHAPES_MAX} each")
    bad = [s.name for s in shapes.spheres
           if not (np.all(np.isfinite(s.center)) and math.isfinite(s.radius) and s.radius >= 0)]
    if bad:
        raise ValueError(f"spheres {bad}: a centre or radius is not finite, or a radius is negative")
    doc = {"schema": SCHEMA, "body_sha256": body_sha256,
           # Full precision: a cut cone ends exactly where G7 allows, and rounding could move it inside a vertex.
           "spheres": [{"name": s.name, "bone": s.bone, "center": [float(v) for v in s.center],
                        "radius": float(s.radius)} for s in shapes.spheres],
           "cones": [[int(a), int(b)] for a, b in shapes.cones]}
    Path(path).write_text(json.dumps(doc, indent=1, allow_nan=False) + "\n", encoding="utf-8")
    return doc


def read(path):
    """A set from a garment.colliders.v1 file; body_sha256 goes to `log`."""
    doc = json.loads(Path(path).read_text(encoding="utf-8"))
    if doc.get("schema") != SCHEMA:
        raise ValueError(f"{path}: not {SCHEMA}")
    spheres = [Sphere(s["name"], s["bone"], np.asarray(s["center"], float), float(s["radius"])) for s in doc["spheres"]]
    cones = [tuple(c) for c in doc["cones"]]
    if any(not (0 <= a < len(spheres) and 0 <= b < len(spheres) and a != b) for a, b in cones):
        raise ValueError(f"{path}: a cone must join two different spheres")
    return ColliderSet(spheres, cones, [{"body_sha256": doc["body_sha256"]}])


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def vertex_areas(points, faces):
    """A third of the area of each adjacent triangle, per vertex (m^2)."""
    points, faces = np.asarray(points, float), np.asarray(faces, np.int64)
    area = 0.5 * np.linalg.norm(np.cross(points[faces[:, 1]] - points[faces[:, 0]], points[faces[:, 2]] - points[faces[:, 0]]), axis=1)
    out = np.zeros(len(points))
    for col in faces.T:
        np.add.at(out, col, area / 3)
    return out


def _segment_distance(points, a, b):
    ab = b - a
    t = np.clip((points - a) @ ab / max(ab @ ab, 1e-12), 0, 1)
    return np.linalg.norm(points - (a + t[:, None] * ab), axis=1)


def belonging(body, rig, segments):
    """The segment index of each body vertex (rule 1), -1 for none."""
    weights = np.asarray(body.weights, float)
    dominant = np.where(weights.max(1) > 0, weights.argmax(1), -1)
    out = np.full(len(body.points), -1)
    by_joint = {}
    for k, s in enumerate(segments):
        by_joint.setdefault(rig.index(s.joints[0]), []).append(k)
    for joint, ks in by_joint.items():
        mine = np.flatnonzero(dominant == joint)
        if not len(mine):
            continue
        near = np.stack([_segment_distance(body.points[mine], rig.position(segments[k].joints[0]),
                                           rig.position(segments[k].joints[1])) for k in ks])
        out[mine] = np.asarray(ks)[near.argmin(0)]
    return out


@dataclass
class Cone:
    a: np.ndarray
    b: np.ndarray
    ra: float
    rb: float

    def cut(self, ca, cb):
        """The cone between fractions ca and 1 - cb of its length, radii interpolated: a subset of this one."""
        d, dr = self.b - self.a, self.rb - self.ra
        return Cone(self.a + ca * d, self.b - cb * d, self.ra + ca * dr, self.rb - cb * dr)

    def distance(self, points):
        return round_cone_distance(points, self.a, self.b, self.ra, self.rb)


def _frame(u):
    e1 = np.cross(u, [0.0, 0.0, 1.0]) if abs(u[2]) < 0.9 else np.cross(u, [1.0, 0.0, 0.0])
    e1 /= np.linalg.norm(e1)
    return e1, np.cross(u, e1)


def _radii(tau, rho, length):
    """Least-volume end radii of the frustum with (1 - tau) ra + tau rb >= rho at every sample: (volume, ra, rb).
    The volume along the feasible boundary rb(ra) is convex in ra (rb(ra) is a maximum of affine functions)."""
    zero = tau < 1e-9
    low = float(rho[zero].max(initial=0.0))
    below = tau < 1 - 1e-9
    high = max(low, float(np.max(rho[below] / (1 - tau[below]), initial=0.0)))
    # The cylinder of the largest rho is feasible and V >= pi L ra^2 / 3, so the optimum has ra <= sqrt(3) max rho.
    high = max(low, min(high, math.sqrt(3) * float(rho.max(initial=0.0))))
    t, r = tau[~zero], rho[~zero]
    lo, hi = low, high
    # Nested grids bracket the minimum of a convex function to about 1e-6 of the range.
    for _ in range(4):
        ra = np.linspace(lo, hi, 33)
        rb = np.maximum(0.0, np.max((r[None] - (1 - t[None]) * ra[:, None]) / t[None], axis=1, initial=0.0))
        volume = math.pi * length * (ra * ra + ra * rb + rb * rb) / 3
        k = int(np.argmin(volume))
        lo, hi = ra[max(k - 1, 0)], ra[min(k + 1, len(ra) - 1)]
    return float(volume[k]), float(ra[k]), float(rb[k])


def cover(points, a, b):
    """Rule 2: the round cone on an axis parallel to a -> b whose spheres sit at the first and last point along it and
    which holds every point with the least volume between its spheres (beyond them lies the next segment or the end of
    the limb). The axis may shift sideways; the radii follow from the points."""
    points = np.asarray(points, float)
    u = (b - a) / np.linalg.norm(b - a)
    e1, e2 = _frame(u)
    rel = points - a
    s = rel @ u
    side = np.stack([rel @ e1, rel @ e2], axis=1)
    t0, t1 = float(s.min()), float(s.max())
    length = max(t1 - t0, 1e-3)
    tau = (s - t0) / length

    def solve(o):
        return _radii(tau, np.linalg.norm(side - o, axis=1), length)

    best = minimize(lambda o: solve(o)[0], side.mean(0), method="Nelder-Mead",
                    options={"xatol": 1e-6, "fatol": 1e-14, "maxiter": 2000})
    _, ra, rb = solve(best.x)
    base = a + best.x[0] * e1 + best.x[1] * e2
    return Cone(base + t0 * u, base + t1 * u, ra, rb)


def halves(points, cone):
    """The two halves of the points on either side of the plane through the cone's axis that holds its section's
    short direction (rule 3, the split): a boolean mask of the first half."""
    u = (cone.b - cone.a) / max(np.linalg.norm(cone.b - cone.a), 1e-12)
    rel = points - cone.a
    radial = rel - np.outer(rel @ u, u)
    radial -= radial.mean(0)
    _, vectors = np.linalg.eigh(radial.T @ radial)
    return radial @ vectors[:, -1] >= 0


def references(pieces, matrices):
    """The reference points of every piece, concatenated: (frames, vertices, 3)."""
    return np.concatenate([np.zeros((len(matrices), 0, 3))] + [M.skin_points(p.rest, p.weights, matrices)
                                                               for p in pieces], axis=1)


def g4_failing(body, pieces, clips):
    """Per clip, (frames, vertices of all pieces): True where the reference fails gate G4 (body.reach_violation, its
    one implementation: inside the posed body deeper than its max distance allows, a pin inside it at all; a closest
    point on a hole of an incomplete body passes)."""
    from .body import RESOLUTION, reach_violation
    out = []
    for _, _, matrices in clips:
        refs = references(pieces, matrices)
        skin = M.skin_points(body.points, body.weights, matrices)
        fail = np.zeros(refs.shape[:2], bool)
        for f in range(len(matrices) if refs.shape[1] else 0):
            start = 0
            for p in pieces:
                end = start + len(p.r)
                fail[f, start:end] = reach_violation(refs[f, start:end], skin[f], body.faces, p.r, p.thickness,
                                                       body.rim_faces) > RESOLUTION
                start = end
        out.append(fail)
    return out


class Reach:
    """The reference samples G7 is evaluated on (every vertex of every piece in every clip frame that passes G4), seen
    from one joint's rest frame, with its allowance m = r - thickness: a shape riding that joint holds the vertex too
    deep when sd < -m."""

    def __init__(self, rig, pieces, clips, g4):
        self.rig = rig
        self.matrices = np.concatenate([m for _, _, m in clips])
        refs = np.concatenate([references(pieces, m) for _, _, m in clips])
        allowance = np.concatenate([np.zeros(0)] + [p.r - p.thickness for p in pieces]) - RESOLUTION_M
        keep = ~np.concatenate(g4)
        self.frame = np.broadcast_to(np.arange(len(self.matrices))[:, None], keep.shape)[keep]
        self.refs = refs[keep]
        self.allowance = np.broadcast_to(allowance, keep.shape)[keep]
        self._cache = {}

    def seen_from(self, joint):
        """(points (samples, 3), allowance per sample) in the joint's rest frame."""
        if joint not in self._cache:
            inverse = np.linalg.inv(self.matrices[:, self.rig.index(joint)])[self.frame]
            q = np.einsum("nab,nb->na", inverse[:, :3, :3], self.refs) + inverse[:, :3, 3]
            self._cache = {joint: (q, self.allowance)}
        return self._cache[joint]


def _failing(cone, q, m):
    return cone.distance(q) + m < 0


def _outside(cones, points, area):
    """The skin left outside a set of cones: sum of area x depth outside the nearest one."""
    if not cones:
        return float(np.inf)
    return float(np.sum(area * np.maximum(np.min([c.distance(points) for c in cones], axis=0), 0.0)))


def _give_way(cone, q, m, points, area):
    """Rule 4 for one cone: the passing cone on its axis, cut by CUTS at each end and with end radii no larger than the
    cover's there, that leaves the least skin outside; the full length and then the larger cone on a tie; None when
    none passes. A smaller pair of radii gives a cone inside the larger one, so for each ra on a grid the largest
    passing rb is found by bisection. The samples are first thinned to the nearest one (x + m least) in each half
    millimetre along the axis, and the winner is checked against all of them."""
    bad = _failing(cone, q, m)
    if not bad.any():
        return cone
    length = float(np.linalg.norm(cone.b - cone.a))
    u = (cone.b - cone.a) / length
    rel = q[bad] - cone.a
    y = rel @ u
    x = np.linalg.norm(rel - np.outer(y, u), axis=1)
    m = m[bad]
    order = np.lexsort((x + m, np.round(y / 5e-4)))
    first = np.r_[True, np.diff(np.round(y[order] / 5e-4)) != 0]
    near = order[first]
    best = None
    for ca in CUTS:
        for cb in CUTS:
            part = cone.cut(ca, cb)
            a0, l0 = ca * length, (1 - ca - cb) * length

            def fails(ra, rb, k=near):
                return bool(np.any(profile_distance(y[k] - a0, x[k], l0, ra, rb) + m[k] < 0))

            if fails(0.0, 0.0):
                continue
            for ra in np.linspace(0.0, part.ra, STEPS + 1):
                if fails(ra, 0.0):
                    break
                lo, hi = 0.0, part.rb
                if fails(ra, hi):
                    for _ in range(30):
                        mid = 0.5 * (lo + hi)
                        lo, hi = (mid, hi) if not fails(ra, mid) else (lo, mid)
                else:
                    lo = hi
                candidate = Cone(part.a, part.b, ra, lo)
                key = (_outside([candidate], points, area), ca + cb, -(ra * ra + ra * lo + lo * lo))
                if best is None or key < best[0]:
                    best = (key, candidate, a0, l0)
    if best is None:
        return None
    _, found, a0, l0 = best
    scale = 1.0
    # The thinned samples stand for their half millimetre; if one they hid still fails, the cone shrinks to pass it.
    while np.any(profile_distance(y - a0, x, l0, found.ra * scale, found.rb * scale) + m < 0):
        scale *= 0.98
        if scale < 1e-3:
            return None
    return Cone(found.a, found.b, found.ra * scale, found.rb * scale)


def fit(body, rig, segments, pieces, clips, g4=None):
    """The collider set of one body model by the rule of this module's docstring.

    body: garment.body.Body (its skin weights on the rig's joints); rig: Rig; segments: [Segment]; pieces: [Piece] of
    every garment worn on this body; clips: clip_poses(...) output, the rest pose included; g4: g4_failing(...) when
    already computed. Returns a ColliderSet whose log lists first the G4 failures per clip that the fit did not fit
    around, then per segment its vertices, the covering cone, the samples it held and what it gave up."""
    for seg in segments:
        a, b = (rig.position(j) for j in seg.joints)
        # The cone's axis is the segment: with both joints at one rest position it has no direction.
        if not np.linalg.norm(b - a) > 1e-9:
            raise ValueError(f"segment {seg.name!r}: joints {seg.joints[0]} and {seg.joints[1]} share a rest position")
    owner = belonging(body, rig, segments)
    area = vertex_areas(body.points, body.faces)
    g4 = g4_failing(body, pieces, clips) if g4 is None else g4
    reach = Reach(rig, pieces, clips, g4)
    thickness = min((p.thickness for p in pieces), default=0.0)
    spheres, cones = [], []
    log = [dict(g4_failing={name: int(f.sum()) for (name, _, _), f in zip(clips, g4) if f.any()})]
    for k, seg in enumerate(segments):
        mine = owner == k
        started = time.time()
        entry = dict(segment=seg.name, joint=seg.joints[0], vertices=int(mine.sum()), area_m2=round(float(area[mine].sum()), 5))
        log.append(entry)
        if mine.sum() < 2:
            entry["decision"] = "no skin"
            continue
        points, weight = body.points[mine], area[mine]
        whole = cover(points, rig.position(seg.joints[0]), rig.position(seg.joints[1]))
        entry["cover"] = dict(ra_mm=round(whole.ra * 1000, 1), rb_mm=round(whole.rb * 1000, 1),
                              length_mm=round(float(np.linalg.norm(whole.b - whole.a)) * 1000, 1),
                              uncovered=int(np.sum(whole.distance(points) > 1e-9)))
        q, m = reach.seen_from(seg.joints[0])
        held = _failing(whole, q, m)
        entry["held_points"] = int(held.sum())
        if not held.any():
            chosen, entry["decision"] = [whole], "cover"
        else:
            single = [c for c in [_give_way(whole, q, m, points, weight)] if c is not None]
            first = halves(points, whole)
            pair = []
            for h in (first, ~first) if first.sum() >= 2 and (~first).sum() >= 2 else ():
                half = _give_way(cover(points[h], rig.position(seg.joints[0]), rig.position(seg.joints[1])), q, m,
                                 points[h], weight[h])
                pair += [half] if half is not None else []
            # A second shape is worth its cost only where it brings the skin closer than the cloth is thick.
            gain = (_outside(single, points, weight) - _outside(pair, points, weight)) / float(weight.sum())
            chosen = pair if gain > thickness else single
            entry["decision"] = ("split" if chosen is pair else "given way") + (
                "" if not chosen else f", length kept {np.mean([np.linalg.norm(c.b - c.a) for c in chosen]) / np.linalg.norm(whole.b - whole.a):.2f}")
        entry["radii_mm"] = [[round(c.ra * 1000, 1), round(c.rb * 1000, 1)] for c in chosen]
        outside = np.min([c.distance(points) for c in chosen], axis=0) if chosen else np.full(len(points), np.inf)
        entry["skin_outside"] = dict(share=round(float(weight[outside > 1e-9].sum() / weight.sum()), 3),
                                     depth_p90_mm=round(float(np.percentile(np.maximum(outside, 0), 90)) * 1000, 1))
        for n, c in enumerate(chosen):
            stem = seg.name if len(chosen) == 1 else f"{seg.name}/{n + 1}"
            spheres += [Sphere(f"{stem}.a", seg.joints[0], c.a, c.ra), Sphere(f"{stem}.b", seg.joints[0], c.b, c.rb)]
            cones.append((len(spheres) - 2, len(spheres) - 1))
        entry["shapes"] = len(chosen)
        entry["seconds"] = round(time.time() - started, 1)
    return ColliderSet(spheres, cones, log)


def g7(shapes, rig, pieces, clips, g4=None):
    """Gate G7 of a set over the clips: per piece {clip: {worst_mm, frames, vertices}}, and per piece the vertices
    held in any frame. A vertex is held when sd(ref, shapes) < thickness - r; with g4 (g4_failing), only samples that
    pass G4 count, and each piece also lists its G4-failing vertices."""
    out = {p.id: dict(clips={}, held=set()) for p in pieces}
    starts = np.cumsum([0] + [len(p.r) for p in pieces])
    if g4 is not None:
        for k, p in enumerate(pieces):
            failing = np.concatenate([f[:, starts[k]:starts[k + 1]] for f in g4]).any(0)
            out[p.id]["g4_vertices"] = int(failing.sum())
    if not shapes.spheres:
        return {k: dict({n: x for n, x in v.items() if n != "held"}, clips={}, held_vertices=0, worst_mm=0.0)
                for k, v in out.items()}
    for c, (name, _, matrices) in enumerate(clips):
        centres = shapes.posed(rig, matrices)
        for k, p in enumerate(pieces):
            ref = M.skin_points(p.rest, p.weights, matrices)
            sd, _ = shapes.distance(ref, centres)
            violation = p.thickness - sd - p.r
            bad = violation > 0
            if g4 is not None:
                bad &= ~g4[c][:, starts[k]:starts[k + 1]]
                violation = np.where(bad, violation, 0.0)
            if bad.any():
                vertices = np.flatnonzero(bad.any(0))
                out[p.id]["held"] |= set(vertices.tolist())
                out[p.id]["clips"][name] = dict(worst_mm=round(float(violation.max()) * 1000, 1),
                                                frames=int(bad.any(1).sum()), vertices=len(vertices))
    return {k: dict({n: x for n, x in v.items() if n != "held"}, held_vertices=len(v["held"]),
                    worst_mm=max([c["worst_mm"] for c in v["clips"].values()], default=0.0)) for k, v in out.items()}


def samples(body):
    """Body surface samples for the measures: the vertices and the triangle centroids, with area weights that sum to
    the surface area, dense skin weights and, per sample, its triangle (-1 for a vertex)."""
    points, faces, weights = body.points, body.faces, np.asarray(body.weights, float)
    tri = points[faces]
    area = 0.5 * np.linalg.norm(np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]), axis=1)
    vertex = np.zeros(len(points))
    for col in faces.T:
        np.add.at(vertex, col, area / 6)
    return dict(points=np.concatenate([points, tri.mean(1)]), area=np.concatenate([vertex, area / 2]),
                weights=np.concatenate([weights, weights[faces].mean(1)]),
                face=np.r_[np.full(len(points), -1), np.arange(len(faces))])


def footprint(body, piece, where):
    """The body samples under a piece and their zone (spec C.3 reports): a sample is under the piece when the nearest
    closest-body-point of a piece vertex lies within the piece's median edge length of it (the cloth's own spacing);
    it is loose when that vertex can reach the skin (r > 0 and r + thickness > its rest gap), fitted when max distance
    alone keeps it off. Returns (under mask, loose mask, share of the piece's free vertices whose closest body point is
    a hole's rim)."""
    from scipy.spatial import cKDTree
    _, _, gap, closest, _, rim = body.closest(piece.rest)
    e = np.concatenate([piece.faces[:, [0, 1]], piece.faces[:, [1, 2]], piece.faces[:, [2, 0]]])
    spacing = float(np.median(np.linalg.norm(piece.rest[e[:, 0]] - piece.rest[e[:, 1]], axis=1)))
    distance, near = cKDTree(closest).query(where, distance_upper_bound=spacing)
    under = np.isfinite(distance)
    near = np.minimum(near, len(closest) - 1)
    r = piece.r[near]
    loose = under & (r > 0) & (r + piece.thickness > gap[near])
    free = piece.r > 0
    return under, loose, float(rim[free].mean()) if free.any() else 0.0


def _shares(sd, area, mask):
    """Area share of masked samples outside (sd > 0) and the depth of those outside: p50, p90, max (mm)."""
    if not mask.any():
        return None
    sd, area = sd[..., mask], np.broadcast_to(area[mask], sd[..., mask].shape)
    out = sd > 0
    depth = sd[out] * 1000
    return dict(share=round(float(area[out].sum() / area.sum()), 4),
                depth_mm=[round(float(np.percentile(depth, q)), 1) for q in (50, 90)] + [round(float(depth.max()), 1)]
                if out.any() else [0.0, 0.0, 0.0])


def surface_points(shapes, centres, rings=24, around=32):
    """Points on the surface of the union of the posed shapes (centres (spheres, 3)) and the shape of each: rings of
    the cone radius along each axis and the two end caps, less those inside another shape."""
    points, owner = [], []
    for k, (i, j) in enumerate(shapes.shapes()):
        a, b = centres[i], centres[j]
        ra, rb = shapes.spheres[i].radius, shapes.spheres[j].radius
        d = b - a
        length = np.linalg.norm(d)
        u = d / length if length > 1e-9 else np.array([0.0, 1.0, 0.0])
        e1, e2 = _frame(u)
        angle = np.linspace(0, 2 * np.pi, around, endpoint=False)
        ring = np.outer(np.cos(angle), e1) + np.outer(np.sin(angle), e2)
        for t in np.linspace(0, 1, rings) if length > 1e-9 else []:
            points.append(a + t * d + (ra + t * (rb - ra)) * ring)
            owner += [k] * around
        for centre, radius, sign in ((a, ra, -1), (b, rb, 1)):
            for lat in np.linspace(0.15, 1, 6) * (np.pi / 2):
                cap = centre + radius * (np.cos(lat) * ring + sign * np.sin(lat) * u)
                points.append(cap)
                owner += [k] * around
    if not points:
        return np.zeros((0, 3)), np.zeros(0, int)
    points, owner = np.concatenate(points), np.asarray(owner)
    keep = np.ones(len(points), bool)
    for k, (i, j) in enumerate(shapes.shapes()):
        sd = round_cone_distance(points, centres[i], centres[j], shapes.spheres[i].radius, shapes.spheres[j].radius)
        keep &= (owner == k) | (sd > 1e-6)
    return points[keep], owner[keep]


def proud(shapes, body):
    """How far the shapes stand proud of the skin at rest: per surface point of the union, its distance to the body
    when it lies outside the body (closest-point normal), else 0; points whose closest body point is a hole's rim are
    left out (the body behind them is unknown). {all: [p50, p90, max] mm, segments: {name: [...]}, rim_share}."""
    if not shapes.spheres:
        return dict(all=None, segments={}, rim_share=0.0)
    points, owner = surface_points(shapes, np.stack([s.center for s in shapes.spheres]))
    _, _, distance, closest, normal, rim = body.closest(points)
    outside = np.einsum("nj,nj->n", points - closest, normal) > 0
    mm = np.where(outside, distance, 0.0) * 1000
    keep = ~rim
    stats = lambda v: [round(float(np.percentile(v, q)), 1) for q in (50, 90)] + [round(float(v.max()), 1)] if len(v) else None
    names = [shapes.segment(s) for s in shapes.shapes()]
    segments = {}
    for name in dict.fromkeys(names):
        mask = keep & np.isin(owner, [k for k, n in enumerate(names) if n == name])
        segments[name] = stats(mm[mask])
    return dict(all=stats(mm[keep]), segments=segments, rim_share=round(float(rim.mean()), 3))


def report(shapes, body, rig, pieces, clips, hz=5.0, g4=None):
    """The measures of a set on a body model (spec C.3 reports and G7):
    - per piece, the body surface under it (footprint), loose and fitted zones apart: share of area outside every
      shape and the depth outside (p50, p90, max mm), at rest and over the clip frames (sampled at most `hz`);
    - how far the shapes stand proud of the skin at rest (proud);
    - G7 per piece and clip, over every key frame, on the samples that pass G4 (g7; g4: g4_failing when computed);
    - the count of spheres and cones."""
    s = samples(body)
    out = dict(spheres=len(shapes.spheres), cones=len(shapes.cones), proud=proud(shapes, body),
               g7=g7(shapes, rig, pieces, clips, g4_failing(body, pieces, clips) if g4 is None else g4), pieces={})
    zones = {}
    for p in pieces:
        under, loose, rim = footprint(body, p, s["points"])
        zones[p.id] = (loose, under & ~loose)
        out["pieces"][p.id] = dict(hole_share=round(rim, 3), loose_area_m2=round(float(s["area"][loose].sum()), 4),
                                   fitted_area_m2=round(float(s["area"][under & ~loose].sum()), 4))
    needed = np.any([z for pair in zones.values() for z in pair], axis=0)
    for name, times, matrices in clips:
        if name != REST and len(times) > 1:
            step = max(1, int(round(1 / (hz * max(float(np.median(np.diff(times))), 1e-6)))))
            matrices = matrices[::step]
        posed = M.skin_points(s["points"][needed], s["weights"][needed], matrices)
        sd = np.full((len(matrices), len(s["points"])), -np.inf)
        if shapes.spheres:
            sd[:, needed] = shapes.distance(posed, shapes.posed(rig, matrices))[0]
        else:
            sd[:, needed] = np.inf
        key = "rest" if name == REST else "clips"
        for pid, (loose, fitted) in zones.items():
            row = out["pieces"][pid].setdefault(key, dict(loose=[], fitted=[]))
            for zone, mask in (("loose", loose), ("fitted", fitted)):
                if mask.any():
                    row[zone].append((sd[:, mask], s["area"][mask]))
    for pid in zones:
        for key in ("rest", "clips"):
            row = out["pieces"][pid].get(key)
            if row is None:
                continue
            for zone in ("loose", "fitted"):
                parts = row[zone]
                if not parts:
                    row[zone] = None
                    continue
                sd = np.concatenate([a.ravel() for a, _ in parts])
                area = np.concatenate([np.broadcast_to(w, a.shape).ravel() for a, w in parts])
                row[zone] = _shares(sd, area, np.ones(len(sd), bool))
    return out
