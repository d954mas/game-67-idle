"""Fit tapered capsules (round cones) to arbitrary skinned character body meshes."""
from __future__ import annotations

import numpy as np


def fit_tapered_capsule(points, a, b, *, percentile=98.0, margin_m=0.002, min_radius_m=0.01, bins=6):
    """Fit (radius_a, radius_b) covering `points` along segment a->b with minimal bulge."""
    pts = np.asarray(points, dtype=np.float64)
    a = np.asarray(a, dtype=np.float64)
    b = np.asarray(b, dtype=np.float64)
    if pts.ndim != 2 or pts.shape[1] != 3 or len(pts) == 0:
        raise ValueError("fit_tapered_capsule requires non-empty (N, 3) points")
    if a.shape != (3,) or b.shape != (3,) or not np.isfinite([a, b]).all():
        raise ValueError("capsule endpoints must be finite 3D positions")
    if not 0 <= percentile <= 100 or not np.isfinite([margin_m, min_radius_m]).all() or min_radius_m <= 0:
        raise ValueError("invalid capsule percentile, margin or minimum radius")
    axis = b - a
    len2 = float(axis @ axis)
    if len2 <= 1e-12:
        r = float(np.percentile(np.linalg.norm(pts - a, axis=1), percentile)) + margin_m
        r = max(r, min_radius_m)
        return r, r

    length = float(np.sqrt(len2))
    t = np.clip(((pts - a) @ axis) / len2, 0.0, 1.0)
    closest = a + t[:, None] * axis
    radial = np.linalg.norm(pts - closest, axis=1)

    # Measure upper-envelope radius in axial slices so a linear taper hugs the limb cleanly.
    ts, rs = [], []
    edges = np.linspace(0.0, 1.0, bins + 1)
    for lo, hi in zip(edges[:-1], edges[1:]):
        mask = (t >= lo) & (t <= hi if hi == 1.0 else t < hi)
        if np.count_nonzero(mask) < 3:
            continue
        ts.append(0.5 * (lo + hi))
        rs.append(float(np.percentile(radial[mask], percentile)) + margin_m)

    if len(ts) < 2:
        r = max(float(np.percentile(radial, percentile)) + margin_m, min_radius_m)
        return r, r

    ts_arr = np.asarray(ts, dtype=np.float64)
    rs_arr = np.asarray(rs, dtype=np.float64)
    slope, intercept = np.polyfit(ts_arr, rs_arr, 1)
    # Shift line upward so no sampled slice falls below its measured envelope.
    residual = rs_arr - (intercept + slope * ts_arr)
    lift = float(max(0.0, residual.max()))
    ra = max(float(intercept + lift), min_radius_m)
    rb = max(float(intercept + slope + lift), min_radius_m)
    # Keep taper well inside non-degenerate cone bounds (|ra - rb| < 0.85 * length).
    max_dr = 0.85 * length
    if abs(ra - rb) > max_dr:
        mid = 0.5 * (ra + rb)
        sign = 1.0 if ra > rb else -1.0
        ra = max(mid + 0.5 * sign * max_dr, min_radius_m)
        rb = max(mid - 0.5 * sign * max_dr, min_radius_m)
    return round(ra, 5), round(rb, 5)


def fit_body_capsules(points, joints, body, shape, overrides=None):
    """Fit ordered [bone_a, a, bone_b, b, radius_a, radius_b] rows in rest-model space."""
    points = np.asarray(points, dtype=float)
    joints = np.asarray(joints)
    if points.shape != (len(joints), 3) or not np.isfinite(points).all():
        raise ValueError("expected finite body points and one joint name per point")
    if shape not in body["shapes"]:
        raise ValueError(f"unknown body fit shape {shape!r}")
    profile = body["shapes"][shape]
    overrides = {} if overrides is None else overrides
    if not isinstance(overrides, dict):
        raise ValueError("fit_overrides must map segment groups to settings")
    rows, settings, shared_radii = [], [], {}
    used = set()
    for name in profile["segments"]:
        segment = body["segments"][name]
        group = segment.get("group")
        options = overrides.get(group, {})
        used.add(group)
        if not isinstance(options, dict) or options.keys() - {"max_radius_m", "start_y_m", "start_z_offset_m"}:
            raise ValueError(f"{name}: unknown fit override")
        joint_a, a = segment["a"]
        joint_b, b = segment["b"]
        mask = np.isin(joints, segment["sample_joints"])
        if "bounds_m" in segment:
            lower, upper = segment["bounds_m"]
            for axis in range(3):
                if lower[axis] is not None:
                    mask &= points[:, axis] >= lower[axis]
                if upper[axis] is not None:
                    mask &= points[:, axis] <= upper[axis]
        samples = points[mask]
        if len(samples) < 6:
            raise ValueError(f"{name}: too few body vertices ({len(samples)}) for {segment['sample_joints']}")
        low = body["min_radius_m"]
        high = options.get("max_radius_m", segment["max_radius_m"])
        if not np.isfinite([low, high]).all() or not 0 < low <= high:
            raise ValueError(f"{name}: invalid radius range")
        ra, rb = fit_tapered_capsule(samples, a, b, min_radius_m=low,
                                     percentile=segment.get("percentile", profile["percentile"]),
                                     margin_m=segment.get("margin_m", profile["margin_m"]))
        ra, rb = round(min(high, ra), 4), round(min(high, rb), 4)
        if segment.get("uniform_radius", False):
            ra = rb = max(ra, rb)
        radius_group = segment.get("radius_group")
        if radius_group is not None:
            other_a, other_b = shared_radii.get(radius_group, (0., 0.))
            shared_radii[radius_group] = max(ra, other_a), max(rb, other_b)
        rows.append([joint_a, list(a), joint_b, list(b), ra, rb])
        settings.append((radius_group, options))
    if overrides.keys() - used:
        raise ValueError(f"unknown fit group(s) for {shape}: {sorted(overrides.keys() - used)}")
    for row, (radius_group, options) in zip(rows, settings):
        _, a, _, b, ra, rb = row
        ra, rb = shared_radii.get(radius_group, (ra, rb))
        if "start_y_m" in options:
            y = options["start_y_m"]
            if a[1] == b[1]:
                raise ValueError("start_y_m requires a non-horizontal capsule")
            t = (y - a[1]) / (b[1] - a[1])
            if not 0 <= t < 1:
                raise ValueError("start_y_m must lie on the capsule axis before the endpoint")
            a[:] = [round(v + t * (w - v), 6) for v, w in zip(a, b)]
            ra = round(ra + t * (rb - ra), 6)
        if "start_z_offset_m" in options:
            offset = options["start_z_offset_m"]
            if not np.isfinite(offset):
                raise ValueError("start_z_offset_m must be finite")
            a[2] = round(a[2] + offset, 6)
        row[4:] = [ra, rb]
    return rows
