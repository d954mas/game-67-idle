import json
import subprocess
import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/assets/render_ui_asset_edge_proof.py"


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
                proof_rgba = proof.convert("RGBA")
            pixels = proof_rgba.load()
            red_marks = 0
            for y in range(proof_rgba.height):
                for x in range(proof_rgba.width):
                    red, green, blue, alpha = pixels[x, y]
                    if red > 240 and green < 80 and blue < 80 and alpha == 255:
                        red_marks += 1
            self.assertGreater(red_marks, 0)
            self.assertIn("wrote edge proof", result.stdout)

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
