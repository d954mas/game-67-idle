#!/usr/bin/env python3
from __future__ import annotations

import unittest

import numpy as np
from PIL import Image

from ai_studio.assets.tools.image.alpha_dualplate.dual_plate_pair_gate import evaluate
from ai_studio.assets.tools.image.alpha_dualplate.pair_align import align_pair


def _plate(bg: tuple[int, int, int], box: tuple[int, int, int, int], fg=(120, 80, 40), size=(60, 50)) -> Image.Image:
    width, height = size
    x0, y0, x1, y1 = box
    array = np.zeros((height, width, 4), dtype=np.uint8)
    array[..., :3] = bg
    array[..., 3] = 255
    array[y0:y1, x0:x1, :3] = fg
    return Image.fromarray(array, "RGBA")


class PairAlignTests(unittest.TestCase):
    def test_recovers_a_known_translation(self) -> None:
        # Sign convention (see pair_align.align_pair docstring): the dark plate's
        # subject sits (3, -2) away from the light plate's subject (3 px right,
        # 2 px up) -> the fix is to shift the dark plate (-3, +2).
        light = _plate((255, 255, 255), (20, 15, 40, 35))
        dark = _plate((0, 0, 0), (23, 13, 43, 33))  # box offset by (+3, -2)

        dx, dy, fraction, aligned = align_pair(light, dark, max_shift=8)

        self.assertEqual((dx, dy), (-3, 2))
        self.assertLess(fraction, 0.05)
        # The aligned pair now passes the gate it previously wouldn't have.
        self.assertEqual(evaluate(light, aligned)["verdict"], "pass")

    def test_already_aligned_pair_returns_zero_shift_unchanged(self) -> None:
        light = _plate((255, 255, 255), (20, 15, 40, 35))
        dark = _plate((0, 0, 0), (20, 15, 40, 35))
        before = evaluate(light, dark)["inconsistent_fraction"]

        dx, dy, fraction, aligned = align_pair(light, dark, max_shift=8)

        self.assertEqual((dx, dy), (0, 0))
        self.assertEqual(fraction, before)
        self.assertTrue(np.array_equal(np.asarray(aligned), np.asarray(dark.convert("RGBA"))))

    def test_never_returns_a_shift_worse_than_the_original(self) -> None:
        # A pair too misaligned for an 8px search to fully fix (drift = 20px):
        # align_pair may not reach a passing verdict, but it must not make the
        # fraction WORSE than doing nothing.
        light = _plate((255, 255, 255), (20, 15, 40, 35))
        dark = _plate((0, 0, 0), (40, 15, 60, 35))  # 20px drift, outside max_shift
        before = evaluate(light, dark)["inconsistent_fraction"]

        dx, dy, fraction, _aligned = align_pair(light, dark, max_shift=8)

        self.assertLessEqual(fraction, before)

    def test_shift_pads_with_background_not_wraparound(self) -> None:
        # Direct test of the shift primitive align_pair searches with: a marker
        # at the FAR edge is wraparound bait — numpy.roll would smear it into the
        # newly revealed border on the opposite side; edge-crop + background-pad
        # must not.
        from ai_studio.assets.tools.image.alpha_dualplate.pair_align import _pad_with_fill, _shifted_view

        width, height = 20, 10
        array = np.zeros((height, width, 3), dtype=np.float32)
        array[:, 8:10] = (120, 80, 40)  # mid-frame marker, to verify correct translation
        array[:, width - 2 :] = (200, 10, 10)  # right-edge marker: wraparound bait

        fill = np.zeros(3, dtype=np.float32)
        padded = _pad_with_fill(array, pad=4, fill=fill)
        shifted = _shifted_view(padded, pad=4, height=height, width=width, dx=3, dy=0)

        # Correct translation: the mid-frame marker (columns 8-9) is now at 11-12.
        self.assertTrue(np.all(shifted[:, 11:13] == (120, 80, 40)))
        # Revealed columns 0-2 (dx=3) must be background fill, NOT the right-edge
        # marker that numpy.roll would wrap around into this region.
        self.assertTrue(np.all(shifted[:, :3] == 0), f"revealed border leaked wrapped content: {shifted[:, :3]}")

    def test_cli_reports_before_after_fraction(self) -> None:
        import json
        import subprocess
        import sys
        import tempfile
        from pathlib import Path

        light = _plate((255, 255, 255), (20, 15, 40, 35))
        dark = _plate((0, 0, 0), (23, 13, 43, 33))
        script = Path(__file__).resolve().parent / "pair_align.py"

        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            light_path = root / "light.png"
            dark_path = root / "dark.png"
            json_path = root / "align.json"
            light.save(light_path)
            dark.save(dark_path)

            result = subprocess.run(
                [sys.executable, str(script), "--light", str(light_path), "--dark", str(dark_path), "--json-output", str(json_path)],
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads(json_path.read_text(encoding="utf-8"))
            self.assertEqual(report["schema"], "game.dual_plate_pair_align")
            self.assertEqual((report["dx"], report["dy"]), (-3, 2))
            self.assertGreater(report["fraction_before"], report["fraction_after"])


if __name__ == "__main__":
    unittest.main()
