"""Animation check of a skinned GLB, or of a clip GLB applied to a model GLB: broken keys, pops,
teleports, foot slide on planted feet, soles through the floor, and optionally the loop seam.

    python clip_check.py character.glb [--animations Walk Idle] [--fps 30] [--feet LeftFoot RightFoot]
    python clip_check.py clips.glb --model character.glb [--loop]

Thresholds are physical (m/s, m/s^2), so the sampling rate does not move them, and are scaled by
the model's height against a 1.75 m human (--body-scale 1 keeps them absolute). Foot joints come from
--feet, else common names (mixamorig:LeftFoot, foot_l, LeftFoot, *Foot*) are detected. Clips with root
motion are judged in world space; an in-place locomotion clip shows its stride as slide.
"""
import re
import sys
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
            "skinned_soles": True, "in_place": "auto", "body_scale": "auto"}
SCALED = ("pop_accel", "teleport_speed", "slide", "sink", "float", "loop_seam")
HUMAN_HEIGHT = 1.75
SAMPLES = 5
SIDE = {"left": ("left", "l"), "right": ("right", "r")}


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
    """Second derivative per sample of (frames, 3) positions on a possibly uneven time grid, m/s^2."""
    out = np.zeros_like(p)
    if len(p) < 3:
        return out
    h0, h1 = np.diff(times)[:-1], np.diff(times)[1:]
    v0 = (p[1:-1] - p[:-2]) / h0[:, None]
    v1 = (p[2:] - p[1:-1]) / h1[:, None]
    out[1:-1] = 2 * (v1 - v0) / (h0 + h1)[:, None]
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


def check_pops(names, positions, times, opts, anim, out, row, leaves=()):
    """Second-difference spikes on joints that carry other joints: a chain's tip (finger, toe end)
    multiplies every rotation above it into speed, so a fast swing reads as a pop there."""
    worst, hits = 0.0, []
    floor = 0.36  # m/s^2: 1e-4 m/frame^2 at 60 fps keeps a perfectly smooth stretch from dividing by zero
    w = opts["pop_window"]
    for j, joint in enumerate(names):
        if joint in leaves:
            continue
        vec = _accel(positions[:, j], times)
        acc = np.linalg.norm(vec, axis=1)
        worst = max(worst, float(acc.max(initial=0)))
        for k in np.flatnonzero(acc > opts["pop_accel"]):
            local = float(np.median(acc[max(1, k - w):k + w + 1]))
            # A pop goes out and comes back: the sample strays from the chord of its neighbours by more
            # than the chord is long. A fast swing or a strike travels through, however hard it accelerates.
            p = positions[:, j]
            out_and_back = np.linalg.norm(p[k] - (p[k - 1] + p[k + 1]) / 2) > np.linalg.norm(p[k + 1] - p[k - 1])
            if acc[k] > opts["pop_ratio"] * max(local, floor) and out_and_back:
                hits.append((float(acc[k]), joint, float(times[k])))
    row["max_accel_mps2"] = round(worst, 1)
    if hits:
        hits.sort(reverse=True)
        joints = sorted({h[1] for h in hits})
        out.append(R.finding("pop", "medium", f"{anim}: {len(hits)} pops on {len(joints)} joints "
                             f"(worst {hits[0][0]:.0f} m/s^2 on {hits[0][1]} at {hits[0][2]:.3f} s)",
                             "smooth or re-key the spike (a flipped quaternion or a stray key)",
                             {"animation": anim, "pops": [{"joint": j, "time": round(t, 3), "accel": round(a)}
                                                          for a, j, t in hits[:SAMPLES]]},
                             round(hits[0][0], 1), opts["pop_accel"]))


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


def check_feet(model, feet, names, positions, times, opts, anim, out, row):
    """Planted-foot drift. An in-place clip (feet end where they started) is judged in the frame of
    the treadmill it implies: the median velocity of the feet while they are low."""
    if len(times) < 4 or not feet:
        return
    t = times - times[0]
    fps = (len(times) - 1) / max(t[-1], 1e-9)
    tracks = {f: np.array([motion.from_gltf(p) for p in positions[:, names.index(f)]]) for f in feet}
    legs = {f: leg_length(model, f) for f in feet}
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
    for foot, tr in tracks.items():
        ankles = [tuple(p) for p in tr]
        flags = motion.detect_contacts(ankles, fps, legs[foot])
        drift, at = 0.0, None
        for start, end in motion.contact_runs(flags):
            d = np.linalg.norm(tr[start:end + 1, :2] - tr[start, :2], axis=1)
            if d.max() > drift:
                drift, at = float(d.max()), float(times[start])
        row["feet"][foot] = {"planted_frames": int(sum(flags)), "max_drift_m": round(drift, 4),
                             "slide_total_m": round(motion.foot_slide(ankles, flags), 4)}
        if drift > opts["slide"]:
            out.append(R.finding("foot_slide", "medium", f"{anim}: planted {foot} slides {drift:.3f} m "
                                 f"(plant from {at:.3f} s)", "lock the foot during the plant (foot IK / "
                                 "foot lock), or match root speed to the stride",
                                 {"animation": anim, "joint": foot, "time": round(at, 3)},
                                 round(drift, 4), round(opts["slide"], 4)))


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


def lowest_points(model, world, use_skin):
    nodes = [i for i, n in enumerate(model.nodes) if "skin" in n and "mesh" in n] if use_skin else []
    if nodes:
        return min(float(np.min(p[:, 1])) for i in nodes for p in model.skinned_positions(world, i) if len(p))
    return None


def check_soles(model, feet, names, positions, worlds, times, opts, anim, out, row):
    lows = [lowest_points(model, w, opts["skinned_soles"]) for w in worlds]
    source = "skinned mesh"
    if any(v is None for v in lows):
        if not feet:
            return
        source = "foot joints"
        lows = positions[:, [names.index(f) for f in feet], 1].min(axis=1)
    lows = np.asarray(lows, float)
    k = int(np.argmin(lows))
    row.update(lowest_y=round(float(lows[k]), 4), lowest_source=source)
    depth = opts["floor_y"] - lows[k]
    if depth > opts["sink"]:
        out.append(R.finding("sole_sink", "medium", f"{anim}: {source} goes {depth:.3f} m below the floor at "
                             f"{times[k]:.3f} s", "raise the root/hips or fix the foot keys so soles stay on "
                             "the floor", {"animation": anim, "time": round(float(times[k]), 3)},
                             round(float(depth), 4), opts["sink"]))
    if source == "skinned mesh" and lows[k] - opts["floor_y"] > opts["float"]:
        out.append(R.finding("sole_float", "low", f"{anim}: the mesh never comes within {opts['float']} m of the "
                             f"floor (lowest {lows[k] - opts['floor_y']:+.3f} m)", "lower the root, unless the "
                             "clip is airborne by design", {"animation": anim}, round(float(lows[k]), 4)))


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


def check_animation(clip, model, anim, joint_ids, opts, lim, findings, row):
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
    worlds = [model.posed_world(channels, t) for t in times]
    missing = [model.name(i) for i in ids if i not in worlds[0]]
    if missing:
        findings.append(R.finding("joints_in_scene", "high", f"{anim}: {len(missing)} joints are not in the scene; "
                                  "they are left out", "export the whole armature", {"animation": anim,
                                                                                     "joints": missing[:SAMPLES]}))
        ids = [i for i in ids if i in worlds[0]]
    names = [model.name(i) for i in ids]
    roots = {model.name(i) for i in ids if model.parent.get(i) not in ids}
    positions = np.array([[w[i][:3, 3] for i in ids] for w in worlds]).reshape(len(times), len(ids), 3)
    row["samples"] = len(times)
    if not len(ids):
        return
    feet = opts["feet"] or detect_feet(names)
    feet = [f for f in feet if f in names]
    row["foot_joints"] = feet
    leaves = {model.name(i) for i in ids if not any(c in ids for c in model.nodes[i].get("children", []))}
    check_pops(names, positions, times, lim, anim, findings, row, leaves)
    check_teleport(roots, names, positions, times, lim, anim, findings)
    check_feet(model, feet, names, positions, times, lim, anim, findings, row)
    check_soles(model, feet, names, positions, worlds, times, lim, anim, findings, row)
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
    scale = opts["body_scale"]
    if scale == "auto":
        height = body_height(model)
        scale = height / HUMAN_HEIGHT if height and height > 1e-3 else 1.0
    lim = {**opts, **{k: opts[k] * float(scale) for k in SCALED}}
    skins = model.json.get("skins", [])
    lim["skinned_soles"] = opts["skinned_soles"] and R.guarded(findings, "skin", _skin_usable, model, findings)
    joint_ids = sorted({j for s in skins for j in s["joints"] if 0 <= j < len(model.nodes)})
    anims = opts["animations"] or clip.animation_names()
    if not anims:
        findings.append(R.finding("animations", "info", "no animations in the file"))
    for anim in anims:
        row = {"animation": anim}
        rows.append(row)
        R.guarded(findings, f"animation {anim}", check_animation, clip, model, anim, joint_ids, opts, lim,
                  findings, row)
    findings = group_by_cause(findings)
    shown = {k: (v.label if isinstance(v, G.Glb) else v) for k, v in opts.items()}
    metrics = {"animations": len(rows), "body_scale": round(float(scale), 3), "per_animation": rows}
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
                        help="use foot joints instead of skinned vertices for the floor test")


def _scale(text):
    return text if text == "auto" else float(text)


def _run(args, config):
    opts = R.resolve_options(TOOL, DEFAULTS, config, R.cli_overrides(args, DEFAULTS))
    return [check(path, **opts) for path in args.clips]


main = R.cli(__doc__, _configure, _run)

if __name__ == "__main__":
    sys.exit(main())
