#!/usr/bin/env python3
"""Unit tests for the spritesheet packer (pure PIL, synthetic frames, no GPU).

Run from the repo root:
  PYTHONPATH=C:/projects/game-67-idle .venv/Scripts/python.exe -m unittest \
    ai_studio.assets.tools.video.sheet.pack_sheet_test
"""
from __future__ import annotations

import json
import os
import tempfile
import unittest

from PIL import Image

from ai_studio.assets.tools.video.sheet.pack_sheet import (
    SCHEMA,
    build_sheet,
    choose_columns,
    pack,
)


def solid_frame(w: int, h: int, rgba: tuple[int, int, int, int]) -> Image.Image:
    return Image.new("RGBA", (w, h), rgba)


def frames_with_colors(colors, w=8, h=6):
    return [solid_frame(w, h, c) for c in colors]


class ChooseColumnsTest(unittest.TestCase):
    def test_near_square_ceil_sqrt(self) -> None:
        cases = {1: 1, 2: 2, 4: 2, 5: 3, 7: 3, 9: 3, 16: 4, 25: 5, 33: 6, 36: 6, 37: 7}
        for count, expected in cases.items():
            self.assertEqual(choose_columns(count), expected, f"count={count}")

    def test_zero_frames_is_loud(self) -> None:
        with self.assertRaises(ValueError):
            choose_columns(0)


class PackGeometryTest(unittest.TestCase):
    def test_grid_dimensions_and_row_major_placement(self) -> None:
        # 5 distinct colors, default columns = ceil(sqrt(5)) = 3 -> 3x2 grid.
        colors = [
            (255, 0, 0, 255),
            (0, 255, 0, 255),
            (0, 0, 255, 255),
            (255, 255, 0, 255),
            (0, 255, 255, 255),
        ]
        w, h = 8, 6
        sheet, meta = pack(frames_with_colors(colors, w, h))
        self.assertEqual(meta["columns"], 3)
        self.assertEqual(meta["rows"], 2)
        self.assertEqual(meta["count"], 5)
        self.assertEqual(meta["frame_w"], w)
        self.assertEqual(meta["frame_h"], h)
        self.assertEqual(sheet.size, (3 * w, 2 * h))
        self.assertEqual(meta["layout"], "row-major")

        px = sheet.load()
        for i, color in enumerate(colors):
            r, c = divmod(i, 3)
            cx, cy = c * w + w // 2, r * h + h // 2
            self.assertEqual(px[cx, cy], color, f"frame {i} at cell (r={r},c={c})")
        # The 6th (empty) cell stays fully transparent.
        self.assertEqual(px[2 * w + w // 2, 1 * h + h // 2], (0, 0, 0, 0))

    def test_explicit_columns_override(self) -> None:
        sheet, meta = pack(frames_with_colors([(1, 2, 3, 255)] * 6, 10, 10), columns=6)
        self.assertEqual(meta["columns"], 6)
        self.assertEqual(meta["rows"], 1)
        self.assertEqual(sheet.size, (60, 10))

    def test_non_uniform_frames_is_loud(self) -> None:
        frames = [solid_frame(8, 8, (255, 0, 0, 255)), solid_frame(8, 9, (0, 255, 0, 255))]
        with self.assertRaises(ValueError):
            pack(frames)

    def test_empty_is_loud(self) -> None:
        with self.assertRaises(ValueError):
            pack([])


class TrimTest(unittest.TestCase):
    def _framed_dot(self, size=20, box=(8, 8, 12, 12)):
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        for x in range(box[0], box[2]):
            for y in range(box[1], box[3]):
                img.putpixel((x, y), (255, 255, 255, 255))
        return img

    def test_trim_off_keeps_full_frame(self) -> None:
        frames = [self._framed_dot() for _ in range(2)]
        sheet, meta = pack(frames, trim=False)
        self.assertEqual(meta["frame_w"], 20)
        self.assertEqual(meta["frame_h"], 20)
        self.assertFalse(meta["trim"])

    def test_trim_crops_to_union_bbox(self) -> None:
        # Two dots in different places -> the union bbox spans both.
        a = self._framed_dot(20, (2, 2, 5, 5))
        b = self._framed_dot(20, (14, 14, 18, 18))
        sheet, meta = pack([a, b], trim=True)
        # Union bbox = (2,2,18,18) -> 16x16 frame box for BOTH frames.
        self.assertEqual(meta["frame_w"], 16)
        self.assertEqual(meta["frame_h"], 16)
        self.assertTrue(meta["trim"])


class BuildSheetIoTest(unittest.TestCase):
    def test_writes_png_and_meta(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            frames_dir = os.path.join(tmp, "frames")
            out_dir = os.path.join(tmp, "sheet")
            os.makedirs(frames_dir)
            colors = [(255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 255, 255), (255, 255, 0, 255)]
            for i, color in enumerate(colors):
                solid_frame(12, 9, color).save(os.path.join(frames_dir, f"frame_{i:03d}.png"))

            meta = build_sheet(frames_dir, out_dir, "wings", fps=16)
            self.assertEqual(meta["schema"], SCHEMA)
            self.assertEqual(meta["count"], 4)
            self.assertEqual(meta["columns"], 2)
            self.assertEqual(meta["rows"], 2)
            self.assertEqual(meta["frame_w"], 12)
            self.assertEqual(meta["frame_h"], 9)
            self.assertEqual(meta["fps"], 16)
            self.assertEqual(meta["sheet_png"], "wings_sheet.png")

            png_path = os.path.join(out_dir, "wings_sheet.png")
            json_path = os.path.join(out_dir, "wings_sheet.json")
            self.assertTrue(os.path.isfile(png_path))
            self.assertTrue(os.path.isfile(json_path))
            with Image.open(png_path) as sheet:
                self.assertEqual(sheet.size, (24, 18))
            with open(json_path, "r", encoding="utf-8") as fh:
                on_disk = json.load(fh)
            self.assertEqual(on_disk["schema"], SCHEMA)
            self.assertIn("source", on_disk)


if __name__ == "__main__":
    unittest.main()
