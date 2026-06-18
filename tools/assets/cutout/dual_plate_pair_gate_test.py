#!/usr/bin/env python3
from __future__ import annotations

import unittest

import numpy as np
from PIL import Image

from tools.assets.cutout.dual_plate_pair_gate import evaluate


def _plate(bg: tuple[int, int, int], subject_cols: tuple[int, int], fg=(120, 80, 40)) -> Image.Image:
    array = np.zeros((20, 20, 4), dtype=np.uint8)
    array[..., :3] = bg
    array[..., 3] = 255
    array[5:15, subject_cols[0] : subject_cols[1], :3] = fg
    return Image.fromarray(array, "RGBA")


class DualPlatePairGateTests(unittest.TestCase):
    def test_consistent_pair_passes(self) -> None:
        # Same opaque subject in the same place on white and black -> consistent.
        light = _plate((255, 255, 255), (5, 15))
        dark = _plate((0, 0, 0), (5, 15))
        report = evaluate(light, dark)
        self.assertEqual(report["verdict"], "pass")
        self.assertLess(report["inconsistent_fraction"], 0.05)

    def test_redrawn_pair_regenerates(self) -> None:
        # Subject in a different place on each plate (generator redrew it) -> ghost.
        light = _plate((255, 255, 255), (2, 8))
        dark = _plate((0, 0, 0), (12, 18))
        report = evaluate(light, dark)
        self.assertEqual(report["verdict"], "regenerate")

    def test_resizes_mismatched_dark_plate(self) -> None:
        light = _plate((255, 255, 255), (5, 15))
        dark = _plate((0, 0, 0), (5, 15)).resize((24, 24))
        report = evaluate(light, dark)
        self.assertIn(report["verdict"], {"pass", "align", "regenerate"})
        self.assertEqual(report["size"], [20, 20])


if __name__ == "__main__":
    unittest.main()
