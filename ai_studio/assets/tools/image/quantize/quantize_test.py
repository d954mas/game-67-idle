#!/usr/bin/env python3
from __future__ import annotations

import unittest

import numpy as np
from PIL import Image

from ai_studio.assets.tools.image.quantize.quantize import _count_unique_colors, quantize_image


def _gradient_rgba(width: int = 32, height: int = 32) -> Image.Image:
    """A smooth RGB gradient (lots of unique colors) with a half-transparent right
    half, so tests can prove alpha never moves and transparent-pixel RGB is free
    to change under quantization."""
    array = np.zeros((height, width, 4), dtype=np.uint8)
    for y in range(height):
        for x in range(width):
            red = int(round(x * 255 / (width - 1)))
            green = int(round(y * 255 / (height - 1)))
            alpha = 255 if x < width // 2 else 0
            array[y, x] = (red, green, 128, alpha)
    return Image.fromarray(array, "RGBA")


class QuantizeTests(unittest.TestCase):
    def test_alpha_byte_identical(self) -> None:
        image = _gradient_rgba()
        result, _stats = quantize_image(image, colors=8)
        before_alpha = np.asarray(image)[..., 3]
        after_alpha = np.asarray(result)[..., 3]
        self.assertTrue(np.array_equal(before_alpha, after_alpha), "alpha channel must be byte-identical")

    def test_reduces_unique_colors_to_at_most_n(self) -> None:
        image = _gradient_rgba()
        colors = 8
        result, stats = quantize_image(image, colors=colors)
        after_rgb = np.asarray(result)[..., :3]
        self.assertLessEqual(_count_unique_colors(after_rgb), colors)
        self.assertLessEqual(stats["palette_size_after"], colors)
        # The gradient has far more than `colors` unique colors before quantizing.
        self.assertGreater(stats["palette_size_before"], colors)

    def test_dither_still_preserves_alpha_and_the_color_cap(self) -> None:
        image = _gradient_rgba()
        colors = 6
        result, stats = quantize_image(image, colors=colors, dither=True)
        before_alpha = np.asarray(image)[..., 3]
        after_alpha = np.asarray(result)[..., 3]
        self.assertTrue(np.array_equal(before_alpha, after_alpha))
        self.assertLessEqual(stats["palette_size_after"], colors)

    def test_changed_pixel_pct_is_sane(self) -> None:
        image = _gradient_rgba()
        _result, stats = quantize_image(image, colors=4)
        self.assertGreater(stats["changed_pixel_pct"], 0.0)
        self.assertLessEqual(stats["changed_pixel_pct"], 100.0)

    def test_flat_image_at_its_own_color_count_is_a_near_no_op(self) -> None:
        # A single flat color quantized to a generous palette should not need to
        # move any pixel's RGB (Pillow may not report a perfect 0%, but it must
        # stay very small — sanity, not an exact-zero guarantee).
        array = np.full((16, 16, 4), (90, 140, 200, 255), dtype=np.uint8)
        image = Image.fromarray(array, "RGBA")
        _result, stats = quantize_image(image, colors=16)
        self.assertEqual(stats["palette_size_before"], 1)
        self.assertLessEqual(stats["changed_pixel_pct"], 5.0)

    def test_rejects_out_of_range_colors(self) -> None:
        image = _gradient_rgba()
        with self.assertRaises(ValueError):
            quantize_image(image, colors=1)
        with self.assertRaises(ValueError):
            quantize_image(image, colors=257)


if __name__ == "__main__":
    unittest.main()
