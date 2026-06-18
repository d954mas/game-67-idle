import json
import importlib.util
import tempfile
import unittest
from pathlib import Path
from subprocess import run

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "tools/assets/intake/audit_source_sheet_intake.py"


def load_intake_module():
    spec = importlib.util.spec_from_file_location("audit_source_sheet_intake_module", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


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
            self.assertGreater(data["problem_summary"]["components_with_exact_key_conflict"], 0)
            self.assertEqual(data["blocking_reasons"][0]["code"], "key_color_conflict")
            self.assertEqual(data["blocking_reasons"][0]["action"], "regenerate_source_sheet_with_safer_key_color")
            self.assertEqual(data["recommended_next_step"]["action"], "regenerate_source_sheet_with_safer_key_color")
            self.assertEqual(data["recommended_next_step"]["key_color"], data["next_prompt_key_color"])

    def test_rejects_exact_key_color_hole_inside_component_bbox(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "sheet.png"
            report = Path(tmp) / "report.json"
            image = Image.new("RGBA", (128, 96), (255, 0, 255, 255))
            draw = ImageDraw.Draw(image)
            draw.rectangle((32, 24, 96, 72), fill=(80, 60, 40, 255))
            draw.rectangle((56, 40, 72, 56), fill=(255, 0, 255, 255))
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
            self.assertGreater(data["components"][0]["exact_key_conflict_px"], 0)

    def test_recommends_dual_plate_when_no_safer_key_candidate_exists(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "sheet.png"
            report = Path(tmp) / "report.json"
            markdown = Path(tmp) / "report.md"
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
                    "--candidate-key-colors",
                    "#ff00ff",
                    "--json-output",
                    str(report),
                    "--report",
                    str(markdown),
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertNotEqual(result.returncode, 0)
            data = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(data["key_color_action"], "split_preserve_or_dual_plate_alpha")
            self.assertIsNone(data["next_prompt_key_color"])
            self.assertEqual(data["recommended_next_step"]["action"], "split_preserve_or_dual_plate_alpha")
            self.assertGreater(data["problem_summary"]["components_with_key_hue_conflict"], 0)
            self.assertEqual(data["blocking_reasons"][0]["code"], "key_color_conflict")
            self.assertEqual(data["blocking_reasons"][0]["action"], "split_preserve_or_dual_plate_alpha")
            markdown_text = markdown.read_text(encoding="utf-8")
            self.assertIn("## Problem Summary", markdown_text)
            self.assertIn("## Blocking Reasons", markdown_text)
            self.assertIn("## Recommended Next Step", markdown_text)
            self.assertIn("- action: split_preserve_or_dual_plate_alpha", markdown_text)

    def test_component_pixel_offsets_do_not_leak_to_json_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "sheet.png"
            report = Path(tmp) / "report.json"
            image = Image.new("RGBA", (128, 96), (255, 0, 255, 255))
            draw = ImageDraw.Draw(image)
            draw.rectangle((32, 24, 96, 72), fill=(80, 60, 40, 255))
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
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            data = json.loads(report.read_text(encoding="utf-8"))
            self.assertTrue(data["components"])
            for component in data["components"]:
                self.assertFalse(any(key.startswith("_") for key in component))

    def test_numpy_components_use_compact_pixel_runs_for_metrics(self):
        module = load_intake_module()
        if module.np is None:
            self.skipTest("numpy is unavailable")
        image = Image.new("RGBA", (12, 8), (255, 0, 255, 255))
        draw = ImageDraw.Draw(image)
        draw.rectangle((2, 2, 5, 4), fill=(80, 60, 40, 255))
        array = module.np.asarray(image.convert("RGBA"))

        components = module.find_components_numpy(image, (255, 0, 255), 0, array)
        self.assertEqual(len(components), 1)
        self.assertIn("_pixel_runs", components[0])
        self.assertNotIn("_pixel_offsets", components[0])

        red, green, blue = module.add_key_conflict_metrics(image, components, (255, 0, 255), 0, array)
        self.assertEqual(int(red.size), 12)
        self.assertEqual(int(module.np.count_nonzero(red == 80)), 12)
        self.assertEqual(components[0]["visible_px"], 12)
        self.assertEqual(components[0]["exact_key_conflict_px"], 0)
        self.assertEqual(int(green.size), 12)
        self.assertEqual(int(blue.size), 12)

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
            self.assertEqual(data["recommended_next_step"]["action"], "slice_ready")

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

    def test_merges_zero_gap_fragments_into_large_component(self):
        module = load_intake_module()
        components = [
            {
                "id": "component_1",
                "bbox": [0, 0, 200, 200],
                "area_px": 40000,
                "_pixel_offsets": [0],
            }
        ]
        for index in range(80):
            x = 10 + (index % 10) * 12
            y = 10 + (index // 10) * 12
            components.append(
                {
                    "id": f"component_{index + 2}",
                    "bbox": [x, y, 4, 4],
                    "area_px": 16,
                    "_pixel_offsets": [index + 1],
                }
            )

        merged = module.merge_small_fragments(components, distance=24, max_fragment_ratio=0.2)

        self.assertEqual(len(merged), 1)
        self.assertEqual(merged[0]["bbox"], [0, 0, 200, 200])
        self.assertEqual(merged[0]["area_px"], 40000 + 80 * 16)
        self.assertEqual(len(merged[0]["merged_from"]), 81)

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
            self.assertIn("recommended_next_step: slice_ready", markdown_text)
            self.assertIn("## Problem Summary", markdown_text)
            self.assertIn("## Blocking Reasons", markdown_text)
            self.assertIn("- none", markdown_text)
            self.assertIn("## Recommended Next Step", markdown_text)
            self.assertIn("## Timing", markdown_text)

    def test_profile_output_keeps_intake_report_stable(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "sheet.png"
            report = Path(tmp) / "report.json"
            markdown = Path(tmp) / "report.md"
            profile = Path(tmp) / "tmp/profile/intake-profile.json"
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
                    "--profile-output",
                    str(profile),
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("wrote profile telemetry:", result.stdout)
            self.assertIn("profile: slowest stage", result.stdout)
            data = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(data["status"], "pass")
            self.assertNotIn("timing_ms", data)
            markdown_text = markdown.read_text(encoding="utf-8")
            self.assertNotIn("## Timing", markdown_text)
            profile_data = json.loads(profile.read_text(encoding="utf-8"))
            self.assertEqual(profile_data["schema"], "game.source_sheet_intake_profile")
            self.assertEqual(profile_data["status"], "pass")
            self.assertEqual(profile_data["recommended_next_step"]["action"], "slice_ready")
            self.assertIn("timing_ms", profile_data)
            self.assertIn("find_components", profile_data["timing_ms"])


if __name__ == "__main__":
    unittest.main()
