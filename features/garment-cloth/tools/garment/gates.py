"""The gates of spec C.3 on a built sidecar: what the authored data must satisfy before the runtime sees it.

Each gate returns findings, one per piece (or pair), as plain dicts:
    {gate, piece, enforced, failed, worst: {clip, t, value_mm, vertices}, frames_failed, frames, samples,
     clips: {clip: mm}}
`value_mm` is the largest violation in that frame (how far the inequality is broken) and `vertices` the sim vertices
breaking it there, worst first. `samples` counts what the gate measured; a gate that measured nothing did not run
(`not_run` gives the reason), and fails where it is enforced.

A clip whose manifest entry names a corrective the outfit has is gated on what the game shows while it plays: the
corrected reference at weight 1 (rest + delta, its body direction, its key backstop) over every frame of the clip,
never the plain reference. Each corrective is also gated at its own key, the bake's check of its drape.

Which gates are enforced is one table (`ENFORCED_FROM`), read by `check.py` and the cook alike, so the exit code of a
check is the cook's decision for the outfit's declared stage. Stage 1 is the content shipped before the professional
scheme: it is held to what the runtime needs to load it and draw it without tearing (the rebuild, G1, G3, G9 and the
bake of any corrective it ships); every other gate reports on it. From stage 2 every gate is enforced, and a reach
gate needs the complete body (B.3) to be decided at all.

Thresholds are meanings, not tuned numbers: the fabric thickness, its strain limit, a triangle's longest edge."""
import math
from concurrent.futures import ProcessPoolExecutor
from dataclasses import dataclass, field
import os
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
# A clip spec ("crossfade:<a>><b>@<u>", path a, path b, u) is a cross-fade from every key frame of a into b's first at
# layer weight u: the pose blended by local slerp and the configurations by the runtime's rule (`blend`).
CROSSFADE = "crossfade:"
CROSSFADE_U = (0.25, 0.5, 0.75)
PER_FRAME = ("G4", "G5", "G6", "G7", "G8", "G10")
# `crossfade` reports the per-frame gates asked for on the cross-fades between seated clips that share a seat.
ALL_GATES = ("G1", "G2", "G3", "bind") + PER_FRAME + ("G9", "crossfade")
# The first declared stage at which a finding fails the check and the cook; absent: a report.
ENFORCED_FROM = {"rebuild": 1, "stale": 1, "G1": 1, "G3": 1, "G9": 1, "bake": 1,
                 "G2": 2, "bind": 2, "G4": 2, "G5": 2, "G6": 2, "G7": 2, "G8": 2, "G10": 2, "derive": 2}
# Gates decided against the body surface itself: on the fallback body (with holes) they cannot be decided.
NEEDS_COMPLETE_BODY = ("G4", "G10")
# Gates the cook runs from the sidecar and the models alone; the per-frame gates need the body and the clips.
STATIC = ("G1", "G2", "G3", "bind", "G9")
CHUNK = 16


def enforced(gate, stage):
    """Whether a failed `gate` finding stops the check and the cook of an outfit declared at `stage`."""
    return gate in ENFORCED_FROM and stage >= ENFORCED_FROM[gate]


def enforced_gates(stage):
    return {g for g in ALL_GATES if enforced(g, stage)}


def blocking(findings, stage):
    """The findings that stop the check and the cook of an outfit declared at `stage`: those of a gate the table
    enforces there that failed, measured nothing or could not be decided. check.py's exit code and the cook's
    acceptance of a receipt are this one rule."""
    return [f for f in findings
            if enforced(f["gate"], stage) and (f.get("failed") or f.get("not_run") or f.get("undecided"))]


@dataclass
class GateData:
    """Everything the gates read, picklable for worker processes: the sidecar bytes, the model path, the body
    (points, faces, weights, complete, rim faces), the body shapes, the fabrics per piece, the clips [(name, path)],
    the declared stage, per body vertex the gap a seat must leave under it (NaN where no garment covers it) and
    {clip: corrective} for the clips the game plays with a corrective of the outfit."""
    sidecar: bytes
    model: str
    body: tuple
    shapes: dict
    fabrics: dict
    clips: list
    looks: list = field(default_factory=list)
    seats: dict = field(default_factory=dict)
    stage: int = 1000000
    seat_gap: np.ndarray = None
    clip_correctives: dict = field(default_factory=dict)


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
        if name.startswith(CROSSFADE):
            from . import host as H
            leaving, entering = M.load(path), M.load(clip[2])
            times = leaving.key_times()
            skeleton = H.Skeleton.from_glb(self.glb)
            a = M.clip_worlds(self.glb, leaving, times)
            b = M.clip_worlds(self.glb, entering, entering.key_times()[:1])
            first = skeleton.pose({n: m[0] for n, m in b.items()})
            frames = [H.ramp(skeleton.pose({n: m[f] for n, m in a.items()}), first, clip[3], skeleton)
                      for f in range(len(times))]
            worlds = {node: np.stack([pose[j] for pose in frames]) for j, node in enumerate(skeleton.nodes)}
            return times, M.skin_matrices(self.glb, self.joints, self.ibm, worlds)
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


def _record(gate, piece, enforced=False):
    return dict(gate=gate, piece=piece, enforced=enforced, failed=False, worst=None, frames_failed=0, frames=0,
                samples=0, clips={})


def _frame(record, clip, t, violation, vertices=None):
    """Fold one frame's per-vertex (or per-edge) violation in metres into a record."""
    record["frames"] += 1
    record["samples"] += int(np.size(violation))
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


def clip_correctives(sc, manifest):
    """{clip: corrective} of a clip manifest for the correctives the sidecar has: the clips the game plays on the
    corrected reference (manifest `game.corrective`)."""
    import json
    if manifest is None:
        return {}
    have = set(sc.corrective_names)
    clips = json.loads(Path(manifest).read_text(encoding="utf-8"))["animations"]
    return {c["name"]: c["game"]["corrective"] for c in clips if c.get("game", {}).get("corrective") in have}


def configuration(piece, corrective):
    """(rest points, rest body direction, d, R) of a piece's reference at weight 1 of a corrective, or the plain ones
    (corrective None), float64."""
    points, b = piece.points.astype(float), piece.b.astype(float)
    if corrective is None:
        return points, b, piece.d.astype(float), piece.R.astype(float)
    delta, turn, key = piece.correctives[corrective]
    return points + delta, b + turn, key[:, 0].astype(float), key[:, 1].astype(float)


def _arc(axis, key):
    """Log map at the unit directions `axis` of the directions of `key` (rows): tangents along the arcs, as long as
    the angles."""
    toward = key / np.maximum(np.linalg.norm(key, axis=1, keepdims=True), 1e-300)
    cosine = np.einsum("ij,ij->i", toward, axis)
    t = toward - cosine[:, None] * axis
    sine = np.linalg.norm(t, axis=1)
    scale = np.where(sine > 0, np.arctan2(sine, cosine) / np.where(sine > 0, sine, 1.0), 1.0)
    return scale[:, None] * t


def blend(piece, weights):
    """(rest points, rest body direction, d, R) of a piece's reference at corrective weights {name: w} (sum at most 1),
    float64, by the runtime's rule (README, "Correctives"): positions add, the direction turns along the arcs toward the
    keys' by the coefficient-weighted log maps, d and the curvature blend with an off configuration (R = 0) taken as a
    flat body at the vertex's reach r, off only where every configuration that weighs is off, and a configuration that
    carries the whole weight gives its own values."""
    names = [k for k, w in weights.items() if w != 0]
    if not names:
        return configuration(piece, None)
    if len(names) == 1 and weights[names[0]] == 1:
        return configuration(piece, names[0])
    w = np.array([weights[k] for k in names], float)
    rest_c = max(1.0 - float(w.sum()), 0.0)
    points, b = piece.points.astype(float), piece.b.astype(float)
    shapes = [[np.asarray(a, float) for a in piece.correctives[k]] for k in names]
    points = points + sum(wk * s[0] for wk, s in zip(w, shapes))
    axis = b / np.linalg.norm(b, axis=1, keepdims=True)
    tangent = sum(wk * _arc(axis, b + s[1]) for wk, s in zip(w, shapes))
    angle = np.linalg.norm(tangent, axis=1, keepdims=True)
    along = np.where(angle > 0, np.sin(angle) / np.where(angle > 0, angle, 1.0), 1.0)
    direction = np.cos(angle) * axis + along * tangent
    reach = piece.r.astype(float)
    d, curvature, on, only = np.zeros(len(points)), np.zeros(len(points)), np.zeros(len(points), bool), None
    for c, (cd, cR) in zip([rest_c, *w], [(piece.d.astype(float), piece.R.astype(float))] +
                          [(s[2][:, 0], s[2][:, 1]) for s in shapes]):
        if c == 0:
            continue
        off = ~(cR > 0)
        d += c * np.where(off, reach, cd)
        curvature += c * np.where(off | np.isinf(cR), 0.0, 1.0 / np.where(off, 1.0, cR))
        on |= ~off
        only = cd
    R = np.where(on, np.where(curvature > 0, 1.0 / np.where(curvature > 0, curvature, 1.0), np.inf), 0.0)
    return points, direction, np.where(on, np.maximum(d, 0.0), only), R


def corrective_specs(sc, clips):
    """A clip spec per corrective of a sidecar, for clip_gates: its key and the record of its bake."""
    paths = dict(clips)
    return [(CORRECTIVE + c["name"], paths.get(c.get("clip")), c) for c in sc.correctives]


def crossfade_specs(data, clip_names=None):
    """Clip specs of the cross-fades a game shows between two seated clips that share a seat (the same seat planes in
    the outfit's data) of which one at least plays a corrective of the outfit: from every key frame of the one into
    the other's first frame, at each layer weight of CROSSFADE_U (report only)."""
    paths = dict(c[:2] for c in data.clips)
    seated = sorted(c for c in data.seats if c in paths and (clip_names is None or c in clip_names))
    out = []
    for a in seated:
        for b in seated:
            if a == b or not np.array_equal(np.asarray(data.seats[a], float), np.asarray(data.seats[b], float)):
                continue
            if a in data.clip_correctives or b in data.clip_correctives:
                out += [(f"{CROSSFADE}{a}>{b}@{u}", paths[a], paths[b], u) for u in CROSSFADE_U]
    return out


def static_findings(sc, model, looks, fabrics, stage):
    """The gates that need no clip (STATIC: G1, G2, G3, bind, G9) on a sidecar, decided for `stage`: what the cook
    re-runs on what it ships, so a shipped outfit's structural validity never rests on a recorded finding."""
    return decide(g1_maps(sc, fabrics) + g2_g3(model, sc) + g9_looks(model, looks), stage, True)


def clip_gates(data, clip, gates=PER_FRAME):
    """The per-frame gates over one clip (or the rest pose), or over a corrective's corrected reference at its key:
    {(gate, piece[, corrective]): record}. A clip the game plays with a corrective is gated on the corrected
    reference at weight 1 (`data.clip_correctives`). Every gate asked for gets a record, so one that has nothing to
    measure here (no body shapes, no layer hit) shows as not run instead of passing."""
    pose = Pose(data)
    times, matrices = pose.matrices(clip)
    name = clip_name = clip[0]
    at_key = name.startswith(CORRECTIVE)
    fading = name.startswith(CROSSFADE)
    corrective = name[len(CORRECTIVE):] if at_key else None if fading else data.clip_correctives.get(name)
    weights = {corrective: 1.0} if corrective is not None else {}
    if at_key:
        # The bake's own check of its drape at the key. A seat is the clip's gate, run where the clip itself is.
        gates = tuple(g for g in gates if g != "G10")
        name = f"{clip[2].get('clip', 'pose')} with {corrective} at its key"
    elif fading:
        pair, u = name[len(CROSSFADE):].rsplit("@", 1)
        leaving, entering = pair.split(">")
        u = float(u)
        weights = {}
        for held, share in ((data.clip_correctives.get(leaving), 1.0 - u), (data.clip_correctives.get(entering), u)):
            if held is not None:
                weights[held] = weights.get(held, 0.0) + share
        # Both clips sit on one seat, so the seat gate reads the leaving clip's.
        clip_name = leaving
        name = f"{leaving} into {entering} at {u}"
    elif corrective is not None:
        name = f"{name} with {corrective}"
    body_points, body_faces, body_weights, complete = data.body[:4]
    rim_faces = data.body[4] if len(data.body) > 4 else None
    # A G7 sample whose reference is already deeper in the skin than its r is G4's finding, but only where G4 decides.
    g4_decides = enforced("G4", data.stage) and complete
    out = {}
    posed_body = M.skin_points(body_points, body_weights, matrices) if {"G4", "G7"} & set(gates) else None
    centres, radii = [], []
    for joint, centre, radius in data.shapes["spheres"]:
        j = pose.joints.index(joint)
        centres.append(np.einsum("fab,b->fa", matrices[:, j, :3, :], np.r_[centre, 1.0]))
        radii.append(radius)
    in_cone = {i for cone in data.shapes["cones"] for i in cone}
    refs, dirs, rests, backstops = {}, {}, {}, {}
    for p in pose.sc.pieces:
        w = pose.dense[p.id]
        points, b, d, R = blend(p, weights)
        rests[p.id], backstops[p.id] = points, (d, R)
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
            rec = out[("G4", p.id)] = _record("G4", p.id)
            for k, t in enumerate(times):
                _frame(rec, name, t, reach[k])
        if "G5" in gates:
            rec = out[("G5", p.id)] = _record("G5", p.id)
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
        if "G7" in gates:
            rec = out[("G7", p.id)] = _record("G7", p.id)
            if not radii:
                rec["not_run"] = "the outfit declares no body shapes"
                continue
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
                masked = (reach[k] > RESOLUTION) & (violation > RESOLUTION) if g4_decides else np.zeros(len(r), bool)
                rec["masked_by_g4"] = rec.get("masked_by_g4", 0) + int(masked.sum())
                _frame(rec, name, t, np.where(masked, 0.0, violation))
    if "G6" in gates:
        _, rest_m = pose.matrices((REST, None))
        for p in pose.sc.pieces:
            rec = out[("G6", p.id)] = _record("G6", p.id)
            if not radii:
                rec["not_run"] = "the outfit declares no body shapes"
                continue
            r = p.r.astype(float)
            t_fab = float(data.fabrics[p.id]["thickness"])
            limbs = side_limbs(data.shapes, pose.joints, rest_m[0], rests[p.id], r, t_fab)
            for k, t in enumerate(times):
                violation = np.zeros(len(r))
                for limb in limbs:
                    violation = np.maximum(violation, limb.crossing(refs[p.id][k], matrices[k], r, t_fab))
                _frame(rec, name, t, violation)
    if "G10" in gates and clip_name in data.seats:
        # The game's seat check: where a garment covers the body, the posed body must stay above the seat top by the
        # room the runtime keeps for the fabric between them (seat_gap: twice the covering fabric's thickness).
        planes = np.asarray(data.seats[clip_name], float).reshape(-1, 4)
        covered = np.flatnonzero(np.isfinite(data.seat_gap)) if data.seat_gap is not None else np.zeros(0, int)
        rec = out[("G10", "body")] = _record("G10", "body")
        posed = M.skin_points(body_points, body_weights, matrices)
        for k, t in enumerate(times):
            # Distance outside the convex: the largest plane distance (negative inside).
            outside = np.max(posed[k][covered] @ planes[:, :3].T + planes[:, 3], axis=1, initial=-np.inf)
            _frame(rec, name, t, data.seat_gap[covered] - outside, vertices=covered)
    if "G8" in gates:
        for outer_id, inner_id in pose.sc.layers:
            o, i = pose.sc.piece(outer_id), pose.sc.piece(inner_id)
            rec = out[("G8", f"{outer_id}>{inner_id}")] = _record("G8", f"{outer_id}>{inner_id}")
            hit_t, hit_face, hit_bary = layer_hits(o, i, data.body)
            hit = np.flatnonzero(np.isfinite(hit_t))
            if not len(hit):
                rec["not_run"] = "no outer vertex lies over the inner piece at rest"
                continue
            corners = i.faces[hit_face[hit]]
            r_in = np.einsum("nk,nk->n", hit_bary[hit], i.r.astype(float)[corners])
            # The outer comes toward the inner by at most its backstop distance (at this configuration), and not at all
            # where it is pinned.
            reach = np.minimum(backstops[outer_id][0], o.r.astype(float))[hit]
            # The fabric thickness keeps moving cloth apart; where neither side moves the model's own distance holds
            # and only the order matters.
            moves = (reach + r_in) > 0
            need = reach + r_in + moves * (data.fabrics[outer_id]["thickness"] + data.fabrics[inner_id]["thickness"])
            for k, t in enumerate(times):
                inner_point = np.einsum("nk,nkj->nj", hit_bary[hit], refs[inner_id][k][corners])
                gap = np.einsum("nj,nj->n", inner_point - refs[outer_id][k][hit], dirs[outer_id][k][hit])
                _frame(rec, name, t, need - gap, vertices=hit)
    if at_key:
        out = {(*key, corrective): dict(rec, corrective=corrective) for key, rec in out.items()}
    elif fading:
        # Reported, not enforced: the measures of the cross-fade under their own gate.
        out = {("crossfade", key[0], key[1], pair): dict(rec, gate="crossfade", measure=key[0], pair=pair, enforced=False)
               for key, rec in out.items()}
    elif corrective is not None:
        # The worst frame names what it was measured on, so a card can draw the corrected reference in that clip.
        for rec in out.values():
            if rec["worst"]:
                rec["worst"].update(clip=clip_name, corrective=corrective)
    return out


def seat_gap(body, sc, fabrics):
    """Per body vertex the gap a seat top must leave under it: the runtime keeps a cloth vertex one fabric thickness
    outside the body shapes (and its backstop at the body plus that thickness) and one thickness above a convex, so a
    garment between body and seat needs twice its thickness. A body vertex is covered by a piece when the ray along its
    normal meets the piece's sim mesh at rest; layered pieces each need their own room, so the thickest covering one
    decides. NaN where no garment covers the vertex."""
    points, normals = np.asarray(body.points, float), np.asarray(body.normals, float)
    out = np.full(len(points), np.nan)
    for p in sc.pieces:
        hit = sim.ray_hits(points, normals, p.points.astype(float), p.faces)[0]
        covered = np.isfinite(hit)
        out[covered] = np.fmax(out[covered], 2 * float(fabrics[p.id]["thickness"]))
    return out


def bake_findings(sc):
    """Per corrective and piece the bake's residual drift: the cloth reset onto the corrected reference and held must
    stay within the fabric's thickness of it, or the drape is not an equilibrium of the runtime and the game will see
    it move off the corrective. Every corrective in a sidecar ships, so the table enforces this at every stage."""
    out = []
    for c in sc.correctives:
        for piece, drift_mm in c.get("drift_mm", {}).items():
            limit = c["thickness_mm"][piece]
            failed = drift_mm > limit or not c.get("converged", False)
            out.append(dict(gate="bake", piece=piece, corrective=c["name"], enforced=False, failed=failed,
                            worst=dict(clip=c.get("clip", "pose"), t=c.get("t"), value_mm=drift_mm, vertices=[]),
                            passes=c["passes"], converged=c["converged"], thickness_mm=limit, frames=1, samples=1,
                            frames_failed=int(failed), clips={}))
    return out


def derive_findings(sc):
    """What the build could not do as its rule says (the sidecar's `findings`): free vertices whose body radius no disc
    determines (their backstop is off), inner vertices the layer rule made kinematic and outer vertices it left no
    room (a gap thinner than the two fabrics). Render triangles the sim sheet leaves out are reported as `sheet`; the
    bind gate measures how far they lie from what carries them."""
    out = []
    for f in sc.findings:
        gate = "sheet" if f["kind"] == "sheet_dropped" else "derive"
        out.append(dict(gate=gate, piece=f["piece"], kind=f["kind"], enforced=False, failed=True, frames=1, samples=1,
                        frames_failed=1, counts=dict(count=f["count"]), clips={},
                        worst=dict(clip=REST, t=0.0, value_mm=None, vertices=f.get("vertices", []))))
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
            acc["samples"] += rec.get("samples", 0)
            acc["frames_failed"] += rec["frames_failed"]
            acc["failed"] |= rec["failed"]
            acc["clips"].update(rec["clips"])
            if rec.get("not_run") and not acc.get("not_run"):
                acc["not_run"] = rec["not_run"]
            if rec["worst"] and (acc["worst"] is None or rec["worst"]["value_mm"] > acc["worst"]["value_mm"]):
                acc["worst"] = rec["worst"]
    return out


def decide(findings, stage, complete):
    """Apply the enforcement table to findings of an outfit declared at `stage` (complete: its body is the declared
    complete body). A gate that measured nothing is not run, and fails where it is enforced; a gate that needs the
    complete body fails where it is enforced and the body is the fallback."""
    for f in findings:
        f["enforced"] = enforced(f["gate"], stage)
        if f["gate"] in ALL_GATES and not f.get("samples"):
            f["not_run"] = f.get("not_run") or "nothing to measure"
        if f["gate"] in NEEDS_COMPLETE_BODY and not complete:
            # What it measured stays as a report; it decides nothing.
            f["undecided"] = "the body is the fallback with holes, not a complete body"
        if f["enforced"] and (f.get("not_run") or f.get("undecided")):
            f["failed"] = True
    return findings


def g1_maps(sc, fabrics=None):
    """fabrics: {piece id: fields}. G1: r, d, R negative or non-finite (R = +inf is the plane form), r > 1 m; a component with fewer than two
    kinematic vertices; a free vertex without a tether path (a component with no pin); and every structural check
    the cook makes of the sim mesh (vertex and edge capacity, non-manifold edges, winding, degenerate triangles)."""
    from .cook import cook_mesh
    out = []
    for p in sc.pieces:
        rec = _record("G1", p.id, True)
        r, d, R = (np.asarray(getattr(p, k), float) for k in ("r", "d", "R"))
        bad = ~np.isfinite(r) | (r < 0) | (r > 1) | ~np.isfinite(d) | (d < 0) | np.isnan(R) | (R < 0)
        comp = sim.components(p.faces, len(r))
        pins = np.bincount(comp, weights=(r == 0), minlength=comp.max() + 1)
        few = pins[comp] < 2
        untethered = (pins[comp] == 0) & (r > 0)
        structure = None
        try:
            slack = 1 + float((fabrics or {}).get(p.id, {}).get("max_edge_strain", 0.0))
            cook_mesh(p.id, p.points.astype(np.float64), p.faces, np.flatnonzero(r == 0), wearable=True,
                      tether_slack=slack, max_distance=r, backstop=np.column_stack([d, R]))
        except ValueError as error:
            structure = str(error)
        rec.update(frames=1, samples=len(r), failed=bool(bad.any() or few.any() or structure),
                   counts=dict(bad_values=int(bad.sum()), in_components_with_under_two_pins=int(few.sum()),
                               without_tether_path=int(untethered.sum())))
        if structure:
            rec["structure"] = structure
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
    """G2 (seam), G3 (cut) and the bind distance on the grown binding of each piece, per primitive with its label."""
    out = []
    for p in sc.pieces:
        marks = model.marks(p.label)
        found = []
        for k, binding in render_bindings(model, p):
            found += _g2_g3(model, p, k, binding, marks[k])
        # A primitive with nothing sewn in it has no G2 to check; a piece sewn nowhere is a G2 that did not run.
        seams = [f for f in found if f["gate"] == "G2"]
        if seams and not any(f["samples"] for f in seams):
            seams[0]["not_run"] = "no render vertex of the piece must follow skinning (no seam, no split copy)"
        out += [f for f in found if f["gate"] != "G2" or f["samples"] or f is seams[0]]
    return out


def _g2_g3(model, p, k, binding, marks):
    take = model.labels[k] == p.label
    where = f"{p.id}/{model.materials[k]}"
    # The render vertices that must follow skinning: the seam inside the primitive and the split copies of
    # unlabelled vertices in any primitive, the set pin_bound pins (without its height band).
    must = marks["labelled"] & (marks["seam"] | marks["split"])
    seam, amount = B.seam_breaks(binding, must, p.r == 0)
    g2 = _record("G2", where, True)
    g2.update(frames=1, samples=int(must.sum()), failed=bool(len(seam)), counts=dict(render_vertices=int(len(seam))))
    if len(seam):
        corners = np.unique(p.faces[binding.triangle[seam]])
        g2["frames_failed"] = 1
        g2["worst"] = dict(clip=REST, t=0.0, value_mm=None, amount_max=round(float(amount.max()), 3),
                           render_vertices=seam[:12].tolist(), vertices=corners[p.r[corners] > 0][:12].tolist())
    links = B.render_links(model.positions[k], model.faces[k], take)
    edges, excess = B.torn(binding, model.positions[k], model.faces[k], take)
    g3 = _record("G3", where, True)
    g3.update(frames=1, samples=int(len(links)), failed=bool(len(edges)), counts=dict(render_edges=int(len(edges))))
    if len(edges):
        worst = int(np.argmax(excess))
        corners = p.faces[binding.triangle[edges[worst]]]
        g3["frames_failed"] = 1
        g3["worst"] = dict(clip=REST, t=0.0, value_mm=round(float(excess[worst]) * 1000, 1),
                           render_edge=edges[worst].tolist(), vertices=np.unique(corners).tolist())
    # The transfer carries a render vertex in the frame of its bound triangle: beyond the triangle's own size from it
    # the vertex is swung by that frame rather than carried with the surface.
    rec = _record("bind", where)
    bound = np.flatnonzero(take & (binding.triangle >= 0))
    corners = p.points.astype(float)[p.faces[binding.triangle[bound]]]
    distance = np.linalg.norm(model.positions[k][bound] - np.einsum("nk,nkj->nj", binding.bary[bound], corners), axis=1)
    _frame(rec, REST, 0.0, distance - binding.longest[binding.triangle[bound]], vertices=bound)
    if rec["worst"]:
        rec["worst"]["render_vertices"] = rec["worst"].pop("vertices")
        rec["worst"]["vertices"] = np.unique(p.faces[binding.triangle[rec["worst"]["render_vertices"]]]).tolist()
    rec["counts"] = dict(render_vertices=int(np.sum(distance > binding.longest[binding.triangle[bound]] + RESOLUTION)),
                         farthest_mm=round(float(distance.max(initial=0)) * 1000, 1))
    return [g2, g3, rec]


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
        rec.update(frames=1, samples=len(base), failed=bool(differ), counts=dict(positions=len(differ)))
        if differ:
            rec["worst"] = dict(clip=REST, t=0.0, value_mm=None, vertices=[], positions=[str(k) for k in differ[:5]])
        out.append(rec)
    return out


def gate_data(data, root, body=None):
    """(GateData, model, body, sidecar) of built sidecar bytes from what its build recorded: the model and body in
    `extras.build`, the colliders, clips, looks, seats and fabrics in `extras.check`. The cook and check.py both start
    here, so both gate the same inputs. body: an already loaded Body of the outfit."""
    import json
    from .outfit import load_body
    root = Path(root)
    sc = read(data)
    model = M.RenderModel(root / sc.build["model"])
    spec = sc.build["body"]
    if body is None:
        body = load_body(model, spec, lambda p: root / p)
    check = sc.check
    colliders = json.loads((root / check["colliders"]).read_text(encoding="utf-8")) if check.get("colliders") else None
    clips = [(REST, None)] + (M.clips(root / check["clips"]) if check.get("clips") else [])
    fabrics = check["fabrics"]
    gd = GateData(sidecar=bytes(data), model=str(model.path), body=(body.points, body.faces, body.weights, body.complete,
                                                                   body.rim_faces),
                  shapes=shapes_from(colliders), fabrics=fabrics, clips=clips,
                  looks=[str(root / p) for p in check.get("looks", [])],
                  seats={clip: np.asarray(planes, float) for clip, planes in check.get("seats", {}).items()},
                  stage=int(sc.build["stages"]),
                  clip_correctives=clip_correctives(sc, root / check["clips"] if check.get("clips") else None))
    return gd, model, body, sc


def run(gd, model, body, sc, wanted, clip_names=None, workers=None):
    """Every finding of the gates in `wanted` on one built sidecar, with the reports, decided by the enforcement table
    for the sidecar's stage. clip_names limits the per-frame gates to those clips (the rest pose always runs); a name
    that is not a clip of the outfit is refused."""
    known = {c[0] for c in gd.clips}
    if clip_names is not None and set(clip_names) - known:
        raise ValueError(f"unknown clips {sorted(set(clip_names) - known)}; the outfit's clips: {sorted(known - {REST})}")
    unknown_seats = set(gd.seats) - known
    if unknown_seats and "G10" in wanted:
        raise ValueError(f"seats declared for clips the outfit does not have: {sorted(unknown_seats)}")
    findings = []
    if "G1" in wanted:
        findings += g1_maps(sc, gd.fabrics)
    if {"G2", "G3", "bind"} & set(wanted):
        findings += [f for f in g2_g3(model, sc) if f["gate"] in wanted]
    per_frame = tuple(g for g in PER_FRAME if g in wanted)
    if "G10" in per_frame and gd.seat_gap is None:
        gd.seat_gap = seat_gap(body, sc, gd.fabrics)
    if per_frame:
        clips = []
        for c in gd.clips:
            if clip_names is not None and c[0] not in clip_names and c[0] != REST:
                continue
            frames = 1 if c[0] == REST else len(M.load(c[1]).key_times())
            # Runs of frames, so a long clip spreads over the workers.
            clips += [c] if c[0] == REST else [(*c, s, s + CHUNK) for s in range(0, frames, CHUNK)]
        clips += [c for c in corrective_specs(sc, gd.clips)
                  if clip_names is None or c[2].get("clip") in clip_names or "pose" in c[2]]
        fades = crossfade_specs(gd, clip_names) if "crossfade" in wanted else []
        clips += fades
        with ProcessPoolExecutor(max_workers=workers or min(4, os.cpu_count() or 1)) as pool:
            parts = list(pool.map(clip_gates, [gd] * len(clips), clips, [per_frame] * len(clips)))
        merged = merge(parts)
        if "G10" in per_frame and ("G10", "body") not in merged:
            merged[("G10", "body")] = dict(_record("G10", "body"), not_run="no seated clip with a declared seat was run")
        if "crossfade" in wanted and not fades:
            merged[("crossfade",)] = dict(_record("crossfade", None), not_run="no two seated clips sharing a seat play "
                                                                         "a corrective of the outfit")
        findings += list(merged.values())
    if "G9" in wanted:
        findings += g9_looks(model, gd.looks)
    findings += holes(sc, body) + bake_findings(sc) + derive_findings(sc)
    return decide(findings, gd.stage, body.complete)
