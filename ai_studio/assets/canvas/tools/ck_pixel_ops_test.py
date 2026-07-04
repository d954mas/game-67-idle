import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from ai_studio.assets.canvas.tools.ck_pixel_ops import (
    compose_regions,
    hue_rotate_180_rgb01,
    hue_shift_image,
    run,
)


class HueRotate180Test(unittest.TestCase):
    """Core HSV math (numpy port of trick_run.py's cv2-based hue180)."""

    def test_magenta_rotates_to_green(self) -> None:
        magenta = np.array([[[1.0, 0.0, 1.0]]], dtype=np.float32)
        rotated = hue_rotate_180_rgb01(magenta)
        np.testing.assert_allclose(rotated[0, 0], [0.0, 1.0, 0.0], atol=1e-5)

    def test_green_rotates_to_magenta(self) -> None:
        green = np.array([[[0.0, 1.0, 0.0]]], dtype=np.float32)
        rotated = hue_rotate_180_rgb01(green)
        np.testing.assert_allclose(rotated[0, 0], [1.0, 0.0, 1.0], atol=1e-5)

    def test_grayscale_is_hue_stable(self) -> None:
        gray = np.array([[[0.4, 0.4, 0.4]]], dtype=np.float32)
        rotated = hue_rotate_180_rgb01(gray)
        np.testing.assert_allclose(rotated[0, 0], [0.4, 0.4, 0.4], atol=1e-5)

    def test_value_and_saturation_preserved(self) -> None:
        # An arbitrary tinted color: after rotation, S and V should be unchanged (only H moves).
        color = np.array([[[0.8, 0.3, 0.5]]], dtype=np.float32)
        rotated = hue_rotate_180_rgb01(color)
        self.assertAlmostEqual(float(color.max()), float(rotated.max()), places=5, msg="V (max channel) preserved")
        orig_delta = float(color.max() - color.min())
        rot_delta = float(rotated.max() - rotated.min())
        self.assertAlmostEqual(orig_delta, rot_delta, places=5, msg="S (chroma spread) preserved")


class HueShiftImageTest(unittest.TestCase):
    """File/Image-level shim: the magenta<->green round trip the corridorkey op relies on."""

    def test_round_trip_magenta_bg_to_green_and_back_bounded_drift(self) -> None:
        # A magenta-keyed sheet with a dark-ish subject blob (value-preserving hue rotation
        # must keep dark colors dark — see research_corridorkey_magenta_2026-07-05.md).
        width, height = 40, 30
        original = Image.new("RGB", (width, height), (255, 0, 255))
        ImageDraw.Draw(original).rectangle([10, 8, 28, 20], fill=(30, 90, 160))

        shifted_to_green = hue_shift_image(original)
        shifted_back = hue_shift_image(shifted_to_green)

        # The intermediate really reads as green where the source was magenta.
        bg_green = np.asarray(shifted_to_green.convert("RGB"))[0, 0]
        self.assertLess(int(bg_green[0]), 40, "R low after magenta->green shim")
        self.assertGreater(int(bg_green[1]), 200, "G high after magenta->green shim")
        self.assertLess(int(bg_green[2]), 40, "B low after magenta->green shim")

        # Round trip (rotate +180 twice = +360 = identity) lands back close to the original —
        # small drift only from the two float<->uint8 round trips, not from a value shift.
        orig_arr = np.asarray(original, dtype=np.int32)
        back_arr = np.asarray(shifted_back.convert("RGB"), dtype=np.int32)
        drift = np.abs(orig_arr - back_arr)
        self.assertLessEqual(int(drift.max()), 2, f"round-trip drift too high: max {int(drift.max())}")

    def test_preserves_alpha_byte_exact(self) -> None:
        width, height = 6, 4
        rgba = np.zeros((height, width, 4), dtype=np.uint8)
        rgba[..., 0] = 255  # R
        rgba[..., 2] = 255  # B -> flat magenta RGB
        rgba[..., 3] = np.arange(width * height, dtype=np.uint8).reshape(height, width)
        image = Image.fromarray(rgba, "RGBA")

        shifted = hue_shift_image(image)

        self.assertEqual(shifted.mode, "RGBA")
        shifted_arr = np.asarray(shifted)
        self.assertTrue(np.array_equal(shifted_arr[..., 3], rgba[..., 3]), "alpha must be byte-exact, never recomputed")
        self.assertFalse(np.array_equal(shifted_arr[..., :3], rgba[..., :3]), "RGB must actually change (hue rotated)")

    def test_no_alpha_source_stays_rgb(self) -> None:
        image = Image.new("RGB", (4, 4), (255, 0, 255))
        shifted = hue_shift_image(image)
        self.assertEqual(shifted.mode, "RGB")


class ComposeRegionsTest(unittest.TestCase):
    """Region-scoped corridorkey composite: source outside regions, CK result inside."""

    def test_rect_region_inside_vs_outside(self) -> None:
        width, height = 20, 20
        source = Image.new("RGBA", (width, height), (255, 0, 255, 255))
        keyed = Image.new("RGBA", (width, height), (10, 20, 30, 128))
        regions = [{"id": "r1", "rect": [4, 4, 8, 8]}]

        out, reports = compose_regions(source, keyed, regions)

        self.assertEqual(reports, [{"id": "r1", "rect": [4, 4, 8, 8]}])
        self.assertEqual(out.getpixel((6, 6)), (10, 20, 30, 128), "inside the region: CK result pixel")
        self.assertEqual(out.getpixel((15, 15)), (255, 0, 255, 255), "outside the region: original source, untouched")

    def test_polygon_masks_within_the_rect(self) -> None:
        width, height = 20, 20
        source = Image.new("RGBA", (width, height), (255, 0, 255, 255))
        keyed = Image.new("RGBA", (width, height), (10, 20, 30, 200))
        # Triangle covering the top-left of the full-size rect.
        regions = [{"id": "tri", "rect": [0, 0, width, height], "polygon": [[0, 0], [width, 0], [0, height]]}]

        out, _ = compose_regions(source, keyed, regions)

        self.assertEqual(out.getpixel((2, 2)), (10, 20, 30, 200), "inside the polygon: CK result")
        self.assertEqual(out.getpixel((18, 18)), (255, 0, 255, 255), "inside rect but outside polygon: source kept")

    def test_rejects_keyed_size_mismatch(self) -> None:
        source = Image.new("RGBA", (10, 10), (255, 0, 255, 255))
        keyed = Image.new("RGBA", (12, 10), (0, 0, 0, 0))
        with self.assertRaises(ValueError):
            compose_regions(source, keyed, [{"id": "r1", "rect": [0, 0, 5, 5]}])


class RunSpecTest(unittest.TestCase):
    """The --spec CLI entry (run()): file I/O, report writing, loud refusals."""

    def test_run_hue_shift_writes_output_and_report(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source.png"
            Image.new("RGB", (4, 4), (255, 0, 255)).save(source)
            output = root / "out.png"
            report_path = root / "report.json"

            report = run({"op": "hue_shift", "source": str(source), "output": str(output), "report": str(report_path)})

            self.assertTrue(output.exists())
            self.assertEqual(report["op"], "hue_shift")
            self.assertTrue(report_path.exists())
            saved = json.loads(report_path.read_text(encoding="utf-8"))
            self.assertEqual(saved["op"], "hue_shift")

    def test_run_compose_regions_writes_output_and_report(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "s.png"
            keyed = root / "k.png"
            Image.new("RGBA", (10, 10), (255, 0, 255, 255)).save(source)
            Image.new("RGBA", (10, 10), (1, 2, 3, 250)).save(keyed)
            output = root / "out.png"

            report = run(
                {
                    "op": "compose_regions",
                    "source": str(source),
                    "keyed": str(keyed),
                    "regions": [{"id": "r1", "rect": [0, 0, 5, 5]}],
                    "output": str(output),
                }
            )

            self.assertTrue(output.exists())
            self.assertEqual(report["region_count"], 1)
            out_img = Image.open(output).convert("RGBA")
            self.assertEqual(out_img.getpixel((1, 1)), (1, 2, 3, 250))
            self.assertEqual(out_img.getpixel((8, 8)), (255, 0, 255, 255))

    def test_run_unknown_op_raises(self) -> None:
        with self.assertRaises(ValueError):
            run({"op": "nonsense", "source": "x", "output": "y"})

    def test_run_hue_shift_missing_source_raises(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            missing = Path(tmp) / "missing.png"
            with self.assertRaises(FileNotFoundError):
                run({"op": "hue_shift", "source": str(missing), "output": str(Path(tmp) / "out.png")})

    def test_run_compose_regions_empty_regions_raises(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "s.png"
            keyed = root / "k.png"
            Image.new("RGBA", (4, 4), (255, 0, 255, 255)).save(source)
            Image.new("RGBA", (4, 4), (0, 0, 0, 0)).save(keyed)
            with self.assertRaises(ValueError):
                run(
                    {
                        "op": "compose_regions",
                        "source": str(source),
                        "keyed": str(keyed),
                        "regions": [],
                        "output": str(root / "out.png"),
                    }
                )


if __name__ == "__main__":
    unittest.main()
