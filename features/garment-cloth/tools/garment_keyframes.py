"""Sample authored garment poses and signed normal offset maps on integer ticks."""
from math import isfinite
from numbers import Real

import numpy as np


def vertex_map(raw, count):
    values = np.asarray(raw, dtype=float)
    if values.ndim == 0:
        values = np.full(count, values)
    if values.shape != (count,) or not np.isfinite(values).all() or np.any(values < 0):
        raise ValueError("shape.travel_m must be a nonnegative radius or a radius per proxy vertex")
    return values


def authored_track(keys, count, points, faces, load_obj, directory):
    ticks, targets = [], []
    for key in keys:
        tick = key["tick"]
        if type(tick) is not int or not 0 <= tick < count or (ticks and ticks[-1] >= tick):
            raise ValueError("shape key ticks must be ordered within the clip")
        if ("obj" in key) == ("vertices_m" in key):
            raise ValueError("a shape key takes either obj or vertices_m")
        if "obj" in key:
            vertices, topology = load_obj(directory / key["obj"])
            if not np.array_equal(topology, faces):
                raise ValueError("shape OBJ must have identical ordered proxy topology")
        else:
            vertices = key["vertices_m"]
        target = np.asarray(vertices, dtype=float)
        if target.shape != points.shape or not np.isfinite(target).all():
            raise ValueError("shape key must match proxy vertices in character space")
        ticks.append(tick)
        targets.append(target)
    if not ticks or ticks[0] != 0 or ticks[-1] != count-1:
        raise ValueError(f"shape keys must cover clip tick 0 through {count-1}")
    frames = []
    for a, b, start, end in zip(ticks, ticks[1:], targets, targets[1:]):
        frames.extend(start + (end-start) * (i/(b-a)) for i in range(b-a))
    return np.asarray(frames + [targets[-1]])


def _finite_real(value, error):
    if isinstance(value, (bool, np.bool_)) or not isinstance(value, Real):
        raise ValueError(error)
    try:
        number = float(value)
    except OverflowError as exc:
        raise ValueError(error) from exc
    if not isfinite(number):
        raise ValueError(error)
    return number


def normal_weight_map(patch, points):
    """Cook one painted or smoothly tapered ellipsoidal mask on the rest proxy."""
    points = np.asarray(points, dtype=float)
    if points.ndim != 2 or points.shape[1] != 3 or not np.isfinite(points).all():
        raise ValueError("expected finite rest positions in meters")
    if "weights" in patch:
        weight = vertex_map(patch["weights"], len(points))
        if np.any(weight > 1) or "center_m" in patch or "radii_m" in patch:
            raise ValueError("specify either painted weights in [0,1] or a region")
        return weight
    if "center_m" not in patch or "radii_m" not in patch:
        raise ValueError("a patch needs painted weights or an ellipsoid center and radii")
    center, radii = (np.asarray(patch[k], dtype=float) for k in ("center_m", "radii_m"))
    if (center.shape != (3,) or radii.shape != (3,) or not np.isfinite([center, radii]).all()
            or np.any(radii <= 0)):
        raise ValueError("invalid patch center or radii")
    w = np.clip(1 - np.linalg.norm((points-center)/radii, axis=1), 0, 1)
    return w*w*(3-2*w)


def normal_offsets(patches, points, count):
    offsets = np.zeros((count, len(points)))
    if not isinstance(patches, list) or not patches:
        raise ValueError("normal_offsets requires a list of patches")
    for patch in patches:
        weight = normal_weight_map(patch, points)
        keys = patch["keys"]
        if (len(keys) < 2 or any(len(k) != 2 or type(k[0]) is not int or not 0 <= k[0] < count
                for k in keys) or any(a[0] >= b[0] for a,b in zip(keys,keys[1:]))):
            raise ValueError("invalid patch activation curve")
        keys = [(tick, _finite_real(value, "invalid patch activation curve")) for tick, value in keys]
        if (any(not 0 <= value <= 1 for _, value in keys) or (keys[0][0] > 0 and keys[0][1] != 0)
                or (keys[-1][0] < count-1 and keys[-1][1] != 0)):
            raise ValueError("invalid patch activation curve")
        amplitude = _finite_real(patch["offset_m"], "offset_m must be a finite real scalar")
        curve = np.zeros(count)
        for (a, va), (b, vb) in zip(keys,keys[1:]):
            u = np.linspace(0,1,b-a+1)
            curve[a:b+1] = va + (vb-va)*u*u*(3-2*u)
        offsets += amplitude * curve[:,None] * weight
    return offsets
