import json
import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/assets/render_ui_asset_edge_proof.py"


def load_edge_module():
    spec = importlib.util.spec_from_file_location("render_ui_asset_edge_proof_module", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def count_mark_pixels(image: Image.Image, color_name: str) -> int:
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    marks = 0
    for y in range(rgba.height):
        for x in range(rgba.width):
            red, green, blue, alpha = pixels[x, y]
            if color_name == "red" and red > 240 and green < 80 and blue < 80 and alpha == 255:
                marks += 1
            elif color_name == "yellow" and red > 240 and green > 180 and blue < 80 and alpha == 255:
                marks += 1
    return marks


class RenderUiAssetEdgeProofTests(unittest.TestCase):
    def run_source_key_preserve_purple_case(self, *, alpha: int, x: int) -> tuple[dict, int, int]:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((x, 16), (0, 255, 0, alpha))
            image.save(out_dir / "icon.png")
            manifest = {
                "schema": "game.art_crop_manifest",
                "version": 1,
                "green_screen": {"mode": "chroma_key", "key": "#00ff00"},
                "sources": [
                    {
                        "id": "source",
                        "path": "source.png",
                        "crops": [
                            {
                                "id": "icon_test",
                                "kind": "icon",
                                "rect": [0, 0, 32, 32],
                                "output": "assets/icon.png",
                                "preserve_purple_edges": True,
                            }
                        ],
                    }
                ],
            }
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            output = root / "proof.png"
            json_output = root / "proof.json"

            result = subprocess.run(
                [
                    "python",
                    str(SCRIPT),
                    "--crop-manifest",
                    str(manifest_path),
                    "--output",
                    str(output),
                    "--asset-id",
                    "icon_test",
                    "--side",
                    "right",
                    "--zoom",
                    "3",
                    "--json-output",
                    str(json_output),
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads(json_output.read_text(encoding="utf-8"))
            with Image.open(output) as proof:
                red_marks = count_mark_pixels(proof, "red")
                yellow_marks = count_mark_pixels(proof, "yellow")
        return report, red_marks, yellow_marks

    def test_renders_zoomed_edge_proof_and_marks_bad_pixels(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((23, 16), (64, 0, 64, 255))
            image.save(out_dir / "icon.png")
            manifest = {
                "schema": "game.art_crop_manifest",
                "version": 1,
                "sources": [
                    {
                        "id": "source",
                        "path": "source.png",
                        "crops": [
                            {
                                "id": "icon_test",
                                "kind": "icon",
                                "rect": [0, 0, 32, 32],
                                "output": "assets/icon.png",
                            }
                        ],
                    }
                ],
            }
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            output = root / "proof.png"

            result = subprocess.run(
                [
                    "python",
                    str(SCRIPT),
                    "--crop-manifest",
                    str(manifest_path),
                    "--output",
                    str(output),
                    "--zoom",
                    "3",
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(output.exists())
            with Image.open(output) as proof:
                self.assertGreater(proof.width, 64)
                self.assertGreater(proof.height, 64)
                red_marks = count_mark_pixels(proof, "red")
            self.assertGreater(red_marks, 0)
            self.assertIn("wrote edge proof", result.stdout)

    def test_marks_green_screen_spill_without_manifest_key(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((23, 16), (19, 205, 9, 255))
            image.putpixel((24, 16), (0, 255, 0, 0))
            image.save(out_dir / "icon.png")
            manifest = {
                "schema": "game.art_crop_manifest",
                "version": 1,
                "sources": [
                    {
                        "id": "source",
                        "path": "source.png",
                        "crops": [
                            {
                                "id": "icon_test",
                                "kind": "icon",
                                "rect": [0, 0, 32, 32],
                                "output": "assets/icon.png",
                            }
                        ],
                    }
                ],
            }
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            output = root / "proof.png"

            result = subprocess.run(
                [
                    "python",
                    str(SCRIPT),
                    "--crop-manifest",
                    str(manifest_path),
                    "--output",
                    str(output),
                    "--asset-id",
                    "icon_test",
                    "--side",
                    "right",
                    "--zoom",
                    "3",
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            with Image.open(output) as proof:
                self.assertGreater(count_mark_pixels(proof, "red"), 0)
                self.assertGreater(count_mark_pixels(proof, "yellow"), 0)

    def test_writes_json_and_markdown_reason_counts(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((23, 16), (19, 205, 9, 255))
            image.putpixel((24, 16), (0, 255, 0, 0))
            image.save(out_dir / "icon.png")
            manifest = {
                "schema": "game.art_crop_manifest",
                "version": 1,
                "sources": [
                    {
                        "id": "source",
                        "path": "source.png",
                        "crops": [
                            {
                                "id": "icon_test",
                                "kind": "icon",
                                "rect": [0, 0, 32, 32],
                                "output": "assets/icon.png",
                            }
                        ],
                    }
                ],
            }
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            output = root / "proof.png"
            json_output = root / "proof.json"
            markdown_output = root / "proof.md"

            result = subprocess.run(
                [
                    "python",
                    str(SCRIPT),
                    "--crop-manifest",
                    str(manifest_path),
                    "--output",
                    str(output),
                    "--asset-id",
                    "icon_test",
                    "--side",
                    "right",
                    "--zoom",
                    "3",
                    "--json-output",
                    str(json_output),
                    "--report",
                    str(markdown_output),
                    "--profile",
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads(json_output.read_text(encoding="utf-8"))
            self.assertEqual(report["schema"], "game.ui_asset_edge_proof")
            self.assertEqual(report["image_output"], str(output).replace("\\", "/"))
            self.assertIn(report["analysis_engine"], {"numpy", "python"})
            self.assertGreater(report["counts"]["total"], 0)
            self.assertGreater(report["counts"]["visible"], 0)
            self.assertGreater(report["counts"]["transparent_rgb"], 0)
            self.assertGreater(report["counts"]["reasons"]["green_screen_spill"], 0)
            self.assertEqual(report["rows"][0]["asset_id"], "icon_test")
            self.assertIn("timing_ms", report)
            self.assertGreaterEqual(report["timing_ms"]["total"], 0)
            self.assertIn("asset_timings", report)
            self.assertIn("timing_ms", report["rows"][0])
            markdown = markdown_output.read_text(encoding="utf-8")
            self.assertIn("Total bad marks", markdown)
            self.assertIn("Analysis engine:", markdown)
            self.assertIn("green_screen_spill", markdown)
            self.assertIn("## Timing", markdown)
            self.assertIn("edge proof marks", result.stdout)
            self.assertIn("profile: slowest edge strip", result.stdout)

    def test_preserve_green_edges_suppresses_green_spill_marks(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((23, 16), (19, 205, 9, 255))
            image.putpixel((24, 16), (0, 255, 0, 0))
            image.save(out_dir / "icon.png")
            manifest = {
                "schema": "game.art_crop_manifest",
                "version": 1,
                "sources": [
                    {
                        "id": "source",
                        "path": "source.png",
                        "crops": [
                            {
                                "id": "icon_test",
                                "kind": "icon",
                                "rect": [0, 0, 32, 32],
                                "output": "assets/icon.png",
                                "preserve_green_edges": True,
                            }
                        ],
                    }
                ],
            }
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            output = root / "proof.png"

            result = subprocess.run(
                [
                    "python",
                    str(SCRIPT),
                    "--crop-manifest",
                    str(manifest_path),
                    "--output",
                    str(output),
                    "--asset-id",
                    "icon_test",
                    "--side",
                    "right",
                    "--zoom",
                    "3",
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            with Image.open(output) as proof:
                self.assertEqual(count_mark_pixels(proof, "red"), 0)
                self.assertEqual(count_mark_pixels(proof, "yellow"), 0)

    def test_preserve_purple_edges_still_marks_source_key_spill(self) -> None:
        report, red_marks, _yellow_marks = self.run_source_key_preserve_purple_case(alpha=255, x=23)
        self.assertGreater(report["counts"]["visible"], 0)
        self.assertGreater(report["counts"]["reasons"]["source_key_spill"], 0)
        self.assertGreater(red_marks, 0)

    def test_preserve_purple_edges_still_marks_source_key_transparent_rgb(self) -> None:
        report, _red_marks, yellow_marks = self.run_source_key_preserve_purple_case(alpha=0, x=24)
        self.assertGreater(report["counts"]["transparent_rgb"], 0)
        self.assertGreater(report["counts"]["reasons"]["source_key_spill"], 0)
        self.assertGreater(yellow_marks, 0)

    def test_can_filter_to_one_asset_side(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            for name, color in [("a.png", (180, 120, 60, 255)), ("b.png", (60, 120, 180, 255))]:
                image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
                draw = ImageDraw.Draw(image)
                draw.rectangle((8, 8, 23, 23), fill=color)
                image.save(out_dir / name)
            manifest = {
                "schema": "game.art_crop_manifest",
                "version": 1,
                "sources": [
                    {
                        "id": "source",
                        "path": "source.png",
                        "crops": [
                            {"id": "first", "kind": "icon", "rect": [0, 0, 32, 32], "output": "assets/a.png"},
                            {"id": "second", "kind": "icon", "rect": [0, 0, 32, 32], "output": "assets/b.png"},
                        ],
                    }
                ],
            }
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            output = root / "proof.png"

            result = subprocess.run(
                [
                    "python",
                    str(SCRIPT),
                    "--crop-manifest",
                    str(manifest_path),
                    "--output",
                    str(output),
                    "--asset-id",
                    "second",
                    "--side",
                    "right",
                    "--zoom",
                    "2",
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            with Image.open(output) as proof:
                self.assertLess(proof.height, 220)

    def test_only_problems_keeps_json_coverage_but_renders_focused_sheet(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            clean = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(clean)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            clean.save(out_dir / "clean.png")
            bad = clean.copy()
            bad.putpixel((23, 16), (64, 0, 64, 255))
            bad.save(out_dir / "bad.png")
            manifest = {
                "schema": "game.art_crop_manifest",
                "version": 1,
                "sources": [
                    {
                        "id": "source",
                        "path": "source.png",
                        "crops": [
                            {"id": "clean", "kind": "icon", "rect": [0, 0, 32, 32], "output": "assets/clean.png"},
                            {"id": "bad", "kind": "icon", "rect": [0, 0, 32, 32], "output": "assets/bad.png"},
                        ],
                    }
                ],
            }
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            output = root / "proof.png"
            json_output = root / "proof.json"
            markdown_output = root / "proof.md"

            result = subprocess.run(
                [
                    "python",
                    str(SCRIPT),
                    "--crop-manifest",
                    str(manifest_path),
                    "--output",
                    str(output),
                    "--side",
                    "right",
                    "--zoom",
                    "2",
                    "--json-output",
                    str(json_output),
                    "--report",
                    str(markdown_output),
                    "--only-problems",
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads(json_output.read_text(encoding="utf-8"))
            self.assertTrue(report["only_problems"])
            self.assertEqual(len(report["rows"]), 2)
            self.assertEqual(report["rendered_rows"], 1)
            self.assertEqual(report["omitted_clean_rows"], 1)
            self.assertEqual(report["counts"]["total"], report["rows"][1]["counts"]["total"])
            markdown = markdown_output.read_text(encoding="utf-8")
            self.assertIn("Omitted clean rows: 1", markdown)
            self.assertIn("`bad`", markdown)
            self.assertNotIn("`clean`", markdown)
            with Image.open(output) as proof:
                self.assertLess(proof.height, 180)

    def test_only_problems_skips_clean_strip_image_rendering(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            clean = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(clean)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            clean.save(out_dir / "clean.png")
            manifest = {
                "schema": "game.art_crop_manifest",
                "version": 1,
                "sources": [
                    {
                        "id": "source",
                        "path": "source.png",
                        "crops": [
                            {"id": "clean", "kind": "icon", "rect": [0, 0, 32, 32], "output": "assets/clean.png"},
                        ],
                    }
                ],
            }
            edge = load_edge_module()
            with patch.object(edge, "render_strip_image", side_effect=AssertionError("clean strips should not render")):
                _proof, report = edge.render_edge_proof(
                    manifest,
                    root,
                    2,
                    18,
                    6,
                    True,
                    None,
                    {"right"},
                    False,
                    True,
                )

            self.assertEqual(len(report["rows"]), 1)
            self.assertEqual(report["counts"]["total"], 0)
            self.assertEqual(report["rendered_rows"], 0)
            self.assertEqual(report["omitted_clean_rows"], 1)

    def test_python_fallback_counts_bad_edges_without_numpy(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((23, 16), (64, 0, 64, 255))
            image.save(out_dir / "bad.png")
            manifest = {
                "schema": "game.art_crop_manifest",
                "version": 1,
                "sources": [
                    {
                        "id": "source",
                        "path": "source.png",
                        "crops": [
                            {"id": "bad", "kind": "icon", "rect": [0, 0, 32, 32], "output": "assets/bad.png"},
                        ],
                    }
                ],
            }
            edge = load_edge_module()
            with patch.object(edge, "np", None):
                _proof, report = edge.render_edge_proof(
                    manifest,
                    root,
                    2,
                    18,
                    6,
                    True,
                    None,
                    {"right"},
                    False,
                    True,
                )

            self.assertEqual(report["analysis_engine"], "python")
            self.assertGreater(report["counts"]["total"], 0)
            self.assertEqual(report["rendered_rows"], 1)
            self.assertEqual(report["omitted_clean_rows"], 0)


if __name__ == "__main__":
    unittest.main()
