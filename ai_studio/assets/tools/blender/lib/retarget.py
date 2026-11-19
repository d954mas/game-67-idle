"""Humanoid clip retargeting between skeletons, numpy only, so it runs in plain Python and inside Blender.

Method: every mapped target joint takes its source joint's world rotation change from the source rest,
applied after a constant rest alignment. Both rests are first brought into one body frame (left, up,
forward from hips, legs, shoulders and head), so rigs facing different axes or exported Z-up line up.
Torso joints then align rest to rest; limbs swing the target bone onto the source bone direction down
the chain (so T-pose, A-pose and odd rests reproduce the source's bone directions); feet and toes align
their horizontal heading only, which keeps the target's own foot pitch instead of standing it on
tiptoe. Local rotations are never copied: generator rigs' local frames differ by tens of degrees.

Hips translation is the source's displacement from its rest, turned into the target frame and scaled
by the ratio of hips heights over the ankles. Unmapped target joints keep their rest local transform.
Space is glTF's (Y up, metres); `lib/motion` (Z up) is called through from_gltf / to_gltf.
"""
import json
import re
import time
from pathlib import Path

import numpy as np

from ai_studio.assets.tools.blender.lib import glb as G, motion

MAPS_DIR = Path(__file__).resolve().parents[1] / "retarget_maps"
SCHEMA = "ai_studio.assets.retarget_map.v1"
SIDES = ("left", "right")
FINGERS = ("thumb", "index", "middle", "ring", "little")
TORSO = ("hips", "spine", "spine_mid", "chest", "neck", "head")
# Landmarks only: tips carry no rotation worth keying, they give the aim of their parent.
ENDS = ("head_end",) + tuple(f"{s}_{e}" for s in SIDES for e in ("hand_end", "toe_end"))
REQUIRED = ("hips", "head") + tuple(f"{s}_{j}" for s in SIDES for j in (
    "upper_arm", "lower_arm", "hand", "upper_leg", "lower_leg", "foot"))
HINGES = tuple(f"{s}_lower_leg" for s in SIDES)


def _limb_chain():
    """canonical -> (alignment parent or None for the body, aim candidates in preference order,
    roll reference kind)."""
    out = {}
    for s in SIDES:
        def n(x):
            return f"{s}_{x}"
        out[n("shoulder")] = (None, [n("upper_arm")], None)
        out[n("upper_arm")] = (n("shoulder"), [n("lower_arm")], "thumb")
        out[n("lower_arm")] = (n("upper_arm"), [n("hand")], "thumb")
        out[n("hand")] = (n("lower_arm"), [n("middle1"), n("hand_end")], "thumb")
        for f in FINGERS:
            for k in (1, 2, 3):
                out[n(f"{f}{k}")] = (n(f"{f}{k - 1}") if k > 1 else n("hand"),
                                     [n(f"{f}{k + 1}")] if k < 3 else [], None)
        out[n("upper_leg")] = (None, [n("lower_leg")], "foot")
        out[n("lower_leg")] = (n("upper_leg"), [n("foot")], "foot")
    return out


CHAIN = _limb_chain()


def secondary(kind, side, pos, both, up):
    """Roll reference of a limb at rest, or None when one rig lacks the landmarks: the thumb side for
    arm joints (palms follow the source's palms), the foot's heading for leg joints (knees follow it)."""
    def has(*names):
        return all(f"{side}_{n}" in both for n in names)

    def at(name):
        return pos[f"{side}_{name}"]

    if kind == "thumb":
        if has("thumb1", "hand"):
            return at("thumb1") - at("hand")
        if has("index1", "little1"):
            return at("index1") - at("little1")
    if kind == "foot" and has("foot", "toe"):
        return flat(at("toe") - at("foot"), up)
    return None


def flat(v, up):
    return v - np.dot(v, up) * up


def is_finger(canonical):
    return any(f in canonical for f in FINGERS)


# --- maps -------------------------------------------------------------------------------------

def name_key(name):
    """Match key of a node name: no namespace (`mixamorig:`, `mixamorig1_`), case-insensitive."""
    key = str(name).rsplit(":", 1)[-1].rsplit("|", 1)[-1]
    key = re.sub(r"^mixamorig\d*_", "", key, flags=re.IGNORECASE)
    return key.lower()


class RigMap:
    """Canonical joint -> node-name aliases for one rig family; the first alias a rig has wins, and a
    node is claimed by one canonical joint only, in the file's order."""

    def __init__(self, data, path=None):
        if data.get("schema") != SCHEMA:
            raise ValueError(f"{path or data.get('id')}: schema must be {SCHEMA}")
        self.id = data["id"]
        self.description = data.get("description", "")
        self.joints = {c: [a] if isinstance(a, str) else list(a) for c, a in data["joints"].items()}
        self.path = Path(path) if path else None

    @classmethod
    def load(cls, path):
        return cls(json.loads(Path(path).read_text(encoding="utf-8")), path)

    def resolve(self, glb, nodes=None):
        """{canonical: node index} on a Glb, over `nodes` (default: skin joints, else every node)."""
        nodes = candidate_nodes(glb) if nodes is None else nodes
        by_key = {}
        for i in nodes:
            by_key.setdefault(name_key(glb.name(i)), i)
        out, claimed = {}, set()
        for canonical, aliases in self.joints.items():
            for alias in aliases:
                i = by_key.get(name_key(alias))
                if i is not None and i not in claimed:
                    out[canonical] = i
                    claimed.add(i)
                    break
        return out


def candidate_nodes(glb):
    skins = glb.json.get("skins") or []
    joints = sorted({j for s in skins for j in s.get("joints", []) if 0 <= j < len(glb.nodes)})
    return joints or list(range(len(glb.nodes)))


def load_maps(directory=MAPS_DIR):
    return {m.id: m for m in (RigMap.load(p) for p in sorted(Path(directory).glob("*.json")))}


def get_map(spec, maps=None):
    """A RigMap from an id, a JSON path or a RigMap; None for "auto"."""
    if spec is None or isinstance(spec, RigMap) or spec == "auto":
        return None if spec == "auto" else spec
    if str(spec).endswith(".json") and Path(spec).is_file():
        return RigMap.load(spec)
    maps = maps if maps is not None else load_maps()
    if spec not in maps:
        raise ValueError(f"unknown rig map {spec!r}; have {sorted(maps)}")
    return maps[spec]


def detect_map(glb, maps=None):
    """(RigMap, resolved, coverage rows best first): the map naming most of the rig's joints."""
    maps = maps if maps is not None else load_maps()
    nodes = candidate_nodes(glb)
    rows = []
    for m in maps.values():
        resolved = m.resolve(glb, nodes)
        missing = sum(c not in resolved for c in REQUIRED)
        rows.append((missing, -len(resolved), m.id, m, resolved))
    # Fewest missing core joints first: maps share names with other meanings (Mixamo's LeftLeg is a
    # shin, SOMA's a thigh), so a raw name count can prefer a map that misreads the legs.
    rows.sort(key=lambda r: r[:3])
    if not rows or not rows[0][4]:
        raise ValueError(f"{glb.label}: no rig map matches its joint names")
    return rows[0][3], rows[0][4], [coverage(r[3], r[4]) for r in rows]


def coverage(rig_map, resolved):
    return {"map": rig_map.id, "joints": len(resolved), "of": len(rig_map.joints),
            "missing_required": [c for c in REQUIRED if c not in resolved]}


# --- math -------------------------------------------------------------------------------------

def _unit(v):
    v = np.asarray(v, float)
    n = np.linalg.norm(v, axis=-1, keepdims=True)
    return v / np.where(n < 1e-12, 1.0, n)


def orthonormal(m):
    """Rotation part of (..., 3, 3) matrices carrying scale (columns normalised)."""
    n = np.linalg.norm(m, axis=-2, keepdims=True)
    return m / np.where(n < 1e-12, 1.0, n)


def swing(a, b):
    """Smallest rotation (3x3) taking direction a onto direction b."""
    a, b = _unit(a), _unit(b)
    v = np.cross(a, b)
    c = float(np.dot(a, b))
    if c < -1 + 1e-9:
        axis = _unit(np.cross(a, [1.0, 0.0, 0.0] if abs(a[0]) < 0.9 else [0.0, 1.0, 0.0]))
        return 2.0 * np.outer(axis, axis) - np.eye(3)
    k = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + k + k @ k / (1.0 + c)


def frame(aim, secondary):
    """Columns (aim, the part of secondary perpendicular to it, their cross)."""
    x = _unit(aim)
    z = np.cross(x, secondary)
    if np.linalg.norm(z) < 1e-6:
        z = np.cross(x, [0.0, 0.0, 1.0] if abs(x[2]) < 0.9 else [1.0, 0.0, 0.0])
    z = _unit(z)
    return np.column_stack([x, np.cross(z, x), z])


def quats(m):
    """(n, 4) glTF (x, y, z, w) quaternions of (n, 3, 3) rotations."""
    m = np.asarray(m, float).reshape(-1, 3, 3)
    d0, d1, d2 = m[:, 0, 0], m[:, 1, 1], m[:, 2, 2]
    q = np.empty((len(m), 4))
    cases = [d0 + d1 + d2, d0 - d1 - d2, d1 - d0 - d2, d2 - d0 - d1]
    pick = np.argmax(np.stack(cases, 1), axis=1)
    with np.errstate(divide="ignore", invalid="ignore"):
        for k in range(4):
            sel = pick == k
            if not sel.any():
                continue
            r = m[sel]
            s = 2.0 * np.sqrt(np.maximum(1.0 + cases[k][sel], 1e-12))
            if k == 0:
                q[sel] = np.c_[(r[:, 2, 1] - r[:, 1, 2]) / s, (r[:, 0, 2] - r[:, 2, 0]) / s,
                               (r[:, 1, 0] - r[:, 0, 1]) / s, 0.25 * s]
            elif k == 1:
                q[sel] = np.c_[0.25 * s, (r[:, 0, 1] + r[:, 1, 0]) / s, (r[:, 0, 2] + r[:, 2, 0]) / s,
                               (r[:, 2, 1] - r[:, 1, 2]) / s]
            elif k == 2:
                q[sel] = np.c_[(r[:, 0, 1] + r[:, 1, 0]) / s, 0.25 * s, (r[:, 1, 2] + r[:, 2, 1]) / s,
                               (r[:, 0, 2] - r[:, 2, 0]) / s]
            else:
                q[sel] = np.c_[(r[:, 0, 2] + r[:, 2, 0]) / s, (r[:, 1, 2] + r[:, 2, 1]) / s, 0.25 * s,
                               (r[:, 1, 0] - r[:, 0, 1]) / s]
    return _unit(q)


def continuous(q):
    """Flip quaternion signs so neighbours share a hemisphere and the first has w >= 0: an engine
    that nlerps between keys never takes the long way round."""
    q = np.array(q, float)
    if q[0, 3] < 0:
        q[0] = -q[0]
    for f in range(1, len(q)):
        if np.dot(q[f], q[f - 1]) < 0:
            q[f] = -q[f]
    return q


def _qmul(a, b):
    ax, ay, az, aw = a.T
    bx, by, bz, bw = b.T
    return np.stack([aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
                     aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz], 1)


def remove_twist(q, axis):
    """Swing part of (n, 4) xyzw rotations: spin about `axis` (unit, same frame) dropped."""
    axis = _unit(axis)
    twist = np.c_[(q[:, :3] @ axis)[:, None] * axis, q[:, 3]]
    n = np.linalg.norm(twist, axis=1)
    ok = n > 1e-9  # a 180-degree swing leaves the twist undefined: keep the rotation
    twist[ok] /= n[ok, None]
    conj = twist * [-1, -1, -1, 1]
    out = _qmul(q, conj)
    out[~ok] = q[~ok]
    return out


def to_gltf(p):
    """lib/motion Z-up point back to glTF Y-up."""
    return np.array([p[0], p[2], -p[1]], float)


# --- rigs -------------------------------------------------------------------------------------

class Rig:
    """One skeleton file resolved through a map: canonical joints, topology, rest transforms."""

    def __init__(self, glb, rig_map="auto", maps=None):
        self.glb = glb
        chosen = get_map(rig_map, maps)
        if chosen is None:
            self.map, self.joints, self.candidates = detect_map(glb, maps)
        else:
            self.map, self.joints = chosen, chosen.resolve(glb)
            self.candidates = [coverage(chosen, self.joints)]
        self.coverage = coverage(self.map, self.joints)
        if "hips" not in self.joints:
            raise ValueError(f"{glb.label}: map {self.map.id} finds no hips joint")
        self.order, self.parent = [], {}
        stack = [(r, None) for r in reversed(glb.roots())]
        while stack:
            i, p = stack.pop()
            self.order.append(i)
            self.parent[i] = p
            stack.extend((c, i) for c in reversed(glb.nodes[i].get("children", [])))
        missing = [c for c, i in self.joints.items() if i not in self.parent]
        if missing:
            raise ValueError(f"{glb.label}: joints outside the scene: {missing}")
        self.rest_trs = {i: glb.local_trs(i) for i in self.order}


def body_frame(pos, joints):
    """Columns (left, up, forward) of a rig at rest from its landmark positions {canonical: (3,)}."""
    pairs = [(f"left_{j}", f"right_{j}") for j in ("upper_leg", "upper_arm", "shoulder", "foot")]
    lefts = [_unit(pos[a] - pos[b]) for a, b in pairs if a in joints and b in joints]
    if not lefts:
        raise ValueError("rig has no left/right joint pair to find its facing")
    left = _unit(np.mean(lefts, axis=0))
    top = next((c for c in ("head", "neck", "chest", "spine_mid", "spine") if c in joints), None)
    if top is not None:
        up = pos[top] - pos["hips"]
    else:
        feet = [pos[c] for c in ("left_foot", "right_foot") if c in joints]
        up = pos["hips"] - np.mean(feet, axis=0)
    up = _unit(up)
    # Snap to the nearest file axis: a rig's hips-to-head line leans a few degrees forward, and
    # aligning two rigs by their lean would tilt the whole clip by the difference.
    axis = int(np.argmax(np.abs(up)))
    if abs(up[axis]) > np.cos(np.radians(30.0)):
        up = np.eye(3)[axis] * np.sign(up[axis])
    left = _unit(left - np.dot(left, up) * up)
    return np.column_stack([left, up, np.cross(left, up)])


class Retargeter:
    """Source rig -> target rig. Build once per pair; `clip()` retargets one animation."""

    def __init__(self, source_rig, target, source_map="auto", target_map="auto", maps=None, fingers=True):
        maps = maps if maps is not None else load_maps()
        self.src = Rig(source_rig, source_map, maps)
        self.tgt = Rig(target, target_map, maps)
        self.fingers = fingers
        self.pairs = [c for c in self.tgt.joints if c in self.src.joints and c not in ENDS
                      and (fingers or not is_finger(c))]
        t = self.tgt
        self.t_rest = t.glb.world_rest()
        self.t_pos = {c: self.t_rest[i][:3, 3].copy() for c, i in t.joints.items()}
        self.t_rot = {c: orthonormal(self.t_rest[i][:3, :3]) for c, i in t.joints.items()}
        self.t_body = body_frame(self.t_pos, t.joints)
        self.up = self.t_body[:, 1]
        self.keyed = {t.joints[c] for c in self.pairs}

    # source side ---------------------------------------------------------------------------
    def _source(self, clip, anim, times):
        """(posed worlds, effective rest worlds) of the source rig under one animation."""
        rig = self.src.glb
        channels = clip.channels(anim, by_index=clip is rig)
        worlds = rig.posed_worlds(channels, times)
        hips = self.src.joints["hips"]
        override = {}
        # A clip that keys a constant offset on a joint (some generators write limbs centimetres off
        # their rig rest) means that offset is the true rest: the rig rest would float or sink the clip.
        for c, i in self.src.joints.items():
            if i == hips:
                continue
            key = (i, "translation") if clip is rig else (rig.name(i), "translation")
            if key in channels:
                keys, values, interp = channels[key]
                override[i] = G.sample_channel_at(keys, values, times, "translation", interp).mean(axis=0)

        def local(i):
            if i in override:
                _, r, s = rig.local_trs(i)
                return G.trs_matrix(override[i], r, s)
            return rig.local_matrix(i)

        return worlds, rig.walk(local)

    def _alignment(self, s_pos):
        """{canonical: 3x3} taking target rest directions onto source rest directions (both already in
        the target's body frame)."""
        t_pos, up = self.t_pos, self.up
        both = set(self.tgt.joints) & set(s_pos)
        out = {}

        def parent_align(c):
            p = CHAIN.get(c, (None,))[0]
            while p is not None and p not in out:
                p = CHAIN.get(p, (None,))[0]
            return out.get(p, np.eye(3)) if p is not None else np.eye(3)

        def flat_frame(pos, a, b):
            v = flat(pos[b] - pos[a], up)
            return frame(v, up) if np.linalg.norm(v) > 1e-6 else None

        for c in sorted(self.pairs, key=align_order):
            side = c.split("_")[0]
            if c in TORSO:
                out[c] = np.eye(3)
            elif c.endswith(("_foot", "_toe")):
                foot = c.endswith("_foot")
                tip = f"{side}_toe" if foot else f"{side}_toe_end"
                fs = flat_frame(s_pos, c, tip) if tip in both else None
                ft = flat_frame(t_pos, c, tip) if tip in both else None
                fallback = out.get(f"{side}_lower_leg" if foot else f"{side}_foot", np.eye(3))
                out[c] = fs @ ft.T if fs is not None and ft is not None else fallback
            else:
                base = parent_align(c)
                _, aims, kind = CHAIN.get(c, (None, [], None))
                aim = next((a for a in aims if a in both), None)
                if aim is None:
                    out[c] = base
                    continue
                a_t, a_s = t_pos[aim] - t_pos[c], s_pos[aim] - s_pos[c]
                sec_t, sec_s = secondary(kind, side, t_pos, both, up), secondary(kind, side, s_pos, both, up)
                if sec_t is not None and sec_s is not None and min(
                        np.linalg.norm(np.cross(_unit(a_t), _unit(sec_t))),
                        np.linalg.norm(np.cross(_unit(a_s), _unit(sec_s)))) > 0.2:
                    out[c] = frame(a_s, sec_s) @ frame(a_t, sec_t).T
                else:
                    out[c] = swing(base @ a_t, a_s) @ base
        return out

    # target side ---------------------------------------------------------------------------
    def _solve(self, world_rot, hips_pos, frames):
        """FK of the target with desired world rotations on keyed joints and the hips at hips_pos:
        (local rotations {node: (F, 3, 3)}, hips local translation (F, 3), worlds {node: (F, 4, 4)})."""
        t = self.tgt
        hips = t.joints["hips"]
        eye = np.broadcast_to(np.eye(4), (frames, 4, 4))
        worlds, local_r, hips_t = {}, {}, None
        for i in t.order:
            p = t.parent[i]
            parent = worlds[p] if p is not None else eye
            tr, r, s = t.rest_trs[i]
            if i in world_rot:
                rl = np.swapaxes(orthonormal(parent[:, :3, :3]), 1, 2) @ world_rot[i]
                local_r[i] = rl
            else:
                rl = np.broadcast_to(G.quat_matrix(r), (frames, 3, 3))
            m = np.zeros((frames, 4, 4))
            m[:, :3, :3] = rl * s[None, None, :]
            m[:, 3, 3] = 1.0
            if i == hips:
                hom = np.c_[hips_pos, np.ones(frames)]
                hips_t = np.einsum("fij,fj->fi", np.linalg.inv(parent), hom)[:, :3]
                m[:, :3, 3] = hips_t
            else:
                m[:, :3, 3] = tr
            worlds[i] = parent @ m
        return local_r, hips_t, worlds

    def clip(self, clip, anim=0, fps=30.0, root="keep", untwist=(), foot_lock=False, ground=False, legs=True,
             speed=0.0, name=None):
        """Retarget one animation of `clip` (the source rig's own file, or a clip file keyed by its node
        names).

        root: "keep" the root motion, or "in_place" (straight-line ground travel removed).
        legs: two-bone IK of the legs onto the source ankles mapped like the hips, so rigs whose thigh
        and shin proportions differ from the source still put their feet where the source did.
        speed: ground speed (m/s, target units) of a clip authored in place; foot contacts are found
        in the frame moving forward at that speed, so an in-place walk can be foot-locked.
        """
        started = time.perf_counter()
        anim = clip.animation_index(anim)
        duration = clip.duration(anim)
        n = max(2, int(round(duration * fps)) + 1)
        times = np.linspace(0.0, duration, n)
        worlds, rest = self._source(clip, anim, times)
        sj = self.src.joints
        s_pos_raw = {c: rest[i][:3, 3] for c, i in sj.items()}
        align = self.t_body @ body_frame(s_pos_raw, sj).T  # source world -> target world
        s_pos = {c: align @ p for c, p in s_pos_raw.items()}
        s_rest_rot = {c: align @ orthonormal(rest[i][:3, :3]) for c, i in sj.items()}

        def height(pos, joints):
            feet = [pos[c] for c in ("left_foot", "right_foot") if c in joints]
            return float(np.dot(pos["hips"] - np.mean(feet, axis=0), self.up)) if feet else 0.0

        hs, ht = height(s_pos, sj), height(self.t_pos, self.tgt.joints)
        scale = ht / hs if hs > 1e-6 and ht > 1e-6 else 1.0

        R = self._alignment(s_pos)
        world_rot = {}
        for c in self.pairs:
            i = sj[c]
            ws = align @ orthonormal(worlds[i][:, :3, :3])  # (F, 3, 3) in target world
            world_rot[self.tgt.joints[c]] = ws @ (s_rest_rot[c].T @ R[c] @ self.t_rot[c])
        ramp = np.linspace(0.0, 1.0, n)[:, None]
        travel = np.zeros(3)
        if root == "in_place":
            hips_src = (align @ worlds[sj["hips"]][:, :3, 3].T).T
            travel = flat(hips_src[-1] - hips_src[0], self.up)
        elif root != "keep":
            raise ValueError(f"root must be keep or in_place, not {root!r}")

        def mapped(c):
            """Source joint path in target space, about the hips rests, with in-place travel removed."""
            p = (align @ worlds[sj[c]][:, :3, 3].T).T - ramp * travel
            return self.t_pos["hips"] + scale * (p - s_pos["hips"])

        hips_pos = mapped("hips")
        # What in_place took away plus the authored speed: restores ground motion for contact finding.
        travel_t = scale * ramp * travel + np.outer(times, self.t_body[:, 2] * speed)

        report = {"clip": name or clip.animation_names()[anim], "frames": n, "fps": fps,
                  "duration_s": round(duration, 4), "source_map": self.src.map.id, "target_map": self.tgt.map.id,
                  "mapped": len(self.pairs), "target_joints": len(self.tgt.joints),
                  "unmapped_target": [c for c in self.tgt.joints if c not in self.pairs and c not in ENDS],
                  "scale": round(scale, 4), "root": root, "travel_removed_m": round(float(np.linalg.norm(travel)), 4)}
        if speed:
            report["speed_mps"] = speed

        local_r, hips_t, tw = self._solve(world_rot, hips_pos, n)
        twisted = [c for c in untwist if c in self.pairs]
        if twisted:
            self._untwist(twisted, world_rot, local_r, tw)
            local_r, hips_t, tw = self._solve(world_rot, hips_pos, n)
            report["untwisted"] = twisted
        chains = self._leg_chains()
        if legs:
            goals = {side: mapped(f"{side}_foot") for side in chains if f"{side}_foot" in sj}
            if goals:
                self._reach(chains, goals, np.ones(n), world_rot, tw)
                local_r, hips_t, tw = self._solve(world_rot, hips_pos, n)
                report["legs_ik"] = sorted(goals)
        if ground:
            shift = self._ground_shift(tw)
            hips_pos = hips_pos + shift * self.up
            report["ground_shift_m"] = round(shift, 4)
            local_r, hips_t, tw = self._solve(world_rot, hips_pos, n)
        else:
            report["floor_gap_m"] = round(-self._ground_shift(tw), 4)
        feet = {}
        plans = self._plans(chains, tw, travel_t, fps)
        for side, plan in plans.items():
            feet[side] = {"planted_frames": int(sum(plan["flags"])),
                          "slide_before_m": round(motion.foot_slide(plan["ankles"], plan["flags"]), 4)}
        if foot_lock and plans:
            for side, plan in plans.items():
                goal = np.full((n, 3), np.nan)
                weight = np.zeros(n)
                for f, (w, anchor) in enumerate(plan["plan"]):
                    if anchor is not None and w > 0:
                        goal[f] = to_gltf((anchor[0], anchor[1], plan["ankles"][f][2])) - travel_t[f]
                        weight[f] = w
                self._reach({side: chains[side]}, {side: goal}, weight, world_rot, tw)
            local_r, hips_t, tw = self._solve(world_rot, hips_pos, n)
            after = self._plans(chains, tw, travel_t, fps, flags={s: p["flags"] for s, p in plans.items()})
            for side, plan in after.items():
                anchors = plans[side]["plan"]
                err = max((float(np.hypot(a[0] - anc[1][0], a[1] - anc[1][1]))
                           for a, f, anc in zip(plan["ankles"], plan["flags"], anchors) if f and anc[1]),
                          default=0.0)
                feet[side]["slide_after_m"] = round(motion.foot_slide(plan["ankles"], plan["flags"]), 4)
                feet[side]["max_pin_error_m"] = round(err, 4)
        report["foot_lock"] = bool(foot_lock)
        report["feet"] = feet
        rotations = {i: continuous(quats(r)) for i, r in local_r.items()}
        translations = {self.tgt.joints["hips"]: hips_t}
        report["seconds"] = round(time.perf_counter() - started, 3)
        return ClipResult(report["clip"], times, rotations, translations, report)

    # passes ----------------------------------------------------------------------------------
    def _low_joints(self):
        return [self.tgt.joints[c] for s in SIDES for c in (f"{s}_foot", f"{s}_toe", f"{s}_toe_end")
                if c in self.tgt.joints]

    def _ground_shift(self, tw):
        """Vertical shift that puts the clip's usual lowest foot joint at its rest height."""
        lows = self._low_joints()
        if not lows:
            return 0.0
        rest = min(float(np.dot(self.t_rest[i][:3, 3], self.up)) for i in lows)
        per_frame = np.min(np.stack([tw[i][:, :3, 3] @ self.up for i in lows], 1), axis=1)
        return rest - float(np.percentile(per_frame, 10))

    def _untwist(self, canonicals, world_rot, local_r, tw):
        t = self.tgt
        for c in canonicals:
            i = t.joints[c]
            aim = next((a for a in CHAIN[c][1] if a in t.joints), None)
            if aim is None or i not in local_r:
                continue
            axis = self.t_rot[c].T @ (self.t_pos[aim] - self.t_pos[c])
            rest_local = G.quat_matrix(t.rest_trs[i][1])
            x = quats(rest_local.T @ local_r[i])
            rl = rest_local @ G.quat_matrices(remove_twist(x, axis))
            parent = tw[t.parent[i]][:, :3, :3] if t.parent[i] is not None else np.eye(3)
            world_rot[i] = orthonormal(parent) @ rl

    def _leg_chains(self):
        """{side: (upper, lower, foot node, thigh length, shin length)} of target legs the clip drives."""
        out = {}
        for side in SIDES:
            names = [f"{side}_upper_leg", f"{side}_lower_leg", f"{side}_foot"]
            if all(c in self.pairs for c in names):
                l1 = float(np.linalg.norm(self.t_pos[names[1]] - self.t_pos[names[0]]))
                l2 = float(np.linalg.norm(self.t_pos[names[2]] - self.t_pos[names[1]]))
                out[side] = tuple(self.tgt.joints[c] for c in names) + (l1, l2)
        return out

    def _plans(self, chains, tw, travel_t, fps, flags=None):
        """Per side: ankles (motion's Z-up, ground travel restored), plant flags and lock plan."""
        out = {}
        for side, (_, _, ft_i, l1, l2) in chains.items():
            ankles = [motion.from_gltf(p) for p in tw[ft_i][:, :3, 3] + travel_t]
            fl = flags[side] if flags else motion.detect_contacts(ankles, fps, l1 + l2)
            out[side] = {"ankles": ankles, "flags": fl, "plan": motion.lock_plan(ankles, fl)}
        return out

    def _reach(self, chains, goals, weight, world_rot, tw):
        """Two-bone IK of each leg towards goals[side] (F, 3), blended by weight (F,); NaN rows are
        skipped. The knee bends in its FK plane and the foot keeps its world rotation."""
        for side, goal in goals.items():
            up_i, lo_i, ft_i, l1, l2 = chains[side]
            new_up, new_lo = world_rot[up_i].copy(), world_rot[lo_i].copy()
            for f in range(len(goal)):
                if weight[f] <= 0 or not np.isfinite(goal[f]).all():
                    continue
                hip, knee0, ankle0 = (tw[j][f, :3, 3] for j in (up_i, lo_i, ft_i))
                target = ankle0 + (goal[f] - ankle0) * weight[f]
                if np.linalg.norm(target - ankle0) < 1e-4 * (l1 + l2):
                    continue  # already there: the solver's reach margin would bend a straight knee
                knee, ankle = motion.two_bone_ik(motion.from_gltf(hip), motion.from_gltf(knee0),
                                                 motion.from_gltf(target), l1, l2)
                knee, ankle = to_gltf(knee), to_gltf(ankle)
                s1 = swing(knee0 - hip, knee - hip)
                s2 = swing(s1 @ (ankle0 - knee0), ankle - knee)
                new_up[f] = s1 @ world_rot[up_i][f]
                new_lo[f] = s2 @ s1 @ world_rot[lo_i][f]
            world_rot[up_i], world_rot[lo_i] = new_up, new_lo


def depth(canonical):
    k = 0
    while canonical in CHAIN and CHAIN[canonical][0] is not None:
        canonical = CHAIN[canonical][0]
        k += 1
    return k


def align_order(canonical):
    """Alignment order: torso, then limbs root to tip, feet after their shins and toes after feet."""
    if canonical in TORSO:
        return 0
    side = canonical.split("_")[0]
    if canonical.endswith("_foot"):
        return depth(f"{side}_lower_leg") + 2
    if canonical.endswith("_toe"):
        return depth(f"{side}_lower_leg") + 3
    return depth(canonical) + 1


class ClipResult:
    def __init__(self, name, times, rotations, translations, report):
        self.name = name
        self.times = times
        self.rotations = rotations        # {target node: (F, 4) local xyzw}
        self.translations = translations  # {target node: (F, 3) local}
        self.report = report


# --- output -----------------------------------------------------------------------------------

def add_animation(builder, result, name=None):
    times = builder.add_accessor(np.asarray(result.times, float), bounds=True)
    samplers, channels = [], []
    for path, tracks in (("rotation", result.rotations), ("translation", result.translations)):
        for node, values in tracks.items():
            samplers.append({"input": times, "output": builder.add_accessor(np.asarray(values, float)),
                             "interpolation": "LINEAR"})
            channels.append({"sampler": len(samplers) - 1, "target": {"node": int(node), "path": path}})
    builder.json.setdefault("animations", []).append(
        {"name": name or result.name, "samplers": samplers, "channels": channels})


def write(target, results, path, mode="full", keep_animations=False):
    """Write clips on the target rig. mode "full": the target file (meshes, skin) plus the clips;
    "clip": the target's node tree only (no meshes), keyed by the same node indices and names."""
    if mode == "full":
        document = json.loads(json.dumps(target.json))
        if not keep_animations:
            document.pop("animations", None)
        builder = G.GlbBuilder(document, target.bin)
    elif mode == "clip":
        builder = G.GlbBuilder()
        keep = ("name", "translation", "rotation", "scale", "matrix", "children")
        builder.json["nodes"] = [{k: n[k] for k in keep if k in n} for n in target.nodes]
        builder.json["scenes"] = [{"nodes": list(target.roots())}]
    else:
        raise ValueError(f"mode must be full or clip, not {mode!r}")
    taken = {a.get("name") for a in builder.json.get("animations", [])}
    for r in results:
        name = r.name
        k = 2
        while name in taken:
            name, k = f"{r.name}_{k}", k + 1
        taken.add(name)
        add_animation(builder, r, name)
    return builder.write(path)
