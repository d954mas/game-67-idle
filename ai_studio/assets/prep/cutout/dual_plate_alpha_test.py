#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

from ai_studio.assets.prep.cutout.dual_plate_alpha import build_report, cleanup_alpha_blobs, extract_dual_plate_alpha, report_image_stats


ROOT = Path(__file__).resolve().parents[4]
SCRIPT = ROOT / "ai_studio/assets/prep/cutout/dual_plate_alpha.py"


def composite_pixel(foreground: tuple[int, int, int], alpha: int, background: tuple[int, int, int]) -> tuple[int, int, int, int]:
    alpha_f = alpha / 255
    return (
        round(foreground[0] * alpha_f + background[0] * (1 - alpha_f)),
        round(foreground[1] * alpha_f + background[1] * (1 - alpha_f)),
        round(foreground[2] * alpha_f + background[2] * (1 - alpha_f)),
        255,
    )


class DualPlateAlphaTests(unittest.TestCase):
    def test_extracts_alpha_and_foreground_color_from_white_black_plates(self) -> None:
        foreground = (120, 80, 40)
        alpha = 128
        light = Image.new("RGBA", (3, 3), composite_pixel(foreground, alpha, (255, 255, 255)))
        dark = Image.new("RGBA", (3, 3), composite_pixel(foreground, alpha, (0, 0, 0)))

        result = extract_dual_plate_alpha(light, dark, alpha_combine="avg", recovery_source="average")
        red, green, blue, result_alpha = result.getpixel((1, 1))

        self.assertLessEqual(abs(red - foreground[0]), 1)
        self.assertLessEqual(abs(green - foreground[1]), 1)
        self.assertLessEqual(abs(blue - foreground[2]), 1)
        self.assertLessEqual(abs(result_alpha - alpha), 1)

    def test_opaque_subject_stays_opaque(self) -> None:
        foreground = (12, 140, 220)
        light = Image.new("RGBA", (2, 2), (*foreground, 255))
        dark = Image.new("RGBA", (2, 2), (*foreground, 255))

        result = extract_dual_plate_alpha(light, dark)

        self.assertEqual(result.getpixel((0, 0)), (*foreground, 255))

    def test_alpha_hardening_clears_low_alpha_pixels(self) -> None:
        foreground = (200, 90, 30)
        alpha = 24
        light = Image.new("RGBA", (1, 1), composite_pixel(foreground, alpha, (255, 255, 255)))
        dark = Image.new("RGBA", (1, 1), composite_pixel(foreground, alpha, (0, 0, 0)))

        result = extract_dual_plate_alpha(light, dark, alpha_hardening=32)

        self.assertEqual(result.getpixel((0, 0)), (0, 0, 0, 0))

    def test_rejects_dimension_mismatch(self) -> None:
        with self.assertRaisesRegex(ValueError, "plate dimensions differ"):
            extract_dual_plate_alpha(Image.new("RGBA", (2, 2)), Image.new("RGBA", (3, 2)))

    def test_cleanup_blobs_removes_small_components(self) -> None:
        image = Image.new("RGBA", (8, 8), (0, 0, 0, 0))
        pixels = image.load()
        for y in range(2, 5):
            for x in range(2, 5):
                pixels[x, y] = (200, 100, 50, 255)
        pixels[7, 7] = (200, 100, 50, 255)

        removed = cleanup_alpha_blobs(image, min_area=2)

        self.assertEqual(removed, 1)
        self.assertEqual(image.getpixel((7, 7)), (0, 0, 0, 0))
        self.assertEqual(image.getpixel((3, 3))[3], 255)

    def test_report_counts_hidden_rgb_under_transparency(self) -> None:
        image = Image.new("RGBA", (2, 1), (0, 0, 0, 0))
        image.putpixel((1, 0), (200, 20, 120, 0))

        report = build_report(image, removed_blob_pixels=0)

        self.assertEqual(report["alpha_bbox"], None)
        self.assertEqual(report["transparent_nonzero_rgb_pixels"], 1)
        self.assertEqual(report["verdict"], "fail")
        self.assertTrue(any("no visible alpha pixels" in problem for problem in report["problems"]))
        self.assertTrue(any("transparent pixels retain non-zero RGB" in problem for problem in report["problems"]))

    def test_cli_writes_png_and_json_report(self) -> None:
        foreground = (100, 70, 40)
        alpha = 180
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            light_path = root / "light.png"
            dark_path = root / "dark.png"
            output_path = root / "out.png"
            json_path = root / "report.json"
            Image.new("RGBA", (4, 4), composite_pixel(foreground, alpha, (255, 255, 255))).save(light_path)
            Image.new("RGBA", (4, 4), composite_pixel(foreground, alpha, (0, 0, 0))).save(dark_path)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--light",
                    str(light_path),
                    "--dark",
                    str(dark_path),
                    "--output",
                    str(output_path),
                    "--json-output",
                    str(json_path),
                    "--report",
                    str(root / "report.md"),
                    "--profile",
                ],
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(output_path.exists())
            report = json.loads(json_path.read_text(encoding="utf-8"))
            self.assertEqual(report["schema"], "game.dual_plate_alpha_report")
            self.assertEqual(report["verdict"], "pass")
            self.assertEqual(report["analysis_engine"], "numpy")
            self.assertEqual(report["problems"], [])
            self.assertEqual(report["visible_pixels"], 16)
            self.assertEqual(report["transparent_nonzero_rgb_pixels"], 0)
            self.assertEqual(report["alpha_bbox"], [0, 0, 4, 4])
            self.assertIn("timing_ms", report)
            self.assertIn("profile: dual-plate alpha total", result.stdout)
            self.assertIn("pass: wrote", result.stdout)
            markdown = (root / "report.md").read_text(encoding="utf-8")
            self.assertIn("Analysis engine:", markdown)
            self.assertIn("## Timing", markdown)

    def test_cli_fails_empty_extraction_unless_no_fail(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            light_path = root / "light.png"
            dark_path = root / "dark.png"
            output_path = root / "out.png"
            json_path = root / "report.json"
            report_path = root / "report.md"
            Image.new("RGBA", (2, 2), (255, 255, 255, 255)).save(light_path)
            Image.new("RGBA", (2, 2), (0, 0, 0, 255)).save(dark_path)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--light",
                    str(light_path),
                    "--dark",
                    str(dark_path),
                    "--output",
                    str(output_path),
                    "--json-output",
                    str(json_path),
                    "--report",
                    str(report_path),
                ],
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 1)
            report = json.loads(json_path.read_text(encoding="utf-8"))
            self.assertEqual(report["verdict"], "fail")
            self.assertIn("no visible alpha pixels", report["problems"][0])
            self.assertIn("Verdict: **fail**", report_path.read_text(encoding="utf-8"))

            no_fail_result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--light",
                    str(light_path),
                    "--dark",
                    str(dark_path),
                    "--output",
                    str(root / "out-no-fail.png"),
                    "--no-fail",
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(no_fail_result.returncode, 0, no_fail_result.stdout + no_fail_result.stderr)


if __name__ == "__main__":
    unittest.main()
