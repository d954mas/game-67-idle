#!/usr/bin/env python3
"""Offline unit tests for matte_math.py -- pure numpy/scipy/PIL, runnable by
the SHARED repo venv (no torch import anywhere in this file or matte_math.py).

    .venv/Scripts/python.exe -m unittest ai_studio.assets.tools.image.vitmatte_matte.matte_math_test
"""
from __future__ import annotations

import unittest

import numpy as np
from PIL import Image

from ai_studio.assets.tools.image.vitmatte_matte.matte_math import (
    build_auto_trimap,
    build_mask_seeded_trimap,
    despill,
    trimap_stats,
)

MAGENTA = (255, 0, 255)


def make_soft_disk_on_key(size: int = 200, key=MAGENTA, fg=(0, 0, 0), r_full: float = 40.0, r_empty: float = 90.0):
    """A filled disk on a flat key plate: coverage=1 inside r_full, coverage=0
    outside r_empty, linear ramp between. Because the composite is a straight
    line interpolation between fg and key, its Euclidean distance-to-key is
    exactly ``coverage * |fg - key|`` -- a controlled, predictable sweep
    through the whole bg/unknown/fg chroma-distance range for the tuned
    AUTO_T1=70 / AUTO_T2=150 thresholds."""
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    center = (size - 1) / 2.0
    radius = np.sqrt((xx - center) ** 2 + (yy - center) ** 2)
    coverage = np.clip((r_empty - radius) / (r_empty - r_full), 0.0, 1.0)
    key_arr = np.asarray(key, dtype=np.float64)
    fg_arr = np.asarray(fg, dtype=np.float64)
    composite = key_arr + coverage[..., None] * (fg_arr - key_arr)
    plate = Image.fromarray(np.rint(composite).astype(np.uint8), "RGB")
    return plate, coverage


def make_soft_mask(size: int = 200, r_full: float = 40.0, r_empty: float = 90.0) -> Image.Image:
    """A coarse neural-mask alpha channel: same radial ramp shape as
    ``make_soft_disk_on_key`` but expressed directly as an L-mode alpha image
    (0..255), for the mask-seeded trimap builder (no flat plate involved)."""
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    center = (size - 1) / 2.0
    radius = np.sqrt((xx - center) ** 2 + (yy - center) ** 2)
    coverage = np.clip((r_empty - radius) / (r_empty - r_full), 0.0, 1.0)
    return Image.fromarray(np.rint(coverage * 255.0).astype(np.uint8), "L")


class BuildAutoTrimapTests(unittest.TestCase):
    def test_band_structure_and_values(self) -> None:
        plate, _coverage = make_soft_disk_on_key()
        trimap = build_auto_trimap(plate, key=MAGENTA)
        array = np.asarray(trimap)

        # Only the three trimap values ever appear.
        self.assertTrue(set(np.unique(array).tolist()) <= {0, 128, 255})

        stats = trimap_stats(trimap)
        self.assertAlmostEqual(stats["bg"] + stats["fg"] + stats["unknown"], 100.0, places=1)
        # The fixture was built to have a real bg ring, a real fg core, and a
        # real transition band -- none of the three classes should vanish.
        self.assertGreater(stats["bg"], 5.0)
        self.assertGreater(stats["fg"], 1.0)
        self.assertGreater(stats["unknown"], 1.0)

        # The exact plate colour (flat key, dist=0) must always be sure background.
        self.assertEqual(int(array[0, 0]), 0)
        # Dead centre (coverage=1, dist=360.6 >> T2=150) must survive erosion as sure foreground.
        center = array.shape[0] // 2
        self.assertEqual(int(array[center, center]), 255)

    def test_custom_thresholds_change_band(self) -> None:
        plate, _coverage = make_soft_disk_on_key()
        wide = build_auto_trimap(plate, key=MAGENTA, t1=20.0, t2=300.0)
        narrow = build_auto_trimap(plate, key=MAGENTA, t1=70.0, t2=150.0)
        wide_stats = trimap_stats(wide)
        narrow_stats = trimap_stats(narrow)
        # A far-apart (t1, t2) pair claims MORE of the image as "unknown" than
        # the tuned (70, 150) pair, on the same soft-edged fixture.
        self.assertGreater(wide_stats["unknown"], narrow_stats["unknown"])


class BuildMaskSeededTrimapTests(unittest.TestCase):
    def test_band_structure_and_values(self) -> None:
        mask = make_soft_mask()
        trimap = build_mask_seeded_trimap(mask)
        array = np.asarray(trimap)

        self.assertTrue(set(np.unique(array).tolist()) <= {0, 128, 255})
        stats = trimap_stats(trimap)
        self.assertAlmostEqual(stats["bg"] + stats["fg"] + stats["unknown"], 100.0, places=1)
        self.assertGreater(stats["bg"], 5.0)
        self.assertGreater(stats["fg"], 1.0)
        self.assertGreater(stats["unknown"], 1.0)

        # Both sure-fg AND sure-bg are eroded for the mask-seeded builder
        # (unlike build_auto_trimap, whose bg mask is used raw), so a point
        # right at the array's literal edge is not a safe bg probe -- scipy's
        # erosion border handling can flip it. Probe a few px in instead,
        # still comfortably inside the flat coverage=0 ring (r_empty=90).
        self.assertEqual(int(array[10, 10]), 0)
        center = array.shape[0] // 2
        self.assertEqual(int(array[center, center]), 255)


class DespillTests(unittest.TestCase):
    def test_recovers_known_foreground_within_tolerance(self) -> None:
        fg = np.array([30, 180, 90], dtype=np.float64)
        alpha_value = 0.6
        size = 32
        plate = np.empty((size, size, 3), dtype=np.float64)
        key_arr = np.asarray(MAGENTA, dtype=np.float64)
        plate[:] = fg * alpha_value + key_arr * (1.0 - alpha_value)
        plate_u8 = np.rint(plate).astype(np.uint8)
        alpha01 = np.full((size, size), alpha_value, dtype=np.float64)

        recovered = despill(plate_u8, alpha01, MAGENTA)

        self.assertEqual(recovered.dtype, np.uint8)
        max_error = np.abs(recovered.astype(np.float64) - fg).max()
        self.assertLessEqual(max_error, 2.0)

    def test_below_clamp_returns_transparent_black(self) -> None:
        size = 16
        plate_u8 = np.full((size, size, 3), MAGENTA, dtype=np.uint8)
        alpha01 = np.full((size, size), 0.01, dtype=np.float64)  # below DESPILL_ALPHA_CLAMP=0.02

        recovered = despill(plate_u8, alpha01, MAGENTA)

        self.assertTrue(np.array_equal(recovered, np.zeros((size, size, 3), dtype=np.uint8)))

    def test_mixed_frame_clamps_only_the_low_alpha_region(self) -> None:
        fg = np.array([10, 20, 200], dtype=np.float64)
        key_arr = np.asarray(MAGENTA, dtype=np.float64)
        size = 8
        alpha01 = np.full((size, size), 0.5, dtype=np.float64)
        alpha01[:, : size // 2] = 0.0  # left half: pure background, below the clamp

        plate = np.empty((size, size, 3), dtype=np.float64)
        plate[:] = key_arr  # left half stays flat key
        plate[:, size // 2 :] = fg * 0.5 + key_arr * 0.5
        plate_u8 = np.rint(plate).astype(np.uint8)

        recovered = despill(plate_u8, alpha01, MAGENTA)

        self.assertTrue(np.array_equal(recovered[:, : size // 2], np.zeros((size, size // 2, 3), dtype=np.uint8)))
        right_error = np.abs(recovered[:, size // 2 :].astype(np.float64) - fg).max()
        self.assertLessEqual(right_error, 2.0)


if __name__ == "__main__":
    unittest.main()
