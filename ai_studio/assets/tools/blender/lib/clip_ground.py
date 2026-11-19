"""Lift clips whose body sinks through the floor, numpy only (plain Python or inside Blender).

Per key of the clip's root translation channel, the lowest skinned point of the body is measured;
where it lies more than `tolerance` below the floor, the root is raised by exactly the sink. The lift
is spread to neighbouring keys with a cosine falloff (never below what a key needs), so nothing pops,
and it is only ever upward: a clip is never lowered and keys already on the floor keep their values
outside those ramps. The floor is a height, or the median lowest point of a baseline clip (an idle),
which is the body's real resting contact when its soles sit a little under zero.

Only the root translation accessor changes. When no other sampler reads it, its bytes are rewritten
in place, so every other clip in the file stays byte-identical.
"""
import copy

import numpy as np

from ai_studio.assets.tools.blender.lib import glb as G


class GroundError(ValueError):
    pass


def skinned_nodes(model):
    return [i for i, n in enumerate(model.nodes) if "skin" in n and "mesh" in n]


def lowest(model, channels, times):
    """Lowest skinned-vertex height of `model` posed by `channels` at each time; joint heights when the
    model has no skinned mesh."""
    worlds = model.posed_worlds(channels, times)
    nodes = skinned_nodes(model)
    low = model.lowest_skinned_y(worlds, nodes) if nodes else None
    if low is None:
        joints = sorted({j for s in model.json.get("skins", []) for j in s["joints"]}) or list(worlds)
        low = np.min(np.stack([worlds[j][:, 1, 3] for j in joints], 1), axis=1)
    return np.asarray(low, float), worlds


def _channels_for(clip, model, anim):
    return clip.channels(anim, by_index=clip is model)


def root_channel(clip, anim):
    """(channel, node) of the animated translation nearest the scene root that has children: the node
    that moves the whole body."""
    a = clip.json["animations"][clip.animation_index(anim)]
    best = None
    for c in a["channels"]:
        node = c["target"].get("node")
        if node is None or c["target"]["path"] != "translation" or not clip.nodes[node].get("children"):
            continue
        depth, i = 0, node
        while i in clip.parent:
            i, depth = clip.parent[i], depth + 1
        if best is None or depth < best[0]:
            best = (depth, c, node)
    if best is None:
        raise GroundError(f"{anim}: no animated root translation to lift")
    return best[1], best[2]


def floor_of(clip, model, baseline, fps=30.0):
    """Median lowest point of the baseline animation: the body's resting floor contact."""
    duration = clip.duration(baseline)
    times = np.linspace(0.0, duration, max(2, int(round(duration * fps)) + 1))
    low, _ = lowest(model, _channels_for(clip, model, baseline), times)
    return float(np.median(low))


def lift_profile(sink, ramp, loop=False):
    """Per-key lift: each key's sink spread to `ramp` keys either side with a cosine falloff, taking
    the largest demand, so a key never gets less than its own sink."""
    n = len(sink)
    out = sink.copy()
    for d in range(1, ramp + 1):
        w = 0.5 * (1.0 + np.cos(np.pi * d / (ramp + 1)))
        for shift in (d, -d):
            if loop:
                moved = np.roll(sink, shift)
            else:
                moved = np.zeros(n)
                if shift > 0:
                    moved[shift:] = sink[:-shift]
                else:
                    moved[:shift] = sink[-shift:]
            out = np.maximum(out, w * moved)
    return out


def ground(clip, model=None, animations=None, floor=0.0, baseline=None, tolerance=0.01, ramp=3, loop=False,
           fps=30.0, baseline_source=None):
    """(document, binary, report) with the named animations lifted out of the floor.

    clip: Glb with the animations; model: skinned body Glb they play on (default: the clip itself,
    channels then matched by node index, else by node name). baseline_source: the Glb holding the
    baseline animation when the game plays its idle from another file (default: clip).
    """
    model = model or clip
    floor_y = floor_of(baseline_source or clip, model, baseline, fps) if baseline else float(floor)
    document = copy.deepcopy(clip.json)
    binary = bytearray(clip.bin)
    names = animations or clip.animation_names()
    report = {"floor_y": round(floor_y, 4), "baseline": baseline, "tolerance": tolerance, "ramp_keys": ramp,
              "clips": {}}
    appended = None
    for name in names:
        anim = clip.animation_index(name)
        channel, node = root_channel(clip, anim)
        sampler = document["animations"][anim]["samplers"][channel["sampler"]]
        keys = clip.accessor(sampler["input"])[:, 0]
        values = clip.accessor(sampler["output"])
        cubic = sampler.get("interpolation", "LINEAR") == "CUBICSPLINE"
        points = values.reshape(len(keys), 3, 3)[:, 1] if cubic else values
        channels = _channels_for(clip, model, anim)
        low, worlds = lowest(model, channels, keys)
        sink = np.where(floor_y - low > tolerance, floor_y - low, 0.0)
        lift = lift_profile(sink, ramp, loop)
        dense = np.linspace(0.0, clip.duration(anim), max(2, int(round(clip.duration(anim) * fps)) + 1))
        before = float(floor_y - lowest(model, channels, dense)[0].min())
        row = {"root": clip.name(node), "keys": len(keys), "max_sink_before": round(max(before, 0.0), 4),
               "frames_touched": int((lift > 1e-6).sum()), "max_lift": round(float(lift.max()), 4)}
        if lift.max() > 0:
            # The model's node of the same name carries the lift; its parent frame turns world up into local.
            target = node if model is clip else model.by_name.get(clip.name(node))
            parent = model.parent.get(target) if target is not None else None
            if parent is None:
                up_local = np.repeat([[0.0, 1.0, 0.0]], len(keys), 0)
            else:
                up = np.broadcast_to([0.0, 1.0, 0.0], (len(keys), 3))[..., None]
                up_local = np.linalg.solve(worlds[parent][:, :3, :3], up)[..., 0]
            new_points = points + lift[:, None] * up_local
            if cubic:
                new_values = values.reshape(len(keys), 3, 3).copy()
                new_values[:, 1] = new_points
                new_values = new_values.reshape(-1, 3)
            else:
                new_values = new_points
            appended = _store(document, binary, sampler, new_values, appended)
        report["clips"][name] = row
    if appended is not None:
        document, binary = appended.json, appended.payload
    fixed = G.Glb(copy.deepcopy(document), bytes(binary))
    body = fixed if model is clip else model
    for name in names:
        anim = fixed.animation_index(name)
        dense = np.linspace(0.0, fixed.duration(anim), max(2, int(round(fixed.duration(anim) * fps)) + 1))
        low, _ = lowest(body, _channels_for(fixed, body, anim), dense)
        report["clips"][name]["max_sink_after"] = round(max(float(floor_y - low.min()), 0.0), 4)
    return document, bytes(binary), report


def _exclusive(document, accessor):
    users = sum(s["output"] == accessor or s["input"] == accessor
                for a in document.get("animations", []) for s in a["samplers"])
    acc = document["accessors"][accessor]
    view = acc.get("bufferView")
    shared_view = sum(a.get("bufferView") == view for a in document["accessors"]) > 1
    plain = (acc["componentType"] == G.FLOAT and "sparse" not in acc and view is not None
             and document["bufferViews"][view].get("byteStride", 12) == 12
             and document["bufferViews"][view].get("buffer", 0) == 0)
    return users == 1 and not shared_view and plain


def _store(document, binary, sampler, values, builder):
    """Rewrite the sampler's output in place when it owns its bytes, else append a new accessor."""
    values = np.asarray(values, "<f4")
    index = sampler["output"]
    if builder is None and _exclusive(document, index):
        acc = document["accessors"][index]
        view = document["bufferViews"][acc["bufferView"]]
        start = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
        data = values.tobytes()
        if len(data) != acc["count"] * 12:
            raise GroundError("root translation accessor size changed")
        binary[start:start + len(data)] = data
        if "min" in acc:
            acc["min"], acc["max"] = values.min(0).tolist(), values.max(0).tolist()
        return None
    if builder is None:
        builder = G.GlbBuilder(document, bytes(binary))  # shares `document`, so `sampler` is its own
    sampler["output"] = builder.add_accessor(values.astype(float))
    return builder
