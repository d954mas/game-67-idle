import argparse
import unittest

import numpy as np
from PIL import Image

from ai_studio.assets.tools.lib.color import (
    estimate_border_key,
    format_hex,
    key_distance,
    merge_alpha,
    parse_hex,
    split_alpha,
)


class KeyDistanceTest(unittest.TestCase):
    def test_matches_manual_chebyshev_on_rgb_array(self) -> None:
        pixels = np.array([[[10, 20, 30], [255, 0, 255], [0, 0, 0]]], dtype=np.uint8)
        key = (255, 0, 255)
        distance = key_distance(pixels, key)
        expected = np.array([[max(abs(10 - 255), abs(20 - 0), abs(30 - 255)), 0, max(255, 0, 255)]])
        self.assertTrue(np.array_equal(distance, expected))

    def test_ignores_extra_channels_so_rgba_can_be_passed_directly(self) -> None:
        rgba = np.array([[[10, 20, 30, 128]]], dtype=np.uint8)
        rgb = rgba[..., :3]
        self.assertTrue(np.array_equal(key_distance(rgba, (0, 0, 0)), key_distance(rgb, (0, 0, 0))))

    def test_zero_at_exact_key(self) -> None:
        pixels = np.full((4, 4, 3), (12, 34, 56), dtype=np.uint8)
        self.assertTrue(np.all(key_distance(pixels, (12, 34, 56)) == 0))

    def test_scale_agnostic_when_pixels_and_key_match_scale(self) -> None:
        # 0-1 normalized pixels + a matching 0-1 key must give the same shape
        # of answer as the 0-255 case (this is what key_matte.py relies on).
        pixels = np.array([[[0.5, 0.5, 0.5]]], dtype=np.float64)
        key = np.array([0.0, 0.0, 0.0], dtype=np.float64)
        self.assertAlmostEqual(float(key_distance(pixels, key)[0, 0]), 0.5)


class ParseFormatHexTest(unittest.TestCase):
    def test_parse_hex_roundtrips_with_format_hex(self) -> None:
        self.assertEqual(parse_hex("#ff00ff"), (255, 0, 255))
        self.assertEqual(format_hex((255, 0, 255)), "#ff00ff")

    def test_parse_hex_accepts_without_leading_hash(self) -> None:
        self.assertEqual(parse_hex("00ff80"), (0, 255, 128))

    def test_parse_hex_rejects_wrong_length(self) -> None:
        with self.assertRaises(argparse.ArgumentTypeError):
            parse_hex("#fff")

    def test_parse_hex_rejects_non_hex_digits(self) -> None:
        with self.assertRaises(argparse.ArgumentTypeError):
            parse_hex("#zzzzzz")


class EstimateBorderKeyTest(unittest.TestCase):
    def test_mode_of_uniform_border(self) -> None:
        image = Image.new("RGBA", (10, 8), (255, 0, 255, 255))
        self.assertEqual(estimate_border_key(image), (255, 0, 255))

    def test_mode_wins_over_a_few_stray_border_pixels(self) -> None:
        # Mode convention (T0254 F4): a border that's mostly flat key plus a
        # handful of odd pixels must still resolve to the exact key color, not
        # get pulled toward the stray pixels the way a median could.
        image = Image.new("RGBA", (12, 10), (255, 0, 255, 255))
        image.putpixel((0, 0), (10, 250, 10, 255))
        image.putpixel((1, 0), (10, 250, 10, 255))
        self.assertEqual(estimate_border_key(image), (255, 0, 255))

    def test_alpha_threshold_excludes_transparent_border_pixels(self) -> None:
        image = Image.new("RGBA", (10, 8), (0, 200, 0, 0))
        for x in range(10):
            image.putpixel((x, 0), (20, 40, 60, 255))
        self.assertEqual(estimate_border_key(image, alpha_threshold=0), (20, 40, 60))

    def test_falls_back_when_border_fully_transparent(self) -> None:
        image = Image.new("RGBA", (6, 6), (11, 22, 33, 0))
        self.assertEqual(estimate_border_key(image, fallback=(1, 2, 3)), (1, 2, 3))


class SplitMergeAlphaTest(unittest.TestCase):
    def test_split_then_merge_is_lossless(self) -> None:
        image = Image.new("RGBA", (4, 3), (10, 20, 30, 200))
        rgba, rgb, alpha = split_alpha(image)
        merged = merge_alpha(rgb, alpha)
        self.assertTrue(np.array_equal(np.asarray(merged), rgba))

    def test_merge_lets_rgb_change_while_alpha_stays_identical(self) -> None:
        image = Image.new("RGBA", (4, 3), (10, 20, 30, 200))
        _rgba, rgb, alpha = split_alpha(image)
        processed_rgb = np.clip(rgb.astype(np.int16) + 5, 0, 255).astype(np.uint8)
        merged = merge_alpha(processed_rgb, alpha)
        merged_array = np.asarray(merged)
        self.assertTrue(np.array_equal(merged_array[..., 3], alpha))
        self.assertTrue(np.array_equal(merged_array[..., :3], processed_rgb))

    def test_split_converts_non_rgba_input(self) -> None:
        image = Image.new("RGB", (2, 2), (1, 2, 3))
        rgba, _rgb, alpha = split_alpha(image)
        self.assertEqual(rgba.shape[-1], 4)
        self.assertTrue(np.all(alpha == 255))


if __name__ == "__main__":
    unittest.main()
