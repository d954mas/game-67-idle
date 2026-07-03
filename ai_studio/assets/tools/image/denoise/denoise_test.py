#!/usr/bin/env python3
from __future__ import annotations

import unittest

import numpy as np
from PIL import Image

from ai_studio.assets.tools.image.denoise.denoise import denoise_image


def _flat_rgba(width: int = 24, height: int = 24, color: tuple[int, int, int, int] = (120, 60, 200, 255)) -> Image.Image:
    array = np.empty((height, width, 4), dtype=np.uint8)
    array[..., 0] = color[0]
    array[..., 1] = color[1]
    array[..., 2] = color[2]
    array[..., 3] = color[3]
    return Image.fromarray(array, "RGBA")


def _speckled_rgba(width: int = 24, height: int = 24) -> Image.Image:
    """A flat block with a handful of salt-and-pepper outlier pixels (what a
    median filter is FOR) plus a transparent corner — proves alpha never moves
    even where RGB is being denoised right next to it."""
    array = np.full((height, width, 4), (120, 60, 200, 255), dtype=np.uint8)
    array[4, 4, :3] = (255, 0, 0)
    array[10, 12, :3] = (0, 255, 0)
    array[18, 20, :3] = (0, 0, 255)
    array[2:6, 2:6, 3] = 0  # a transparent corner — alpha must never move
    return Image.fromarray(array, "RGBA")


class DenoiseTests(unittest.TestCase):
    def test_alpha_never_filtered(self) -> None:
        image = _speckled_rgba()
        for strength in (1, 2, 3):
            result, _stats = denoise_image(image, strength=strength)
            before_alpha = np.asarray(image)[..., 3]
            after_alpha = np.asarray(result)[..., 3]
            self.assertTrue(np.array_equal(before_alpha, after_alpha), f"alpha moved at strength {strength}")

    def test_flat_image_is_byte_identical_nothing_to_denoise(self) -> None:
        image = _flat_rgba()
        for strength in (1, 2, 3):
            result, stats = denoise_image(image, strength=strength)
            self.assertTrue(np.array_equal(np.asarray(image), np.asarray(result)), f"flat image changed at strength {strength}")
            self.assertEqual(stats["changed_pixel_pct"], 0.0)

    def test_removes_salt_and_pepper_outliers(self) -> None:
        image = _speckled_rgba()
        result, stats = denoise_image(image, strength=1)
        after = np.asarray(result)
        self.assertTrue(np.array_equal(after[4, 4, :3], (120, 60, 200)), "outlier pixel not median-filtered away")
        self.assertGreater(stats["changed_pixel_pct"], 0.0)
        # Only a small neighborhood around each of the 3 outliers should move.
        self.assertLess(stats["changed_pixel_pct"], 5.0)

    def test_higher_strength_touches_at_least_as_many_pixels(self) -> None:
        image = _speckled_rgba()
        _r1, stats1 = denoise_image(image, strength=1)
        _r3, stats3 = denoise_image(image, strength=3)
        self.assertGreaterEqual(stats3["changed_pixel_pct"], stats1["changed_pixel_pct"])

    def test_rejects_bad_strength(self) -> None:
        image = _flat_rgba()
        with self.assertRaises(ValueError):
            denoise_image(image, strength=4)
        with self.assertRaises(ValueError):
            denoise_image(image, strength=0)


if __name__ == "__main__":
    unittest.main()
