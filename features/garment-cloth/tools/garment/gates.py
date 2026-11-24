"""The gates of spec C.3 on a built sidecar: what the authored data must satisfy before the runtime sees it.

Each gate returns findings, one per piece (or pair), as plain dicts:
    {gate, piece, enforced, failed, worst: {clip, t, value_mm, vertices}, frames_failed, frames, clips: {clip: mm}}
`value_mm` is the largest violation in that frame (how far the inequality is broken) and `vertices` the sim vertices
breaking it there, worst first. A gate against the fallback body (B.3) is reported, not enforced.

Thresholds are meanings, not tuned numbers: the fabric thickness, its strain limit, a triangle's longest edge."""
import math
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from . import bind as B
from .colliders import round_cone_distance
from . import body as body_maps
from .body import RESOLUTION
from . import model as M
from . import sim
from .sidecar import read
from .sim import EDGE_WARP, classify_edges

REST = "rest"
LOOK_WEIGHT = 0.002
# A clip spec ("corrective:<name>", clip path or None, record) is the corrected reference at the corrective's key.
CORRECTIVE = "corrective:"


@dataclass
class GateData:
    """Everything the gates read, picklable for worker processes: the sidecar bytes, the model path, the body
    (points, faces, weights, complete), the body shapes, the fabrics per piece and the clips [(name, path)]."""
    sidecar: bytes
    model: str
    body: tuple
    shapes: dict
    fabrics: dict
    clips: list
    looks: list = field(default_factory=list)
    seats: dict = field(default_factory=dict)


def shapes_from(colliders):
    """Spheres (joint name, rest centre, radius) and cones (sphere a, sphere b) of a garment.colliders.v1 doc."""
    if colliders is None:
        return dict(spheres=[], cones=[])
    return dict(spheres=[(s["bone"], tuple(s["center"]), float(s["radius"])) for s in colliders["spheres"]],
                cones=[tuple(c) for c in colliders["cones"]])


class Pose:
    """Posing for one sidecar on its model: skin matrices per frame from the model's skeleton."""

    def __init__(self, data):
        self.data = data
        self.sc = read(data.sidecar)
        self.glb = M.load(data.model)
        self.ibm = self.sc.inverse_bind_matrices()
        self.joints = self.sc.joint_names
        self.dense = {}
        for p in self.sc.pieces:
            w = np.zeros((len(p.points), len(self.joints)))
            np.add.at(w, (np.arange(len(w))[:, None], p.joints.astype(int)), p.weights.astype(float))
            self.dense[p.id] = w

    def matrices(self, clip):
        """(times, (frames, J, 4, 4)) of a clip (name, path), a run of its key frames (name, path, start, stop), or a
        corrective's key (corrective spec)."""
        name, path = clip[:2]
        if name == REST:
            return np.zeros(1), M.skin_matrices(self.glb, self.joints, self.ibm, M.rest_worlds(self.glb))
        if name.startswith(CORRECTIVE):
            record = clip[2]
            if "pose" in record:
                worlds = {self.glb.node_index(n): np.asarray(m, float)[None]
                          for n, m in zip(record["pose"]["joints"], record["pose"]["matrices"])}
                return np.zeros(1), M.skin_matrices(self.glb, self.joints, self.ibm, worlds)
            times = np.array([record["t"]], float)
            return times, M.skin_matrices(self.glb, self.joints, self.ibm, M.clip_worlds(self.glb, M.load(path), times))
        animation = M.load(path)
        times = animation.key_times()
        if len(clip) == 4:
            times = times[clip[2]:clip[3]]
        return times, M.skin_matrices(self.glb, self.joints, self.ibm, M.clip_worlds(self.glb, animation, times))


def _record(gate, piece, enforced):
    return dict(gate=gate, piece=piece, enforced=enforced, failed=False, worst=None, frames_failed=0, frames=0, clips={})


def _frame(record, clip, t, violation, vertices=None):
    """Fold one frame's per-vertex (or per-edge) violation in metres into a record."""
    record["frames"] += 1
    worst = float(np.max(violation, initial=-np.inf))
    if worst <= RESOLUTION:
        return
    record["failed"] = True
    record["frames_failed"] += 1
    record["clips"][clip] = max(record["clips"].get(clip, 0.0), round(worst * 1000, 1))
    if record["worst"] is None or worst * 1000 > record["worst"]["value_mm"]:
        order = np.argsort(-violation)
        bad = order[violation[order] > RESOLUTION][:12]
        ids = bad if vertices is None else vertices[bad]
        record["worst"] = dict(clip=clip, t=round(float(t), 4), value_mm=round(worst * 1000, 1),
                               vertices=[int(v) for v in np.asarray(ids).ravel()][:12])


def corrective_specs(sc, clips):
    """A clip spec per corrective of a sidecar, for clip_gates: its key and the record of its bake."""
    paths = dict(clips)
    return [(CORRECTIVE + c["name"], paths.get(c.get("clip")), c) for c in sc.correctives]


def clip_gates(data, clip, gates=("G4", "G5", "G6", "G7", "G8", "G10")):
    """The per-frame gates over one clip (or the rest pose), or over a corrective's corrected reference at its key:
    {(gate, piece[, corrective]): record}."""
    pose = Pose(data)
    times, matrices = pose.matrices(clip)
    name = clip[0]
    corrective = name[len(CORRECTIVE):] if name.startswith(CORRECTIVE) else None
    if corrective is not None:
        # The reference the game evaluates while the corrective's clip holds: weight 1. A seat is the game's own gate.
        gates = tuple(g for g in gates if g != "G10")
        name = f"{clip[2].get('clip', 'pose')} with {corrective}"
    body_points, body_faces, body_weights, complete = data.body[:4]
    rim_faces = data.body[4] if len(data.body) > 4 else None
    out = {}
    posed_body = M.skin_points(body_points, body_weights, matrices) if {"G4", "G7"} & set(gates) else None
    centres, radii = [], []
    for joint, centre, radius in data.shapes["spheres"]:
        j = pose.joints.index(joint)
        centres.append(np.einsum("fab,b->fa", matrices[:, j, :3, :], np.r_[centre, 1.0]))
        radii.append(radius)
    in_cone = {i for cone in data.shapes["cones"] for i in cone}
    refs, dirs = {}, {}
    for p in pose.sc.pieces:
        w = pose.dense[p.id]
        points, b = p.points.astype(float), p.b.astype(float)
        if corrective is not None:
            delta, turn = p.correctives[corrective]
            points, b = points + delta, b + turn
        refs[p.id] = M.skin_points(points, w, matrices)
        dirs[p.id] = M.skin_directions(b, w, matrices)
    for p in pose.sc.pieces:
        fabric = data.fabrics[p.id]
        t_fab = float(fabric["thickness"])
        r = p.r.astype(float)
        pins = r == 0
        ref = refs[p.id]
        reach = None
        if "G4" in gates or "G7" in gates:
            reach = [body_maps.reach_violation(ref[k], posed_body[k], body_faces, r, t_fab, rim_faces)
                     for k in range(len(times))]
        if "G4" in gates:
            rec = out[("G4", p.id)] = _record("G4", p.id, complete)
            for k, t in enumerate(times):
                _frame(rec, name, t, reach[k])
        if "G5" in gates:
            rec = out[("G5", p.id)] = _record("G5", p.id, True)
            e = sim.edges(p.faces)
            free = ~(pins[e[:, 0]] & pins[e[:, 1]])
            e = e[free]
            rest = p.points.astype(float)
            length = np.linalg.norm(rest[e[:, 0]] - rest[e[:, 1]], axis=1)
            kinds = classify_edges(rest, p.faces, np.flatnonzero(pins))
            weft = fabric["weft_max_strain"] if fabric.get("weft_max_strain", 0) > 0 else fabric["max_edge_strain"]
            limit = np.array([fabric["max_edge_strain"] if kinds[(int(a), int(b))] == EDGE_WARP else weft for a, b in e])
            for k, t in enumerate(times):
                span = np.linalg.norm(ref[k][e[:, 0]] - ref[k][e[:, 1]], axis=1)
                violation = span - r[e[:, 0]] - r[e[:, 1]] - length * (1 + limit)
                _frame(rec, name, t, violation, vertices=e)
        if "G7" in gates and radii:
            rec = out[("G7", p.id)] = _record("G7", p.id, True)
            for k, t in enumerate(times):
                sd = np.full(len(r), np.inf)
                for a, b in data.shapes["cones"]:
                    sd = np.minimum(sd, round_cone_distance(ref[k], centres[a][k], centres[b][k], radii[a], radii[b]))
                for s in range(len(radii)):
                    if s not in in_cone:
                        sd = np.minimum(sd, np.linalg.norm(ref[k] - centres[s][k], axis=1) - radii[s])
                # As the runtime's held measure: a shape holds a vertex, pin or not, deeper than r inside its surface
                # grown by the fabric thickness.
                violation = -(sd - t_fab) - r
                # A reference that is itself inside the skin deeper than its r fails G4, not the shape (as the
                # collider fit counts it).
                masked = (reach[k] > RESOLUTION) & (violation > RESOLUTION)
                rec["masked_by_g4"] = rec.get("masked_by_g4", 0) + int(masked.sum())
                _frame(rec, name, t, np.where(masked, 0.0, violation))
    if "G6" in gates and radii:
        _, rest_m = pose.matrices((REST, None))
        for p in pose.sc.pieces:
            rec = out[("G6", p.id)] = _record("G6", p.id, True)
            r = p.r.astype(float)
            t_fab = float(data.fabrics[p.id]["thickness"])
            limbs = side_limbs(data.shapes, pose.joints, rest_m[0], p.points.astype(float), r, t_fab)
            for k, t in enumerate(times):
                violation = np.zeros(len(r))
                for limb in limbs:
                    violation = np.maximum(violation, limb.crossing(refs[p.id][k], matrices[k], r, t_fab))
                _frame(rec, name, t, violation)
    if "G10" in gates and name in data.seats:
        # The game's seat check: in a seated clip the posed body must not sink into the seat deeper than the
        # fabric that lies between them (the thickest of the outfit's).
        planes = np.asarray(data.seats[name], float).reshape(-1, 4)
        thickness = max(float(f["thickness"]) for f in data.fabrics.values())
        rec = out[("G10", "body")] = _record("G10", "body", True)
        posed = M.skin_points(body_points, body_weights, matrices)
        for k, t in enumerate(times):
            depth = -np.max(posed[k] @ planes[:, :3].T + planes[:, 3], axis=1)
            _frame(rec, name, t, depth - thickness)
    if "G8" in gates:
        for outer_id, inner_id in pose.sc.layers:
            o, i = pose.sc.piece(outer_id), pose.sc.piece(inner_id)
            rec = out[("G8", f"{outer_id}>{inner_id}")] = _record("G8", f"{outer_id}>{inner_id}", True)
            hit_t, hit_face, hit_bary = layer_hits(o, i, data.body)
            hit = np.flatnonzero(np.isfinite(hit_t))
            if not len(hit):
                continue
            corners = i.faces[hit_face[hit]]
            r_in = np.einsum("nk,nk->n", hit_bary[hit], i.r.astype(float)[corners])
            # The outer comes toward the inner by at most its backstop distance, and not at all where it is pinned.
            reach = np.minimum(o.d, o.r).astype(float)[hit]
            # The fabric thickness keeps moving cloth apart; where neither side moves the model's own distance holds
            # and only the order matters.
            moves = (reach + r_in) > 0
            need = reach + r_in + moves * (data.fabrics[outer_id]["thickness"] + data.fabrics[inner_id]["thickness"])
            for k, t in enumerate(times):
                inner_point = np.einsum("nk,nkj->nj", hit_bary[hit], refs[inner_id][k][corners])
                gap = np.einsum("nj,nj->n", inner_point - refs[outer_id][k][hit], dirs[outer_id][k][hit])
                _frame(rec, name, t, need - gap, vertices=hit)
    if corrective is not None:
        out = {(*key, corrective): dict(rec, corrective=corrective) for key, rec in out.items()}
    return out


def bake_findings(sc):
    """Per corrective and piece the bake's residual drift (reported): the cloth reset onto the corrected reference and
    held must stay within the fabric's thickness of it, or the drape is not an equilibrium of the runtime and the game
    will see it move off the corrective."""
    out = []
    for c in sc.correctives:
        for piece, drift_mm in c.get("drift_mm", {}).items():
            limit = c["thickness_mm"][piece]
            out.append(dict(gate="bake", piece=piece, corrective=c["name"], enforced=False, failed=drift_mm > limit,
                            worst=dict(clip=c.get("clip", "pose"), t=c.get("t"), value_mm=drift_mm, vertices=[]),
                            passes=c["passes"], converged=c["converged"], thickness_mm=limit, frames=1,
                            frames_failed=int(drift_mm > limit), clips={}))
    return out


def corrective_coverage(sc, manifest):
    """The clips of the manifest whose `game.corrective` names a corrective the sidecar lacks (reported): the game
    then plays that clip on the plain reference."""
    import json
    if manifest is None:
        return []
    have = set(sc.corrective_names)
    clips = json.loads(Path(manifest).read_text(encoding="utf-8"))["animations"]
    missing = {}
    for c in clips:
        wanted = c.get("game", {}).get("corrective")
        if wanted and wanted not in have:
            missing.setdefault(wanted, []).append(c["name"])
    return [dict(gate="correctives", piece=None, corrective=name, enforced=False, failed=True, clips=dict.fromkeys(used, 0),
                 worst=None, frames=0, frames_failed=0) for name, used in sorted(missing.items())]


class Limb:
    """One body shape (a cone between two spheres, or a sphere) seen from the vertices that can touch it at rest,
    for the side gate G6.

    A free vertex beside the limb's axis and within its max distance of the limb's surface at rest has a side around
    that limb: its offset from the axis in the frame of the limb's first bone. Skinning with the limb keeps that side
    by construction; a reference that has turned into the opposite half-plane while it can still reach the limb
    (within r of its surface, beside its axis) lies on the wrong side of it, where max distance and collision then
    hold the cloth. The reach gates G4 and G7 cannot see this: every such vertex may be within r of a valid point on
    the far side of the limb."""

    def __init__(self, ends, rest_m, points, r, thickness):
        (self.ja, self.a, self.ra), (self.jb, self.b, self.rb) = ends
        ca, cb = self._centres(rest_m)
        offset, beside = self._offset(points, ca, cb)
        reach = round_cone_distance(points, ca, cb, self.ra + thickness, self.rb + thickness)
        self.near = (r > 0) & beside & (reach < r)
        self.side = (offset @ self._frame(rest_m)) / np.maximum(np.linalg.norm(offset, axis=1, keepdims=True), 1e-12)

    def _centres(self, m):
        A, B = m[self.ja], m[self.jb]
        return A[:3, :3] @ self.a + A[:3, 3], B[:3, :3] @ self.b + B[:3, 3]

    def _frame(self, m):
        R = m[self.ja][:3, :3]
        return R / np.linalg.norm(R, axis=0)

    @staticmethod
    def _offset(points, ca, cb):
        """Offset from the closest point of the axis segment, and whether it lies beside the segment (a sphere is
        all side)."""
        ab = cb - ca
        length2 = float(ab @ ab)
        if length2 <= 1e-12:
            return points - ca, np.ones(len(points), bool)
        u = ((points - ca) @ ab) / length2
        return points - (ca + np.clip(u, 0, 1)[:, None] * ab), (u > 0) & (u < 1)

    def crossing(self, ref, m, r, thickness):
        """How far (m) each reference lies past the limb's axis on the side opposite its rest side, where it can
        reach the limb; 0 elsewhere."""
        if not self.near.any():
            return np.zeros(len(ref))
        ca, cb = self._centres(m)
        offset, beside = self._offset(ref, ca, cb)
        reach = round_cone_distance(ref, ca, cb, self.ra + thickness, self.rb + thickness)
        depth = -np.einsum("ij,ij->i", offset @ self._frame(m), self.side)
        return np.where(self.near & beside & (reach < r) & (depth > 0), depth, 0.0)


def side_limbs(shapes, joints, rest_m, points, r, thickness):
    """The Limbs of a piece: every cone and every sphere outside a cone."""
    spheres = [(joints.index(bone), np.asarray(centre, float), radius) for bone, centre, radius in shapes["spheres"]]
    in_cone = {i for cone in shapes["cones"] for i in cone}
    pairs = [tuple(c) for c in shapes["cones"]] + [(i, i) for i in range(len(spheres)) if i not in in_cone]
    return [Limb((spheres[a], spheres[b]), rest_m, points, r, thickness) for a, b in pairs]


def layer_hits(outer, inner, body):
    """Where each outer vertex's body direction meets the inner mesh at rest, before the body: (t, face, bary)."""
    points, faces = body[0], body[1]
    reach = sim.closest(outer.points.astype(float), points, faces)[2]
    t, f, b = np.full(len(outer.points), np.inf), np.full(len(outer.points), -1), np.zeros((len(outer.points), 3))
    for v in range(len(outer.points)):
        hit = sim.ray_hits(outer.points[v:v + 1].astype(float), outer.b[v:v + 1].astype(float),
                           inner.points.astype(float), inner.faces, reach[v])
        t[v], f[v], b[v] = hit[0][0], hit[1][0], hit[2][0]
    return t, f, b


def merge(records):
    """Fold per-clip records of one gate and piece into one."""
    out = {}
    for part in records:
        for key, rec in part.items():
            if key not in out:
                out[key] = dict(rec, clips=dict(rec["clips"]))
                continue
            acc = out[key]
            acc["frames"] += rec["frames"]
            acc["frames_failed"] += rec["frames_failed"]
            acc["failed"] |= rec["failed"]
            acc["clips"].update(rec["clips"])
            if rec["worst"] and (acc["worst"] is None or rec["worst"]["value_mm"] > acc["worst"]["value_mm"]):
                acc["worst"] = rec["worst"]
    return out


def g1_maps(sc):
    """G1: r, d, R negative or non-finite (R = +inf is the plane form), r > 1 m; a component with fewer than two
    kinematic vertices; a free vertex without a tether path (a component with no pin)."""
    out = []
    for p in sc.pieces:
        rec = _record("G1", p.id, True)
        r, d, R = (np.asarray(getattr(p, k), float) for k in ("r", "d", "R"))
        bad = ~np.isfinite(r) | (r < 0) | (r > 1) | ~np.isfinite(d) | (d < 0) | np.isnan(R) | (R < 0)
        comp = sim.components(p.faces, len(r))
        pins = np.bincount(comp, weights=(r == 0), minlength=comp.max() + 1)
        few = pins[comp] < 2
        untethered = (pins[comp] == 0) & (r > 0)
        rec.update(frames=1, failed=bool(bad.any() or few.any()),
                   counts=dict(bad_values=int(bad.sum()), in_components_with_under_two_pins=int(few.sum()),
                               without_tether_path=int(untethered.sum())))
        if rec["failed"]:
            rec["frames_failed"] = 1
            rec["worst"] = dict(clip=REST, t=0.0, value_mm=None, vertices=np.flatnonzero(bad | few)[:12].tolist())
        out.append(rec)
    return out


def render_bindings(model, piece):
    """[(primitive index, Binding)] of the grown binding of a sidecar piece in every primitive with its label."""
    return [(k, B.grow(model.positions[k], model.faces[k], label == piece.label, piece.points.astype(float),
                       piece.faces, piece.r == 0))
            for k, label in enumerate(model.labels) if np.any(label == piece.label)]


def g2_g3(model, sc):
    """G2 (seam) and G3 (cut) on the grown binding of each piece, per primitive with its label."""
    out = []
    for p in sc.pieces:
        for k, binding in render_bindings(model, p):
            out += _g2_g3(model, p, k, binding)
    return out


def _g2_g3(model, p, k, binding):
    take = model.labels[k] == p.label
    where = f"{p.id}/{model.materials[k]}"
    seam, amount = B.seam_breaks(binding, model.faces[k], take, p.r == 0)
    g2 = _record("G2", where, True)
    g2.update(frames=1, failed=bool(len(seam)), counts=dict(render_vertices=int(len(seam))))
    if len(seam):
        corners = np.unique(p.faces[binding.triangle[seam]])
        g2["frames_failed"] = 1
        g2["worst"] = dict(clip=REST, t=0.0, value_mm=None, amount_max=round(float(amount.max()), 3),
                           render_vertices=seam[:12].tolist(), vertices=corners[p.r[corners] > 0][:12].tolist())
    edges, excess = B.torn(binding, model.positions[k], model.faces[k], take)
    g3 = _record("G3", where, True)
    g3.update(frames=1, failed=bool(len(edges)), counts=dict(render_edges=int(len(edges))))
    if len(edges):
        worst = int(np.argmax(excess))
        corners = p.faces[binding.triangle[edges[worst]]]
        g3["frames_failed"] = 1
        g3["worst"] = dict(clip=REST, t=0.0, value_mm=round(float(excess[worst]) * 1000, 1),
                           render_edge=edges[worst].tolist(), vertices=np.unique(corners).tolist())
    return [g2, g3]


def holes(sc, body):
    """Report, not a gate: per piece the free vertices whose closest body point lies on an open boundary of the body
    surface, where the backstop is off because the body is missing (the complete body of R4 removes them)."""
    out = []
    for p in sc.pieces:
        rim = body_maps_rim(body, p.points.astype(float))
        free = p.r > 0
        out.append(dict(gate="holes", piece=p.id, enforced=False, failed=False, frames=1,
                        counts=dict(free_over_hole=int((rim & free).sum()), free=int(free.sum())),
                        vertices=np.flatnonzero(rim & free)[:12].tolist()))
    return out


def body_maps_rim(body, points):
    return body.closest(points)[5]


def g9_looks(model, looks):
    """G9: a look model shares the outfit's sidecar, so its labelled region must hold the same positions, labels and
    weights per joint as the outfit model (within LOOK_WEIGHT, a weight difference no pose shows)."""
    def signature(m):
        rows = {}
        for k, (x, label) in enumerate(zip(m.positions, m.labels)):
            if not np.any(label > 0):
                continue
            w = m.weights(k)
            for i in np.flatnonzero(label > 0):
                rows[(m.materials[k], tuple(np.round(x[i], 6)))] = (int(label[i]), w[i])
        return rows, m.joints
    base, joints = signature(model)
    out = []
    for path in looks:
        look, look_joints = signature(M.RenderModel(path))
        rec = _record("G9", str(path), True)
        differ = sorted(base.keys() ^ look.keys())
        if look_joints != joints:
            differ = ["joints"]
        else:
            differ += [k for k in base.keys() & look.keys()
                       if base[k][0] != look[k][0] or np.abs(base[k][1] - look[k][1]).max() > LOOK_WEIGHT]
        rec.update(frames=1, failed=bool(differ), counts=dict(positions=len(differ)))
        if differ:
            rec["worst"] = dict(clip=REST, t=0.0, value_mm=None, vertices=[], positions=[str(k) for k in differ[:5]])
        out.append(rec)
    return out
