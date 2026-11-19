"""Pin planted feet of existing clips in place, numpy only (plain Python or inside Blender).

Per key of a clip, the ankles of the body it plays on are tested for contact (lib/motion
detect_contacts: low and slow); each plant is pinned to its mean ground position with a ramped
weight (lock_plan) and the thigh and shin are re-aimed by two-bone IK in their FK bend plane. The
foot keeps its world rotation, so only the thigh, shin and foot rotation keys of the locked clips
change; everything else, other clips included, keeps its bytes when the accessors are not shared.
A locked animation is stamped in its glTF extras and skipped by later runs.
Legs are found through the retarget maps (lib/retarget), so any mapped humanoid works.
"""
import copy

import numpy as np

from ai_studio.assets.tools.blender.lib import clip_ground as CG, glb as G, motion, retarget as RT


STAMP = "nt_foot_lock"


class LockError(ValueError):
    pass


def _legs(rig):
    """{side: (thigh, shin, foot node)} of the model, each the parent of the next."""
    out = {}
    for side in RT.SIDES:
        names = [f"{side}_upper_leg", f"{side}_lower_leg", f"{side}_foot"]
        if not all(c in rig.joints for c in names):
            continue
        nodes = tuple(rig.joints[c] for c in names)
        if rig.parent.get(nodes[1]) != nodes[0] or rig.parent.get(nodes[2]) != nodes[1]:
            raise LockError(f"{side} leg joints are not a direct chain: {[rig.glb.name(i) for i in nodes]}")
        out[side] = nodes
    return out


def _rotation_samplers(clip, anim, names):
    """{name: sampler dict} of the rotation channels on those node names in one animation."""
    a = clip.json["animations"][anim]
    found = {}
    for c in a["channels"]:
        node = c["target"].get("node")
        if node is not None and c["target"]["path"] == "rotation" and clip.name(node) in names:
            found[clip.name(node)] = a["samplers"][c["sampler"]]
    return found


def in_place_shift(clip, anim, model, worlds, keys, up):
    """(keys, 3) world offset a game takes away when it plays the clip in place: the root's local
    translation from the first to the last key, scaled by time / duration, turned into world space
    by the root's parent and stripped of its vertical part. Zero without a root translation track."""
    try:
        _, node = CG.root_channel(clip, anim)
    except CG.GroundError:
        return np.zeros((len(keys), 3))
    times, values, interp = clip.channels(anim, by_index=True)[(node, "translation")]
    duration = clip.duration(anim)
    ends = G.sample_channel_at(times, values, [0.0, duration], "translation", interp)
    travel = ends[1] - ends[0]
    target = node if model is clip else model.by_name.get(clip.name(node))
    parent = model.parent.get(target) if target is not None else None
    phase = (np.asarray(keys, float) / duration)[:, None] if duration > 0 else np.zeros((len(keys), 1))
    local = phase * travel
    world = local if parent is None else np.einsum("fij,fj->fi", worlds[parent][:, :3, :3], local)
    return world - (world @ up)[:, None] * up


def _contacts(ankles, fps, leg, height_frac, speed_frac, ramp, loop):
    """(flags, plan) per key; a loop is read as three repeats and the middle one kept, so plants that
    run through the seam are found whole and the last key gets the same pin as the first."""
    if not loop or len(ankles) < 3:
        flags = motion.detect_contacts(ankles, fps, leg, height_frac, speed_frac)
        return flags, motion.lock_plan(ankles, flags, ramp)
    cycle = list(ankles[:-1])  # the last key repeats the first
    n = len(cycle)
    flags = motion.detect_contacts(cycle * 3, fps, leg, height_frac, speed_frac)
    plan = motion.lock_plan(cycle * 3, flags, ramp)
    return flags[n:2 * n + 1], plan[n:2 * n + 1]


def foot_lock(clip, model=None, animations=None, ramp=3, height_frac=0.12, speed_frac=0.8, rig_map="auto",
              min_slide_frac=0.02, loop=False, space="world", pin="ankle"):
    """(document, binary, report) with planted feet pinned in the named animations (default: all).

    clip: Glb with the animations; model: skinned body or rig Glb they play on (default: the clip).
    A foot whose planted ankle travels less than min_slide_frac of the leg length is left alone, so a
    locked clip run through again comes back unchanged. A clip whose leg keys do not share one key
    time list is skipped with its reason in the report. With `loop` the clip is read as a cycle, so a
    plant running through the seam keeps one anchor and the last pose still meets the first.
    space "in_place": feet are pinned where a game that plays the clip in place shows them, after it
    removes the root's start-to-end horizontal travel linearly over the clip (see in_place_shift).
    pin "toe": the toe joint is the planted point (a stance on the ball of the foot); the ankle goal
    follows it, since the foot keeps its world rotation.
    """
    model = model or clip
    rig = RT.Rig(model, rig_map)
    legs = _legs(rig)
    rest = model.world_rest()
    pos = {c: rest[i][:3, 3] for c, i in rig.joints.items()}
    axes = RT.body_frame(pos, rig.joints)[:, [2, 0, 1]]  # (forward, left, up): motion's z is up

    def to_motion(p):
        return tuple(float(v) for v in axes.T @ p)

    def from_motion(p):
        return axes @ np.asarray(p, float)

    lengths = {side: (float(np.linalg.norm(rest[k][:3, 3] - rest[t][:3, 3])),
                      float(np.linalg.norm(rest[f][:3, 3] - rest[k][:3, 3]))) for side, (t, k, f) in legs.items()}
    pins = {side: rig.joints.get(f"{side}_toe", nodes[2]) if pin == "toe" else nodes[2]
            for side, nodes in legs.items()}
    document = copy.deepcopy(clip.json)
    binary = bytearray(clip.bin)
    builder = None
    report = {"map": rig.map.id, "ramp_keys": ramp, "clips": {}}
    for name in animations or clip.animation_names():
        anim = clip.animation_index(name)
        row = report["clips"][name] = {"feet": {}}
        if STAMP in clip.json["animations"][anim].get("extras", {}):
            row["skipped"] = "already locked (animation extras carry the stamp)"
            continue
        joint_names = {model.name(i) for nodes in legs.values() for i in nodes}
        samplers = _rotation_samplers(clip, anim, joint_names)
        doc_samplers = {k: document["animations"][anim]["samplers"][
            clip.json["animations"][anim]["samplers"].index(v)] for k, v in samplers.items()}
        if set(samplers) != joint_names:
            row["skipped"] = f"no rotation keys on {sorted(joint_names - set(samplers))}"
            continue
        inputs = {s["input"] for s in samplers.values()}
        keys = clip.accessor(next(iter(samplers.values()))["input"])[:, 0]
        if len(inputs) > 1 and not all(np.allclose(clip.accessor(i)[:, 0], keys) for i in inputs):
            row["skipped"] = "leg rotation keys do not share key times"
            continue
        if any(s.get("interpolation", "LINEAR") == "CUBICSPLINE" for s in samplers.values()):
            row["skipped"] = "cubic-spline leg keys"
            continue
        channels = clip.channels(anim, by_index=clip is model)
        worlds = model.posed_worlds(channels, keys)
        if space not in ("world", "in_place") or pin not in ("ankle", "toe"):
            raise LockError(f"space must be world or in_place and pin ankle or toe, not {space!r}, {pin!r}")
        shift = (in_place_shift(clip, anim, model, worlds, keys, axes[:, 2]) if space == "in_place"
                 else np.zeros((len(keys), 3)))
        row["space"], row["pin"] = space, pin
        fps = 1.0 / float(np.median(np.diff(keys))) if len(keys) > 1 else 30.0
        new = {}
        for side, (t, k, f) in legs.items():
            l1, l2 = lengths[side]
            point = pins[side]
            ankles = [to_motion(p) for p in worlds[point][:, :3, 3] - shift]
            flags, plan = _contacts(ankles, fps, l1 + l2, height_frac, speed_frac, ramp, loop)
            slide = motion.foot_slide(ankles, flags)
            row["feet"][side] = {"planted_keys": int(sum(flags)), "slide_before": round(slide, 4), "_flags": flags,
                                 "locked": slide >= min_slide_frac * (l1 + l2)}
            if not row["feet"][side]["locked"]:
                continue
            rot = {j: RT.orthonormal(worlds[j][:, :3, :3]).copy() for j in (t, k, f)}
            for i, (weight, anchor) in enumerate(plan):
                if anchor is None or weight <= 0:
                    continue
                hip, knee0, ankle0 = (worlds[j][i, :3, 3] for j in (t, k, f))
                goal = from_motion((anchor[0], anchor[1], ankles[i][2])) + shift[i] + ankle0 - worlds[point][i, :3, 3]
                target = ankle0 + (goal - ankle0) * weight
                if np.linalg.norm(target - ankle0) < 1e-4 * (l1 + l2):
                    continue
                knee, ankle = (from_motion(p) for p in motion.two_bone_ik(
                    to_motion(hip), to_motion(knee0), to_motion(target), l1, l2))
                s1 = RT.swing(knee0 - hip, knee - hip)
                s2 = RT.swing(s1 @ (ankle0 - knee0), ankle - knee)
                rot[t][i] = s1 @ rot[t][i]
                rot[k][i] = s2 @ s1 @ rot[k][i]
            parent = RT.orthonormal(worlds[model.parent[t]][:, :3, :3]) if t in model.parent else np.eye(3)
            local = {t: np.swapaxes(parent, 1, 2) @ rot[t],
                     k: np.swapaxes(rot[t], 1, 2) @ rot[k],
                     f: np.swapaxes(rot[k], 1, 2) @ rot[f]}
            new.update(local)
        for j, local in new.items():
            sampler = doc_samplers[model.name(j)]
            old = clip.accessor(sampler["output"])
            q = RT.quats(local)
            q[(q * old).sum(axis=1) < 0] *= -1  # keep each key's hemisphere: no long-way blends
            changed = np.abs(q - old).max() > 1e-6
            if changed:
                builder = CG.store_output(document, binary, sampler, q, builder)
        row["keys"], row["_keys"] = len(keys), keys
        row["_space"] = space
        # Locked plants change what reads as planted, so a second lock would move them again.
        document["animations"][anim].setdefault("extras", {})[STAMP] = {"ramp": ramp, "loop": bool(loop), "space": space,
                                                                       "pin": pin}
    if builder is not None:
        document, binary = builder.json, builder.payload
    fixed = G.Glb(copy.deepcopy(document), bytes(binary))
    body = fixed if model is clip else model
    for name, row in report["clips"].items():
        keys = row.pop("_keys", None)
        if keys is None:
            continue
        worlds = body.posed_worlds(fixed.channels(name, by_index=body is fixed), keys)
        shift = (in_place_shift(fixed, fixed.animation_index(name), body, worlds, keys, axes[:, 2])
                 if row.pop("_space") == "in_place" else 0.0)
        for side in legs:
            flags = row["feet"][side].pop("_flags")
            ankles = [to_motion(p) for p in worlds[pins[side]][:, :3, 3] - shift]
            row["feet"][side]["slide_after"] = round(motion.foot_slide(ankles, flags), 4)
    return document, bytes(binary), report
