"""Blender-free math for Kimodo retargeting: foot-contact detection and two-bone IK.

Positions are (x, y, z) tuples in metres, Z up. Kept free of bpy/mathutils so the
contact and IK rules are unit-testable outside Blender.
"""
import math


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def _scale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _length(a):
    return math.sqrt(_dot(a, a))


def _normalized(a):
    n = _length(a)
    return _scale(a, 1.0 / n) if n > 1e-9 else (0.0, 0.0, 0.0)


def _horizontal_distance(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def detect_contacts(ankles, fps, leg_length, height_frac=0.12, speed_frac=0.8, min_run=3, max_gap=2):
    """Per-frame planted flags for one foot.

    A frame is planted when the ankle is within height_frac*leg_length of its lowest
    point in the clip and moves horizontally slower than speed_frac*leg_length per
    second. Gaps up to max_gap frames inside a plant are filled, then plants shorter
    than min_run frames are dropped, so single-frame flicker never pins a foot.
    """
    count = len(ankles)
    if count == 0:
        return []
    floor = min(p[2] for p in ankles)
    height_limit = height_frac * leg_length
    speed_limit = speed_frac * leg_length
    flags = []
    for i in range(count):
        prev_p = ankles[max(i - 1, 0)]
        next_p = ankles[min(i + 1, count - 1)]
        span = (min(i + 1, count - 1) - max(i - 1, 0)) / fps
        speed = _horizontal_distance(next_p, prev_p) / span if span > 0 else 0.0
        flags.append(ankles[i][2] - floor <= height_limit and speed <= speed_limit)

    runs = contact_runs(flags)
    for (_, end_a), (start_b, _) in zip(runs, runs[1:]):
        if start_b - end_a - 1 <= max_gap:
            for i in range(end_a + 1, start_b):
                flags[i] = True
    for start, end in contact_runs(flags):
        if end - start + 1 < min_run:
            for i in range(start, end + 1):
                flags[i] = False
    return flags


def contact_runs(flags):
    """Inclusive (start, end) index pairs of consecutive True flags."""
    runs = []
    start = None
    for i, flag in enumerate(flags):
        if flag and start is None:
            start = i
        elif not flag and start is not None:
            runs.append((start, i - 1))
            start = None
    if start is not None:
        runs.append((start, len(flags) - 1))
    return runs


def lock_plan(ankles, flags, ramp=3):
    """Per-frame (weight, anchor) pairs; anchor is a horizontal (x, y) or None.

    Planted frames get weight 1 and the plant's mean horizontal ankle position, so
    the whole plant is pinned; height stays free so the heel can still roll. The
    blend in and out happens over up to `ramp` swing frames outside the plant and
    never crosses into a neighbouring plant.
    """
    count = len(ankles)
    plan = [(0.0, None)] * count
    for start, end in contact_runs(flags):
        n = end - start + 1
        anchor = (sum(ankles[i][0] for i in range(start, end + 1)) / n,
                  sum(ankles[i][1] for i in range(start, end + 1)) / n)
        for i in range(start, end + 1):
            plan[i] = (1.0, anchor)
        for k in range(1, ramp + 1):
            weight = 1.0 - k / float(ramp + 1)
            for i in (start - k, end + k):
                if 0 <= i < count and not flags[i] and weight > plan[i][0]:
                    plan[i] = (weight, anchor)
    return plan


def foot_slide(ankles, flags):
    """Total horizontal ankle travel between consecutive planted frames, in metres."""
    total = 0.0
    for i in range(1, len(ankles)):
        if flags[i] and flags[i - 1]:
            total += _horizontal_distance(ankles[i], ankles[i - 1])
    return total


def remove_twist(q):
    """Swing part of a bone-local (w, x, y, z) rotation, dropping twist about the bone's Y axis.

    Blender bones point along local Y, so for a hinge such as a knee the swing is
    the bend and the twist is spin the joint cannot make.
    """
    w, x, y, z = q
    n = math.hypot(w, y)
    if n < 1e-9:
        return q  # a 180-degree swing: the twist is undefined, keep the rotation
    tw, ty = w / n, y / n
    # swing = q * conjugate(twist), twist = (tw, 0, ty, 0)
    return (w * tw + y * ty, x * tw + z * ty, y * tw - w * ty, z * tw - x * ty)


def two_bone_ik(hip, knee_hint, target, upper_length, lower_length):
    """Knee and reached ankle positions for a hip-knee-ankle chain aimed at `target`.

    `knee_hint` (usually the FK knee) picks the bend plane. An unreachable target is
    clamped just short of full extension so the knee never snaps straight.
    """
    to_target = _sub(target, hip)
    distance = _length(to_target)
    reach = (upper_length + lower_length) * 0.999
    floor = abs(upper_length - lower_length) * 1.001 + 1e-6
    d = min(max(distance, floor), reach)
    axis = _normalized(to_target) if distance > 1e-9 else _normalized(_sub(knee_hint, hip))
    ankle = _add(hip, _scale(axis, d))
    along = (upper_length * upper_length - lower_length * lower_length + d * d) / (2.0 * d)
    height = math.sqrt(max(upper_length * upper_length - along * along, 0.0))
    hint = _sub(knee_hint, hip)
    bend = _sub(hint, _scale(axis, _dot(hint, axis)))
    if _length(bend) < 1e-9:
        bend = (0.0, 0.0, 1.0) if abs(axis[2]) < 0.9 else (1.0, 0.0, 0.0)
        bend = _sub(bend, _scale(axis, _dot(bend, axis)))
    bend = _normalized(bend)
    knee = _add(_add(hip, _scale(axis, along)), _scale(bend, height))
    return knee, ankle
