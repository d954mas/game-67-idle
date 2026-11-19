"""Animation check of a skinned GLB, or of a clip GLB applied to a model GLB: broken keys, pops,
teleports, foot slide on planted feet, soles through the floor, and optionally the loop seam.

    python clip_check.py character.glb [--animations Walk Idle] [--fps 30] [--feet LeftFoot RightFoot]
    python clip_check.py clips.glb --model character.glb [--loop]

Thresholds are physical (m/s, m/s^2), so the sampling rate does not move them, and are scaled by
the model's height against a 1.75 m human (--body-scale 1 keeps them absolute). Foot joints come from
--feet, else common names (mixamorig:LeftFoot, foot_l, LeftFoot, *Foot*) are detected. Clips with root
motion are judged in world space; an in-place locomotion clip shows its stride as slide.
Slide is measured on a grid of skinned sole points, each planted while near the sole's lowest height
and slow; a plant slides by its least-moving spot, so a pivot on the heel or ball is not a slide. --airborne GLOB... names clips held or thrown by another actor: no slide or floor findings.
"""
import fnmatch
import re
import sys
import weakref
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

import numpy as np  # noqa: E402

from ai_studio.assets.tools.blender.lib import glb as G, motion, report as R  # noqa: E402

TOOL = "clip_check"
# Limits are for a 1.75 m human and scale with the model's height (body_scale "auto"); pop and teleport
# are 0.05 m/frame^2 and 0.3 m/frame at 60 fps, the take-QA thresholds.
DEFAULTS = {"model": None, "animations": None, "fps": 30.0, "feet": None, "floor_y": 0.0,
            "pop_accel": 180.0, "pop_ratio": 4.0, "pop_window": 6, "teleport_speed": 18.0,
            "slide": 0.05, "sink": 0.03, "float": 0.03, "loop": False, "loop_seam": 0.02,
            "skinned_soles": True, "in_place": "auto", "body_scale": "auto", "plant_height": 0.01,
            "airborne": [], "model_scale": None, "baseline": None,
            "pop_stray": 0.02}
SCALED = ("pop_accel", "teleport_speed", "slide", "sink", "float", "loop_seam", "plant_height", "pop_stray")
HUMAN_HEIGHT = 1.75
SAMPLES = 5
SIDE = {"left": ("left", "l"), "right": ("right", "r")}
# A part under the floor is noticed when it is gone long enough to register (s) and deep enough to
# hide a foot (game m); a shorter or shallower dip reads as motion in a fast action. Calibrated on
# reviewed clips: every sink players saw lasted 0.4 s or more at 0.4 m or more.
VISIBLE_DIP = (0.3, 0.1)
SHORT_CLIP = 0.4  # s: pop limits grow as (SHORT_CLIP / duration)^2 below it, as a strike packs its swing
PARTS = (("toe", ("toe",)), ("sole", ("foot", "ankle", "heel")),
         ("knee", ("knee", "calf", "shin", "leg", "thigh")),
         ("hand", ("hand", "finger", "thumb", "wrist", "palm", "index", "middle", "ring", "pinky", "arm", "elbow")))
# Per-model facts, so one loaded model checked against many clip files is measured once.
_MODEL_FACTS = weakref.WeakKeyDictionary()


def _key(name):
    key = re.sub(r"[^a-z]", "", name.split(":")[-1].lower())
    for prefix in ("mixamorig", "def", "bip", "ctrl"):
        if key.startswith(prefix) and len(key) > len(prefix) + 3:
            key = key[len(prefix):]
    return key


def detect_feet(names):
    """One foot joint per side by common rig naming; the most generic (shortest) name wins."""
    found = {}
    for name in names:
        key = _key(name)
        if not ("foot" in key or "ankle" in key) or any(w in key for w in ("toe", "end", "ik", "pole", "target",
                                                                             "roll", "twist", "index")):
            continue
        core = key.replace("foot", "").replace("ankle", "")
        for side, (word, letter) in SIDE.items():
            if core in (word, letter) or core.startswith(word) or core.endswith(word):
                if side not in found or len(_key(found[side])) > len(key):
                    found[side] = name
    return [found[s] for s in ("left", "right") if s in found]


def _accel(p, times):
    """Second derivative per sample of (frames, ..., 3) positions on a possibly uneven time grid, m/s^2."""
    out = np.zeros_like(p)
    if len(p) < 3:
        return out
    shape = (-1,) + (1,) * (p.ndim - 1)
    h0, h1 = np.diff(times)[:-1].reshape(shape), np.diff(times)[1:].reshape(shape)
    v0 = (p[1:-1] - p[:-2]) / h0
    v1 = (p[2:] - p[1:-1]) / h1
    out[1:-1] = 2 * (v1 - v0) / (h0 + h1)
    return out


def _times(clip, anim, fps):
    keys = clip.key_times(anim)
    if not len(keys):
        return np.zeros(1)
    start, end = float(keys[0]), float(keys[-1])
    if fps and fps > 0 and end > start:
        return np.unique(np.r_[np.arange(start, end, 1.0 / fps), end])
    return keys


def check_keys(clip, anim, name, out):
    bad, scale = [], []
    for (index, path), (times, values, interp) in clip.channels(anim, by_index=True).items():
        node = clip.name(index)
        if not (np.isfinite(times).all() and np.isfinite(values).all()):
            bad.append(f"{node}.{path}")
            continue
        if interp == "CUBICSPLINE":  # in-tangent, value, out-tangent triplets: tangents may be zero
            values = values.reshape(len(times), 3, -1)[:, 1]
        if path == "scale" and (values <= 0).any():
            scale.append(f"{node}: {values.min():.3g}")
    if bad:
        out.append(R.finding("finite", "high", f"{name}: {len(bad)} channels hold NaN or infinity",
                             "re-bake the action; check for zero-length quaternions", {"animation": name,
                                                                                         "channels": bad[:SAMPLES]}))
    if scale:
        out.append(R.finding("scale", "high", f"{name}: {len(scale)} channels key zero or negative scale",
                             "key a positive scale (a zero scale hides the bone's mesh, negative mirrors it)",
                             {"animation": name, "channels": scale[:SAMPLES]}))
    return not bad


def check_pops(names, positions, times, opts, anim, out, row, leaves=(), planted=None):
    """Second-difference spikes on joints that carry other joints: a chain's tip (finger, toe end)
    multiplies every rotation above it into speed, so a fast swing reads as a pop there. `planted`
    ({joint: per-frame bool}) names foot and toe joints whose spikes at a foot contact are the
    landing itself, not a pop."""
    worst, hits = 0.0, []
    floor = 0.36  # m/s^2: 1e-4 m/frame^2 at 60 fps keeps a perfectly smooth stretch from dividing by zero
    w = opts["pop_window"]
    duration = float(times[-1] - times[0]) if len(times) else 0.0
    limit = opts["pop_accel"] * max(1.0, (SHORT_CLIP / max(duration, 1e-3)) ** 2)
    row["pop_limit_mps2"] = round(limit, 1)
    every = np.linalg.norm(_accel(positions, times), axis=2)
    for j, joint in enumerate(names):
        if joint in leaves:
            continue
        acc = every[:, j]
        worst = max(worst, float(acc.max(initial=0)))
        contact = (planted or {}).get(joint)
        for k in np.flatnonzero(acc > limit):
            if contact is not None and contact[max(0, k - 1):k + 2].any():
                continue
            local = float(np.median(acc[max(1, k - w):k + w + 1]))
            # A pop goes out and comes back: the sample strays from the chord of its neighbours by more
            # than the chord is long. A fast swing or a strike travels through, however hard it accelerates.
            p = positions[:, j]
            stray = float(np.linalg.norm(p[k] - (p[k - 1] + p[k + 1]) / 2))
            if acc[k] > opts["pop_ratio"] * max(local, floor) and stray > np.linalg.norm(p[k + 1] - p[k - 1]):
                hits.append((float(acc[k]), joint, float(times[k]), stray))
    row["max_accel_mps2"] = round(worst, 1)
    if hits:
        hits.sort(reverse=True)
        joints = sorted({h[1] for h in hits})
        # A one-frame stray under a couple of centimetres (a toe flick) is a pixel or two in game.
        seen = max(h[3] for h in hits) >= opts["pop_stray"]
        out.append(R.finding("pop", "medium" if seen else "low", f"{anim}: {len(hits)} pops on {len(joints)} "
                             f"joints (worst {hits[0][0]:.0f} m/s^2 on {hits[0][1]} at {hits[0][2]:.3f} s, "
                             f"strays up to {max(h[3] for h in hits):.3f} {opts.get('units', 'm')})",
                             "smooth or re-key the spike (a flipped quaternion or a stray key)",
                             {"animation": anim, "pops": [{"joint": j, "time": round(t, 3), "accel": round(a),
                                                           "stray": round(d, 4)} for a, j, t, d in hits[:SAMPLES]]},
                             round(hits[0][0], 1), round(limit, 1)))


def check_teleport(roots, names, positions, times, opts, anim, out):
    if len(times) < 2:
        return
    dt = np.diff(times)
    for j, joint in enumerate(names):
        if joint not in roots:
            continue
        speed = np.linalg.norm(np.diff(positions[:, j], axis=0), axis=1) / dt
        bad = np.flatnonzero(speed > opts["teleport_speed"])
        if len(bad):
            k = int(bad[np.argmax(speed[bad])])
            out.append(R.finding("teleport", "high", f"{anim}: {joint} jumps at {speed[k]:.1f} m/s near "
                                 f"{times[k + 1]:.3f} s", "remove the jump key, or split the clip at it",
                                 {"animation": anim, "joint": joint, "time": round(float(times[k + 1]), 3)},
                                 round(float(speed[k]), 2), opts["teleport_speed"]))


def leg_length(model, foot):
    world = model.world_rest()
    i = model.by_name[foot]
    chain = [i]
    while len(chain) < 3 and chain[-1] in model.parent:
        chain.append(model.parent[chain[-1]])
    pts = [world[c][:3, 3] for c in chain if c in world]
    length = sum(float(np.linalg.norm(a - b)) for a, b in zip(pts, pts[1:]))
    return length if length > 1e-3 else 0.9


def _descendants(model, i):
    out, stack = set(), [i]
    while stack:
        n = stack.pop()
        out.add(n)
        stack.extend(model.nodes[n].get("children", []))
    return out


def sole_vertices(model, feet, height):
    """{foot: vertex indices}: per foot the lowest rest vertex of each cell of a 4 x 3 (heel to toe,
    side to side) grid over its sole (skinned vertices led by the foot joint or a joint below it,
    within `height` of the lowest). Feet without such vertices are left out."""
    nodes = [i for i, n in enumerate(model.nodes) if "skin" in n and "mesh" in n]
    owner = model.skin_vertex_joints(nodes) if nodes else None
    if owner is None:
        return {}
    rest = {i: m[None] for i, m in model.world_rest().items()}
    out = {}
    for foot in feet:
        led = np.flatnonzero(np.isin(owner, list(_descendants(model, model.by_name[foot]))))
        if not len(led):
            continue
        pts = model.skinned_points(rest, nodes, led)[0]
        low = pts[:, 1] <= pts[:, 1].min() + height
        led, pts = led[low], pts[low]
        flat = pts[:, [0, 2]] - pts[:, [0, 2]].mean(axis=0)
        axis = np.linalg.svd(flat, full_matrices=False)[2][0] if len(pts) > 1 else np.array([1.0, 0.0])
        cell = []
        for v, bins in ((flat @ axis, 4), (flat @ (axis[1], -axis[0]), 3)):
            cell.append(np.minimum((bins * (v - v.min()) / max(float(np.ptp(v)), 1e-9)).astype(int), bins - 1))
        picks = []
        for key in set(zip(cell[0].tolist(), cell[1].tolist())):
            members = np.flatnonzero((cell[0] == key[0]) & (cell[1] == key[1]))
            picks.append(int(led[members[np.argmin(pts[members, 1])]]))
        out[foot] = sorted(set(picks))
    return out


def plant_slide(points):
    """(slide, path) of one plant from (frames, points, 2) horizontal planted sole points.

    A foot pivoting on its heel or ball keeps that spot still; a slide moves all of it. So the sole
    is followed as a rigid 2D motion from the plant's first frame, and the slide is the travel of its
    least-moving spot: the sole points, their midpoints and centre."""
    first = points[0]
    spots = first
    if len(first) > 1:
        pairs = (spots[:, None] + spots[None]) / 2
        spots = np.vstack([pairs.reshape(-1, 2), first.mean(axis=0)[None]])
    centre0 = first.mean(axis=0)
    a = first - centre0
    moved = []
    for frame in points:
        centre = frame.mean(axis=0)
        b = frame - centre
        angle = np.arctan2((a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]).sum(), (a * b).sum()) if len(a) > 1 else 0.0
        c, s = np.cos(angle), np.sin(angle)
        rel = spots - centre0
        moved.append(np.c_[c * rel[:, 0] - s * rel[:, 1], s * rel[:, 0] + c * rel[:, 1]] + centre)
    moved = np.array(moved)
    travel = np.linalg.norm(moved - spots, axis=2).max(axis=0)
    best = int(np.argmin(travel))
    return float(travel[best]), float(np.linalg.norm(np.diff(moved[:, best], axis=0), axis=1).sum())


def check_feet(model, feet, names, positions, times, opts, anim, out, row, soles=None):
    """Planted-foot drift of sole points (a grid over the sole; the ankle joint without a skinned mesh),
    each judged planted while it is near the sole's lowest height and slow: a heel that lifts in a
    lunge is not a plant, and a plant slides by its least-moving spot (see plant_slide). An in-place clip (feet end where they started) is judged in the frame of
    the treadmill it implies: the median velocity of the feet while they are low."""
    if len(times) < 4 or not feet:
        return {}
    t = times - times[0]
    fps = (len(times) - 1) / max(t[-1], 1e-9)
    tracks = {f: np.array([motion.from_gltf(p) for p in positions[:, names.index(f)]]) for f in feet}
    legs = {f: leg_length(model, f) * opts["unit"] for f in feet}
    travel = max(float(np.linalg.norm(tr[-1, :2] - tr[0, :2])) for tr in tracks.values())
    in_place = opts["in_place"] == "yes" or opts["in_place"] == "auto" and travel < 0.25 * min(legs.values())
    ground = np.zeros(2)
    if in_place:
        # Treadmill speed from frames within 1.5 % of leg length of the foot's lowest point: a walking
        # foot stays this low only while planted. Every foot must agree; one drifting alone is a slide.
        per_foot = np.array([np.median(np.gradient(tr[:, :2], t, axis=0)[tr[:, 2] - tr[:, 2].min()
                                                                          <= 0.015 * legs[f]], axis=0)
                             for f, tr in tracks.items()])
        mean = per_foot.mean(axis=0)
        speed = float(np.linalg.norm(mean))
        if speed > 0.05 * min(legs.values()) and (np.linalg.norm(per_foot - mean, axis=1) <= 0.25 * speed).all():
            ground = mean
            for tr in tracks.values():
                tr[:, :2] -= ground * t[:, None]
    row.update(in_place=bool(in_place), ground_speed=round(float(np.linalg.norm(ground)), 3), feet={})
    contacts = {}
    for foot, tr in tracks.items():
        if soles and foot in soles:
            points = [np.array([motion.from_gltf(q) for q in soles[foot][:, k]]) for k in range(soles[foot].shape[1])]
            for p in points:
                p[:, :2] -= ground * t[:, None]
            height = opts["plant_height"] / legs[foot]
        else:
            points, height = [tr], 0.12
        # Sole points are judged against the sole's lowest point, so an edge vertex that sits higher
        # never counts as down; the ankle alone is judged against its own lowest point.
        floor = min(float(p[:, 2].min()) for p in points) if len(points) > 1 else None
        flags = np.array([motion.detect_contacts([tuple(q) for q in p], fps, legs[foot], height_frac=height,
                                                 floor=floor) for p in points], bool)
        flat = np.stack([p[:, :2] for p in points], axis=1)
        planted = flags.any(axis=0)
        contacts[foot] = planted
        drift, at, total = 0.0, None, 0.0
        for start, end in motion.contact_runs(planted.tolist()):
            # The sole points down for most of the plant carry it; a heel or toe that only touches at
            # its ends rolls in or out of it and is not followed.
            share = flags[:, start:end + 1].mean(axis=1)
            core = share >= min(0.5, share.max())
            d, path = plant_slide(flat[start:end + 1, core])
            total += path
            if d > drift:
                drift, at = d, float(times[start])
        row["feet"][foot] = {"planted_frames": int(planted.sum()), "max_drift_m": round(drift, 4),
                             "slide_total_m": round(total, 4), "points": "sole" if points[0] is not tr else "ankle"}
        if drift > opts["slide"]:
            out.append(R.finding("foot_slide", "medium", f"{anim}: planted {foot} slides {drift:.3f} {opts['units']} "
                                 f"(plant from {at:.3f} s)", "lock the foot during the plant (foot IK / "
                                 "foot lock), or match root speed to the stride",
                                 {"animation": anim, "joint": foot, "time": round(at, 3)},
                                 round(drift, 4), round(opts["slide"], 4)))
    return contacts


def body_height(model):
    skinned = [i for i, n in enumerate(model.nodes) if "skin" in n and "mesh" in n]
    parts = model.parts(include={model.name(i) for i in skinned} if skinned else None)
    if parts:
        y = np.concatenate([p.positions[:, 1] for p in parts])
        return float(y.max() - y.min())
    world = model.world_rest()
    joints = [j for s in model.json.get("skins", []) for j in s["joints"] if j in world]
    if joints:
        y = np.array([world[j][1, 3] for j in joints])
        return float(y.max() - y.min()) / 0.85  # ankle-to-head span is about 85 % of standing height
    return None


def lowest_points(model, worlds, use_skin):
    """Lowest skinned vertex per pose of posed_worlds, or None without skinned vertices."""
    nodes = [i for i, n in enumerate(model.nodes) if "skin" in n and "mesh" in n] if use_skin else []
    return model.lowest_skinned_y(worlds, nodes) if nodes else None


def lowest_part(model, worlds, frame):
    """(part, joint) of the lowest skinned vertex in one pose: toe, sole, knee, hand or body."""
    nodes = [i for i, n in enumerate(model.nodes) if "skin" in n and "mesh" in n]
    owner = model.skin_vertex_joints(nodes) if nodes else None
    if owner is None:
        return None, None
    pose = {i: m[frame:frame + 1] for i, m in worlds.items()}
    joint = model.name(int(owner[int(np.argmin(model.skinned_points(pose, nodes, np.arange(len(owner)))[0, :, 1]))]))
    return _part(joint), joint


def _part(joint):
    key = _key(joint)
    return next((part for part, words in PARTS if any(w in key for w in words)), "body")


def baseline_low(clip, model, opts, lim):
    """Lowest point of the baseline animation (from the clip file, else the model), or None."""
    name = opts["baseline"]
    source = clip if name in clip.animation_names() else model if name in model.animation_names() else None
    if source is None:
        return None
    channels = source.channels(name, by_index=True) if source is model else source.channels(name)
    worlds = model.posed_worlds(channels, _times(source, name, opts["fps"]))
    lows = lowest_points(model, worlds, lim["skinned_soles"])
    if lows is None:
        return None
    return float(np.min(lows)) * lim["unit"]


def check_soles(model, feet, names, positions, worlds, times, opts, anim, out, row, reference=None):
    """Lowest point per frame against the floor, or against `reference` (the baseline clip's lowest
    point) when given. A sink reports how long it lasts and which part goes lowest."""
    lows = lowest_points(model, worlds, opts["skinned_soles"])
    source = "skinned mesh"
    if lows is None:
        if not feet:
            return
        source = "foot joints"
        lows = positions[:, [names.index(f) for f in feet], 1].min(axis=1)
    else:
        lows = np.asarray(lows, float) * opts["unit"]
    lows = np.asarray(lows, float)
    k = int(np.argmin(lows))
    row.update(lowest_y=round(float(lows[k]), 4), lowest_source=source)
    level = opts["floor_y"] if reference is None else reference
    below = "the floor" if reference is None else f"the {opts['baseline']} baseline's lowest point"
    depth = level - lows[k]
    if depth > opts["sink"]:
        deep = (level - lows) > opts["sink"]
        start = end = k
        while start > 0 and deep[start - 1]:
            start -= 1
        while end < len(lows) - 1 and deep[end + 1]:
            end += 1
        step = float(np.median(np.diff(times))) if len(times) > 1 else 0.0
        duration = float(times[end] - times[start]) + step
        part, joint = lowest_part(model, worlds, k) if source == "skinned mesh" else ("sole", feet[0])
        brief = duration < VISIBLE_DIP[0] or depth < VISIBLE_DIP[1]
        row.update(dip_m=round(float(depth), 4), dip_s=round(duration, 3), dip_part=part)
        out.append(R.finding("sole_sink", "low" if brief else "medium",
                             f"{anim}: {part} ({joint}) goes {depth:.3f} {opts['units']} below {below} at "
                             f"{times[k]:.3f} s, for {duration:.2f} s", "raise the root/hips or fix the foot keys so "
                             "soles stay on the floor", {"animation": anim, "time": round(float(times[k]), 3),
                                                         "duration": round(duration, 3), "part": part, "joint": joint},
                             round(float(depth), 4), opts["sink"]))
    if source == "skinned mesh" and lows[k] - opts["floor_y"] > opts["float"]:
        out.append(R.finding("sole_float", "low", f"{anim}: the mesh never comes within {opts['float']:.3f} "
                             f"{opts['units']} of the floor (lowest {lows[k] - opts['floor_y']:+.3f} {opts['units']})",
                             "lower the root, unless the clip is airborne by design", {"animation": anim},
                             round(float(lows[k]), 4)))


def check_loop(names, positions, roots, opts, anim, out, row):
    root = [names.index(r) for r in roots if r in names][:1]
    rel = positions - positions[:, root] if root else positions
    seam = float(np.linalg.norm(rel[-1] - rel[0], axis=1).max())
    row["loop_seam_m"] = round(seam, 4)
    if seam > opts["loop_seam"]:
        j = names[int(np.argmax(np.linalg.norm(rel[-1] - rel[0], axis=1)))]
        out.append(R.finding("loop_seam", "medium", f"{anim}: last pose differs from the first by {seam:.3f} m "
                             f"({j})", "key the last frame as a copy of the first", {"animation": anim, "joint": j},
                             round(seam, 4), opts["loop_seam"]))


def _skin_usable(model, out):
    """Whether skinned vertices can be evaluated; when not, soles fall back to the foot joints."""
    world = model.world_rest()
    for i, node in enumerate(model.nodes):
        if "skin" not in node or "mesh" not in node:
            continue
        try:
            model.skinned_positions(world, i)
        except (G.GlbError, IndexError, KeyError) as error:
            out.append(R.finding("skin", "high", f"skinned mesh {model.name(i)} cannot be posed: {error}",
                                 "re-export the armature with the mesh; soles are measured on foot joints",
                                 {"node": model.name(i)}))
            return False
    return True


def _model_fact(model, name, compute):
    facts = _MODEL_FACTS.setdefault(model, {})
    if name not in facts:
        facts[name] = compute()
    return facts[name]


def _skin_findings(model):
    """(usable, findings) of _skin_usable, run once per model."""
    notes = []
    return R.guarded(notes, "skin", _skin_usable, model, notes), notes


def check_animation(clip, model, anim, joint_ids, opts, lim, findings, row, reference=None):
    row["duration"] = round(clip.duration(anim), 4)
    if not check_keys(clip, anim, anim, findings):
        return
    if clip is model:
        channels = clip.channels(anim, by_index=True)
        targets = {i for i, _ in channels}
    else:
        channels = clip.channels(anim)
        stray = sorted({n for n, _ in channels if n not in model.by_name})
        if stray:
            findings.append(R.finding("targets", "info", f"{anim}: {len(stray)} channels target nodes the model "
                                      "lacks", "", {"animation": anim, "nodes": stray[:SAMPLES]}, len(stray)))
        targeted = {n for n, _ in channels}
        clip_nodes = {c["target"]["node"] for c in clip.json["animations"][clip.animation_index(anim)]["channels"]
                      if "node" in c["target"]}
        ambiguous = sorted(set(model.duplicate_names()) & targeted | set(clip.duplicate_names(clip_nodes)))
        if ambiguous:
            findings.append(R.finding("ambiguous_targets", "medium", f"{anim}: {len(ambiguous)} channel targets "
                                      "share their name with another node; only the first node of each name "
                                      "is animated", "give every animated node a unique name in the clip and "
                                      "the model", {"animation": anim, "nodes": ambiguous[:SAMPLES]},
                                      len(ambiguous), 0))
        targets = {model.by_name[n] for n in targeted if n in model.by_name}
    ids = joint_ids or sorted(targets)
    times = _times(clip, anim, opts["fps"])
    worlds = model.posed_worlds(channels, times)
    missing = [model.name(i) for i in ids if i not in worlds]
    if missing:
        findings.append(R.finding("joints_in_scene", "high", f"{anim}: {len(missing)} joints are not in the scene; "
                                  "they are left out", "export the whole armature", {"animation": anim,
                                                                                     "joints": missing[:SAMPLES]}))
        ids = [i for i in ids if i in worlds]
    names = [model.name(i) for i in ids]
    roots = {model.name(i) for i in ids if model.parent.get(i) not in ids}
    positions = (np.stack([worlds[i][:, :3, 3] for i in ids], axis=1) if ids else np.zeros((len(times), 0, 3))) * lim["unit"]
    row["samples"] = len(times)
    if not len(ids):
        return
    feet = opts["feet"] or detect_feet(names)
    feet = [f for f in feet if f in names]
    row["foot_joints"] = feet
    leaves = {model.name(i) for i in ids if not any(c in ids for c in model.nodes[i].get("children", []))}
    planted = {}
    if any(fnmatch.fnmatchcase(anim, glob) for glob in opts["airborne"] or ()):
        # Held, carried or thrown by another actor: the floor and the plants say nothing about the clip.
        findings.append(R.finding("airborne", "info", f"{anim}: skipped: airborne clip (foot slide and floor "
                                  "contact not judged)", "", {"animation": anim}))
    else:
        soles = None
        if feet and lim["skinned_soles"]:
            nodes = [i for i, n in enumerate(model.nodes) if "skin" in n and "mesh" in n]
            picks = _model_fact(model, ("soles", tuple(feet), lim["plant_height"]),
                                lambda: sole_vertices(model, feet, lim["plant_height"]))
            soles = {f: model.skinned_points(worlds, nodes, idx) * lim["unit"] for f, idx in picks.items()}
        contacts = check_feet(model, feet, names, positions, times, lim, anim, findings, row, soles)
        for foot, flags in contacts.items():
            # A foot near the floor is landing or pushing off: its snap into place is the step itself.
            height = positions[:, names.index(foot), 1]
            low = flags | (height <= height.min() + 10 * lim["plant_height"])
            for j in _descendants(model, model.by_name[foot]):
                planted[model.name(j)] = low
        check_soles(model, feet, names, positions, worlds, times, lim, anim, findings, row,
                    None if anim == opts["baseline"] else reference)
    check_pops(names, positions, times, lim, anim, findings, row, leaves, planted)
    check_teleport(roots, names, positions, times, lim, anim, findings)
    if opts["loop"]:
        check_loop(names, positions, roots, lim, anim, findings, row)
    if not feet:
        findings.append(R.finding("feet", "info", f"{anim}: no foot joints found; slide not measured",
                                  "pass --feet LeftFoot RightFoot", {"animation": anim}))


def group_by_cause(findings):
    """One finding per check and severity, naming every clip it hit, worst clip first; a file of 50
    clips then reads as a few causes instead of 50 lines."""
    groups, out = {}, []
    for f in findings:
        anim = (f.get("where") or {}).get("animation")
        if anim is None:
            out.append(f)
        else:
            groups.setdefault((f["check"], f["severity"]), []).append(f)
    for (check, severity), items in groups.items():
        clips = list(dict.fromkeys(f["where"]["animation"] for f in items))
        if len(clips) == 1:
            out.extend(items)
            continue
        items.sort(key=lambda f: -f["value"] if isinstance(f.get("value"), (int, float)) else 0)
        clips = list(dict.fromkeys(f["where"]["animation"] for f in items))
        worst = items[0]
        out.append(R.finding(check, severity, f"{len(clips)} clips: {', '.join(clips[:8])}"
                             f"{' ...' if len(clips) > 8 else ''}; worst {worst['message']}", worst.get("hint", ""),
                             {"animation": worst["where"]["animation"], "clips": clips,
                              "items": [{**f["where"], "message": f["message"]} for f in items]},
                             worst.get("value"), worst.get("limit")))
    return out


def check(path_or_glb, **options):
    opts = R.resolve_options(TOOL, DEFAULTS, overrides=options)
    try:
        clip = path_or_glb if isinstance(path_or_glb, G.Glb) else G.Glb.load(path_or_glb)
    except Exception as error:  # noqa: BLE001 - reported as a finding
        return R.unreadable(TOOL, path_or_glb, error)
    model = opts["model"]
    try:
        model = clip if model is None else model if isinstance(model, G.Glb) else G.Glb.load(model)
    except Exception as error:  # noqa: BLE001 - reported as a finding
        return R.unreadable(TOOL, opts["model"], error)
    findings, rows = [], []
    # With model_scale every length (limits, floor_y, reports) is in game metres.
    unit = float(opts["model_scale"]) if opts["model_scale"] else 1.0
    scale = opts["body_scale"]
    if scale == "auto":
        height = _model_fact(model, "height", lambda: body_height(model))
        scale = height * unit / HUMAN_HEIGHT if height and height > 1e-3 else 1.0
    lim = {**opts, **{k: opts[k] * float(scale) for k in SCALED},
           "unit": unit, "units": "game m" if opts["model_scale"] else "m"}
    skins = model.json.get("skins", [])
    if opts["skinned_soles"]:
        usable, notes = _model_fact(model, "skin", lambda: _skin_findings(model))
        findings.extend(notes)
        lim["skinned_soles"] = usable
    else:
        lim["skinned_soles"] = opts["skinned_soles"]
    joint_ids = sorted({j for s in skins for j in s["joints"] if 0 <= j < len(model.nodes)})
    reference = None
    if opts["baseline"]:
        reference = R.guarded(findings, "baseline", baseline_low, clip, model, opts, lim)
        if reference is None:
            findings.append(R.finding("baseline", "info", f"baseline animation {opts['baseline']!r} not found in the "
                                      "clip or the model; sinks are measured against the floor"))
    anims = opts["animations"] or clip.animation_names()
    if not anims:
        findings.append(R.finding("animations", "info", "no animations in the file"))
    for anim in anims:
        row = {"animation": anim}
        rows.append(row)
        R.guarded(findings, f"animation {anim}", check_animation, clip, model, anim, joint_ids, opts, lim,
                  findings, row, reference)
    findings = group_by_cause(findings)
    shown = {k: (v.label if isinstance(v, G.Glb) else v) for k, v in opts.items()}
    metrics = {"animations": len(rows), "body_scale": round(float(scale), 3), "units": lim["units"],
               "per_animation": rows}
    if reference is not None:
        metrics["baseline_lowest_y"] = round(reference, 4)
    return R.make_report(TOOL, clip.label, findings, metrics, shown,
                         G.sha256(clip.path) if clip.path else None)


def _configure(parser):
    parser.add_argument("clips", nargs="+", help="GLB files with animations")
    parser.add_argument("--model", help="skinned model GLB the clips play on (default: the clip file itself)")
    parser.add_argument("--animations", nargs="+", help="only these animation names")
    parser.add_argument("--fps", type=float, help="sampling rate (default 30; 0 samples the key times)")
    parser.add_argument("--feet", nargs="+", help="foot joint names (default: detected)")
    parser.add_argument("--floor-y", type=float)
    parser.add_argument("--pop-accel", type=float, help="m/s^2 (default 180)")
    parser.add_argument("--pop-ratio", type=float, help="times the local median (default 4)")
    parser.add_argument("--teleport-speed", type=float, help="root speed in m/s (default 18)")
    parser.add_argument("--slide", type=float, help="planted drift in metres (default 0.05)")
    parser.add_argument("--sink", type=float, help="metres below the floor (default 0.03)")
    parser.add_argument("--loop", action="store_true", default=None, help="compare the first and last pose")
    parser.add_argument("--loop-seam", type=float)
    parser.add_argument("--in-place", choices=("auto", "yes", "no"),
                        help="judge foot slide against the treadmill an in-place clip implies (default auto)")
    parser.add_argument("--body-scale", type=_scale, help="threshold multiplier, or auto: model height / 1.75 m")
    parser.add_argument("--no-skinned-soles", dest="skinned_soles", action="store_false", default=None,
                        help="use foot joints instead of skinned vertices for the floor and slide tests")
    parser.add_argument("--plant-height", type=float,
                        help="a sole point within this of its lowest height may be planted (default 0.01 m)")
    parser.add_argument("--model-scale", type=float,
                        help="display scale the game applies to the model: lengths become game metres")
    parser.add_argument("--baseline", help="animation (e.g. Idle) whose lowest point sinks are measured against")
    parser.add_argument("--airborne", nargs="+", metavar="GLOB",
                        help="animation names held/carried/thrown by another actor: no slide or floor findings")


def _scale(text):
    return text if text == "auto" else float(text)


def _run(args, config):
    opts = R.resolve_options(TOOL, DEFAULTS, config, R.cli_overrides(args, DEFAULTS))
    if opts["model"] is not None:
        # One model for every clip keeps its decoded skin and pruning boxes across files.
        try:
            opts["model"] = G.Glb.load(opts["model"])
        except Exception:  # noqa: BLE001 - check() reports the unreadable model per clip
            pass
    return [check(path, **opts) for path in args.clips]


main = R.cli(__doc__, _configure, _run)

if __name__ == "__main__":
    sys.exit(main())
