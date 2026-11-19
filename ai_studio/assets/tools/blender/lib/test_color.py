import tempfile
import unittest
from pathlib import Path

import numpy as np

from ai_studio.assets.tools.blender.lib import color as C


class Conversion(unittest.TestCase):
    def test_hex_round_trip(self):
        for text in ("#000000", "#ffffff", "#7f3a10", "#bcbcbc"):
            self.assertEqual(C.linear_to_hex(C.hex_to_linear(text)[:3]), text)

    def test_mid_grey_is_dark_in_linear(self):
        self.assertAlmostEqual(C.hex_to_linear("#bcbcbc")[0], 0.5, delta=0.01)


class Palette(unittest.TestCase):
    def test_overflow_raises_instead_of_wrapping(self):
        atlas = C.PaletteAtlas(width=8, height=16, row_height=4)
        for k in range(atlas.capacity):
            atlas.add_row(f"m{k}", ["#ff0000"])
        with self.assertRaises(C.PaletteFull):
            atlas.add_row("one too many", ["#00ff00"])

    def test_uv_lands_on_row_centre(self):
        atlas = C.PaletteAtlas(width=8, height=16, row_height=4)
        atlas.add_row("a", ["#ff0000"])
        atlas.add_row("b", ["#0000ff"])
        u, v = atlas.uv("b")
        row = int(v * atlas.height)
        self.assertEqual(row, 6)
        self.assertEqual(tuple(atlas.pixels[row, int(u * atlas.width)]), (0, 0, 255, 255))

    def test_alpha_floor_and_small_rows(self):
        with self.assertRaises(ValueError):
            C.PaletteAtlas(height=16, row_height=2)
        atlas = C.PaletteAtlas(width=8, height=8, row_height=4, alpha_floor=12)
        with self.assertRaises(ValueError):
            atlas.add_row("too clear", ["#ffffff"], alpha=8)

    def test_ramp_interpolates_in_linear_space(self):
        atlas = C.PaletteAtlas(width=3, height=4, row_height=4)
        atlas.add_row("ramp", ["#000000", "#ffffff"])
        middle = atlas.pixels[0, 1, 0]
        self.assertEqual(int(middle), int(np.round(C.linear_to_srgb(0.5) * 255)))

    def test_png_writes(self):
        atlas = C.PaletteAtlas(width=4, height=4, row_height=4)
        atlas.add_row("a", ["#123456"])
        with tempfile.TemporaryDirectory() as tmp:
            path = atlas.write_png(Path(tmp) / "p.png")
            self.assertEqual(path.read_bytes()[:8], b"\x89PNG\r\n\x1a\n")


if __name__ == "__main__":
    unittest.main()
