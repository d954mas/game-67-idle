#!/usr/bin/env python3
from __future__ import annotations

import sys
import unittest
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[5]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.image.route.route_cutout import (
    DEPTH90_THRESHOLD,
    SCORE_THRESHOLD,
    route_cutout,
    soft_metrics,
)

GREEN = (0, 255, 0)


def _hard_opaque(size: int = 200, key=GREEN, fg=(245, 245, 250)) -> Image.Image:
    """Solid opaque square on a flat key -> thin 1px edge, no soft zone."""
    arr = np.empty((size, size, 3), dtype=np.float64)
    arr[:] = key
    arr[50:150, 50:150] = fg
    return Image.fromarray(arr.astype(np.uint8), "RGB")


def _soft_glow(size: int = 200, key=GREEN, glow=(255, 150, 0), radius: float = 85.0) -> Image.Image:
    """Radial glow fading into the key over a wide band -> large soft zone."""
    yy, xx = np.mgrid[0:size, 0:size]
    center = size / 2.0
    r = np.sqrt((xx - center) ** 2 + (yy - center) ** 2)
    t = np.clip(1.0 - r / radius, 0.0, 1.0)[..., None]
    img = t * np.array(glow, dtype=np.float64) + (1.0 - t) * np.array(key, dtype=np.float64)
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGB")


def _all_key(size: int = 64, key=GREEN) -> Image.Image:
    arr = np.empty((size, size, 3), dtype=np.float64)
    arr[:] = key
    return Image.fromarray(arr.astype(np.uint8), "RGB")


class RouteCutoutTest(unittest.TestCase):
    def test_opaque_art_routes_to_key_matte(self):
        decision = route_cutout(_hard_opaque(), GREEN)
        self.assertEqual(decision.method, "key_matte")
        self.assertFalse(decision.needs_dual)
        self.assertLess(decision.soft_score, SCORE_THRESHOLD)
        self.assertLess(decision.depth90, DEPTH90_THRESHOLD)

    def test_soft_glow_routes_to_dual_plate(self):
        decision = route_cutout(_soft_glow(), GREEN)
        self.assertEqual(decision.method, "dual_plate")
        self.assertTrue(decision.needs_dual)
        self.assertGreaterEqual(decision.soft_score, SCORE_THRESHOLD)

    def test_key_auto_detected_from_border(self):
        # No key passed -> detected from the green border; routing unchanged.
        self.assertEqual(route_cutout(_hard_opaque()).method, "key_matte")
        self.assertEqual(route_cutout(_soft_glow()).method, "dual_plate")

    def test_empty_all_key_crop_is_opaque_default(self):
        decision = route_cutout(_all_key(), GREEN)
        self.assertEqual(decision.method, "key_matte")
        self.assertEqual(decision.n_core, 0)
        self.assertEqual(decision.n_partial, 0)

    def test_separation_is_stable_across_tolerance(self):
        hard, soft = _hard_opaque(), _soft_glow()
        for tol in (64.0, 80.0, 100.0):
            hm = soft_metrics(hard, GREEN, tolerance=tol)
            sm = soft_metrics(soft, GREEN, tolerance=tol)
            self.assertLess(hm["soft_score"], sm["soft_score"], f"tolerance={tol} must keep hard<soft")
            self.assertLess(hm["soft_score"], SCORE_THRESHOLD, f"tolerance={tol} hard must stay opaque")

    def test_metrics_shape(self):
        m = soft_metrics(_soft_glow(), GREEN)
        for field in ("soft_score", "depth90", "depth_median", "n_core", "n_partial", "key"):
            self.assertIn(field, m)
        self.assertEqual(m["key"], GREEN)


if __name__ == "__main__":
    unittest.main()
