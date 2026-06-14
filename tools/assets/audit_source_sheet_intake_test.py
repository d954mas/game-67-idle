import json
import tempfile
import unittest
from pathlib import Path
from subprocess import run

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/assets/audit_source_sheet_intake.py"


class SourceSheetIntakeAuditTests(unittest.TestCase):
    def test_passes_separated_components(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "sheet.png"
            image = Image.new("RGBA", (256, 128), (255, 0, 255, 255))
            draw = ImageDraw.Draw(image)
            draw.rectangle((32, 32, 88, 88), fill=(80, 60, 40, 255))
            draw.rectangle((160, 32, 216, 88), fill=(80, 60, 40, 255))
            image.save(source)
            result = run(
                [
                    "python",
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--min-components",
                    "2",
                    "--min-gutter",
                    "48",
                    "--min-border",
                    "24",
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("pass: 2 component", result.stdout)

    def test_rejects_touching_border_and_tight_gutter(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "sheet.png"
            image = Image.new("RGBA", (128, 96), (255, 0, 255, 255))
            draw = ImageDraw.Draw(image)
            draw.rectangle((0, 20, 40, 60), fill=(80, 60, 40, 255))
            draw.rectangle((48, 20, 88, 60), fill=(80, 60, 40, 255))
            image.save(source)
            result = run(
                [
                    "python",
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--min-components",
                    "2",
                    "--min-gutter",
                    "24",
                    "--min-border",
                    "12",
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("border gap", result.stdout)
            self.assertIn("closest component gap", result.stdout)

    def test_rejects_exact_key_color_inside_art_component(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "sheet.png"
            report = Path(tmp) / "report.json"
            image = Image.new("RGBA", (128, 96), (255, 0, 255, 255))
            draw = ImageDraw.Draw(image)
            draw.rectangle((32, 24, 96, 72), fill=(80, 60, 40, 255))
            draw.rectangle((56, 40, 72, 56), fill=(250, 4, 250, 255))
            image.save(source)
            result = run(
                [
                    "python",
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--min-components",
                    "1",
                    "--min-gutter",
                    "24",
                    "--min-border",
                    "16",
                    "--json-output",
                    str(report),
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("exact key-color-like art", result.stdout)
            data = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(data["key_color"], "#ff00ff")
            self.assertEqual(data["key_color_action"], "regenerate_with_next_prompt_key_color")
            self.assertNotEqual(data["next_prompt_key_color"], "#ff00ff")
            self.assertGreater(data["key_color_conflict_count"], 0)

    def test_rejects_large_key_hue_band_inside_art_component(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "sheet.png"
            image = Image.new("RGBA", (128, 96), (255, 0, 255, 255))
            draw = ImageDraw.Draw(image)
            draw.rectangle((32, 24, 96, 72), fill=(80, 60, 40, 255))
            draw.rectangle((40, 32, 88, 64), fill=(150, 55, 155, 255))
            image.save(source)
            result = run(
                [
                    "python",
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--min-components",
                    "1",
                    "--min-gutter",
                    "24",
                    "--min-border",
                    "16",
                    "--max-key-hue-conflict-ratio",
                    "0.10",
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("key/halo hue conflict ratio", result.stdout)

    def test_scores_candidate_key_colors_against_component_palette(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "sheet.png"
            report = Path(tmp) / "report.json"
            image = Image.new("RGBA", (160, 96), (255, 0, 255, 255))
            draw = ImageDraw.Draw(image)
            draw.rectangle((32, 24, 128, 72), fill=(80, 60, 40, 255))
            draw.rectangle((64, 36, 104, 60), fill=(20, 180, 30, 255))
            image.save(source)
            result = run(
                [
                    "python",
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--min-components",
                    "1",
                    "--min-border",
                    "16",
                    "--candidate-key-colors",
                    "#00ff00,#00ffff,#ffff00",
                    "--json-output",
                    str(report),
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            data = json.loads(report.read_text(encoding="utf-8"))
            scores = {item["key_color"]: item for item in data["candidate_key_scores"]}
            self.assertGreater(scores["#00ff00"]["hue_band_px"], 0)
            self.assertNotEqual(data["suggested_key_color"], "#00ff00")
            self.assertEqual(data["key_color_action"], "keep_current_key_color")
            self.assertEqual(data["next_prompt_key_color"], "#ff00ff")

    def test_merges_small_satellite_fragments_without_hiding_tight_icons(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "sheet.png"
            image = Image.new("RGBA", (192, 128), (255, 0, 255, 255))
            draw = ImageDraw.Draw(image)
            draw.rectangle((48, 40, 96, 88), fill=(80, 60, 40, 255))
            draw.rectangle((108, 44, 116, 52), fill=(80, 60, 40, 255))
            image.save(source)
            result = run(
                [
                    "python",
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--min-components",
                    "1",
                    "--min-gutter",
                    "24",
                    "--min-border",
                    "24",
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("pass: 1 component", result.stdout)

            tight = Path(tmp) / "tight.png"
            image = Image.new("RGBA", (192, 128), (255, 0, 255, 255))
            draw = ImageDraw.Draw(image)
            draw.rectangle((32, 40, 80, 88), fill=(80, 60, 40, 255))
            draw.rectangle((92, 40, 140, 88), fill=(80, 60, 40, 255))
            image.save(tight)
            result = run(
                [
                    "python",
                    str(SCRIPT),
                    "--source",
                    str(tight),
                    "--min-components",
                    "2",
                    "--min-gutter",
                    "24",
                    "--min-border",
                    "24",
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("closest component gap", result.stdout)

    def test_diagonal_components_use_true_edge_distance(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "sheet.png"
            image = Image.new("RGBA", (192, 192), (255, 0, 255, 255))
            draw = ImageDraw.Draw(image)
            draw.rectangle((32, 32, 96, 72), fill=(80, 60, 40, 255))
            draw.rectangle((103, 120, 167, 160), fill=(80, 60, 40, 255))
            image.save(source)
            result = run(
                [
                    "python",
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--min-components",
                    "2",
                    "--min-gutter",
                    "24",
                    "--min-border",
                    "24",
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_profile_writes_stage_timings(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "sheet.png"
            report = Path(tmp) / "report.json"
            markdown = Path(tmp) / "report.md"
            image = Image.new("RGBA", (160, 96), (255, 0, 255, 255))
            draw = ImageDraw.Draw(image)
            draw.rectangle((32, 24, 128, 72), fill=(80, 60, 40, 255))
            image.save(source)
            result = run(
                [
                    "python",
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--min-components",
                    "1",
                    "--min-border",
                    "16",
                    "--json-output",
                    str(report),
                    "--report",
                    str(markdown),
                    "--profile",
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("profile: slowest stage", result.stdout)
            data = json.loads(report.read_text(encoding="utf-8"))
            self.assertIn(data["analysis_engine"], {"numpy", "python"})
            self.assertIn("timing_ms", data)
            self.assertIn("find_components", data["timing_ms"])
            self.assertIn("candidate_key_scores", data["timing_ms"])
            self.assertIn("total", data["timing_ms"])
            markdown_text = markdown.read_text(encoding="utf-8")
            self.assertIn("analysis_engine:", markdown_text)
            self.assertIn("## Timing", markdown_text)


if __name__ == "__main__":
    unittest.main()
