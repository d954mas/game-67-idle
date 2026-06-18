#!/usr/bin/env python3
from __future__ import annotations

import unittest

from PIL import Image

import tools.assets.cutout.key_matte as key_matte
from tools.assets.cutout.key_matte import key_matte_cutout


def make_ring_on_key(size: int = 160, key: tuple[int, int, int] = (0, 255, 0)):
    import numpy as np

    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    center = (size - 1) / 2.0
    radius = np.sqrt((xx - center) ** 2 + (yy - center) ** 2)
    outer, inner, aa = size * 0.42, size * 0.24, 1.6
    annulus = np.clip((outer - radius) / aa, 0, 1) * np.clip((radius - inner) / aa, 0, 1)
    foreground = np.asarray((150, 162, 178), dtype=np.float64)
    composite = np.empty((size, size, 3), dtype=np.float64)
    for channel in range(3):
        composite[..., channel] = foreground[channel] * annulus + key[channel] * (1.0 - annulus)
    image = np.zeros((size, size, 4), dtype=np.uint8)
    image[..., :3] = np.rint(composite).astype(np.uint8)
    image[..., 3] = 255
    truth_alpha = np.rint(annulus * 255.0).astype(np.uint8)
    return Image.fromarray(image, "RGBA"), truth_alpha, (int(round(center)), int(round(center)))


class KeyMatteTests(unittest.TestCase):
    def test_recovers_ring_with_transparent_hole(self) -> None:
        if key_matte.np is None:
            self.skipTest("numpy required")
        try:
            import pymatting  # noqa: F401
        except Exception:
            self.skipTest("pymatting required")
        import numpy as np

        crop, truth_alpha, center = make_ring_on_key()
        result = key_matte_cutout(crop, (0, 255, 0))
        alpha = np.asarray(result.getchannel("A"))

        # Interior hole must be transparent (the chroma-inside-art failure case).
        self.assertLess(int(alpha[center[1], center[0]]), 40)
        # A point on the annulus must stay opaque.
        ring_y = int(center[1] - 160 * 0.33)
        self.assertGreater(int(alpha[ring_y, center[0]]), 200)
        # Recovered alpha is close to the known truth.
        sad = float(np.mean(np.abs(alpha.astype(np.float64) - truth_alpha.astype(np.float64))))
        self.assertLess(sad, 8.0)
        # Edge colour is decontaminated: almost no strongly-green opaque pixels remain.
        array = np.asarray(result).astype(np.int16)
        visible = array[..., 3] > 40
        green_spill = visible & (array[..., 1] > array[..., 0] + 60) & (array[..., 1] > array[..., 2] + 60)
        self.assertLess(int(np.count_nonzero(green_spill)), 10)

    def test_fallback_without_numpy_returns_rgba(self) -> None:
        original = key_matte.np
        try:
            key_matte.np = None
            crop = Image.new("RGBA", (8, 8), (0, 255, 0, 255))
            crop.putpixel((4, 4), (200, 100, 50, 255))
            result = key_matte_cutout(crop, (0, 255, 0))
            self.assertEqual(result.mode, "RGBA")
            self.assertEqual(result.size, (8, 8))
        finally:
            key_matte.np = original


if __name__ == "__main__":
    unittest.main()
