import json
import subprocess
import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/assets/render_ui_asset_edge_proof.py"


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
            self.assertGreater(report["counts"]["total"], 0)
            self.assertGreater(report["counts"]["visible"], 0)
            self.assertGreater(report["counts"]["transparent_rgb"], 0)
            self.assertGreater(report["counts"]["reasons"]["green_screen_spill"], 0)
            self.assertEqual(report["rows"][0]["asset_id"], "icon_test")
            markdown = markdown_output.read_text(encoding="utf-8")
            self.assertIn("Total bad marks", markdown)
            self.assertIn("green_screen_spill", markdown)
            self.assertIn("edge proof marks", result.stdout)

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


if __name__ == "__main__":
    unittest.main()
