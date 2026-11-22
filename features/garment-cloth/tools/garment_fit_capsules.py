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
