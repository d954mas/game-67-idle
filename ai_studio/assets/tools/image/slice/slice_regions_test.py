import json
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[5]
SCRIPT = ROOT / "ai_studio/assets/tools/image/slice/slice_regions.py"


class SliceRegionsTest(unittest.TestCase):
    def test_slices_regions_and_builds_review_zip(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "sheet.png"
            regions = root / "regions.json"
            output_dir = root / "out"
            review = root / "out" / "review_sheet.png"
            manifest = root / "out" / "manifest.json"
            archive = root / "out" / "regions.zip"

            image = Image.new("RGBA", (32, 20), (255, 0, 255, 255))
            for y in range(2, 8):
                for x in range(3, 9):
                    image.putpixel((x, y), (220, 40, 30, 255))
            for y in range(10, 18):
                for x in range(20, 30):
                    image.putpixel((x, y), (40, 90, 220, 255))
            image.save(source)
            regions.write_text(
                json.dumps(
                    {
                        "schema": "ai_studio.raster2d.region_review.v1",
                        "regions": [
                            {"id": "region_001", "rect": [3, 2, 6, 6]},
                            {"id": "region_002", "rect": [20, 10, 10, 8]},
                        ],
                    }
                ),
                encoding="utf-8",
            )

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--regions",
                    str(regions),
                    "--output-dir",
                    str(output_dir),
                    "--prefix",
                    "sheet",
                    "--review-sheet",
                    str(review),
                    "--manifest-output",
                    str(manifest),
                    "--zip-output",
                    str(archive),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            data = json.loads(manifest.read_text(encoding="utf-8"))
            self.assertEqual(data["schema"], "ai_studio.raster2d.slices.v1")
            self.assertEqual(data["slice_count"], 2)
            self.assertTrue((output_dir / "slices" / "sheet_region_001.png").exists())
            self.assertTrue(review.exists())
            with zipfile.ZipFile(archive) as zipped:
                self.assertEqual(
                    sorted(zipped.namelist()),
                    [
                        "manifest.json",
                        "regions.json",
                        "review_sheet.png",
                        "slices/sheet_region_001.png",
                        "slices/sheet_region_002.png",
                    ],
                )

    def test_polygon_region_masks_pixels_outside_polygon(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "sheet.png"
            regions = root / "regions.json"
            output_dir = root / "out"
            manifest = root / "out" / "manifest.json"

            image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
            for y in range(2, 10):
                for x in range(2, 10):
                    image.putpixel((x, y), (220, 40, 30, 255))
            image.save(source)
            regions.write_text(
                json.dumps(
                    {
                        "schema": "ai_studio.raster2d.region_review.v1",
                        "regions": [
                            {
                                "id": "triangle",
                                "rect": [2, 2, 8, 8],
                                "polygon": [[2, 2], [10, 2], [2, 10]],
                            },
                        ],
                    }
                ),
                encoding="utf-8",
            )

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--regions",
                    str(regions),
                    "--output-dir",
                    str(output_dir),
                    "--prefix",
                    "sheet",
                    "--manifest-output",
                    str(manifest),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            crop = Image.open(output_dir / "slices" / "sheet_triangle.png").convert("RGBA")
            self.assertEqual(crop.getpixel((1, 1))[3], 255)
            self.assertEqual(crop.getpixel((7, 7))[3], 0)
            data = json.loads(manifest.read_text(encoding="utf-8"))
            self.assertEqual(data["slices"][0]["polygon"], [[2, 2], [10, 2], [2, 10]])

    def test_region_names_drive_export_filenames_without_overwriting_duplicates(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "sheet.png"
            regions = root / "regions.json"
            output_dir = root / "out"
            manifest = root / "out" / "manifest.json"
            archive = root / "out" / "regions.zip"

            image = Image.new("RGBA", (32, 16), (0, 0, 0, 0))
            for y in range(2, 8):
                for x in range(2, 8):
                    image.putpixel((x, y), (220, 40, 30, 255))
                for x in range(18, 24):
                    image.putpixel((x, y), (40, 90, 220, 255))
            image.save(source)
            regions.write_text(
                json.dumps(
                    {
                        "schema": "ai_studio.raster2d.region_review.v1",
                        "regions": [
                            {"id": "region_001", "name": "Sword Button", "rect": [2, 2, 6, 6]},
                            {"id": "region_002", "name": "Sword Button", "rect": [18, 2, 6, 6]},
                        ],
                    }
                ),
                encoding="utf-8",
            )

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--regions",
                    str(regions),
                    "--output-dir",
                    str(output_dir),
                    "--prefix",
                    "sheet",
                    "--manifest-output",
                    str(manifest),
                    "--zip-output",
                    str(archive),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue((output_dir / "slices" / "sheet_Sword_Button.png").exists())
            self.assertTrue((output_dir / "slices" / "sheet_Sword_Button_002.png").exists())
            data = json.loads(manifest.read_text(encoding="utf-8"))
            self.assertEqual(data["slices"][0]["name"], "Sword Button")
            self.assertEqual(data["slices"][0]["file"], "sheet_Sword_Button.png")
            self.assertEqual(data["slices"][1]["name"], "Sword Button")
            self.assertEqual(data["slices"][1]["file"], "sheet_Sword_Button_002.png")
            with zipfile.ZipFile(archive) as zipped:
                self.assertIn("slices/sheet_Sword_Button.png", zipped.namelist())
                self.assertIn("slices/sheet_Sword_Button_002.png", zipped.namelist())

    def test_key_matte_alpha_is_default_for_each_region(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "sheet.png"
            regions = root / "regions.json"
            output_dir = root / "out"
            manifest = root / "out" / "manifest.json"

            image = Image.new("RGBA", (16, 16), (255, 0, 255, 255))
            for y in range(5, 11):
                for x in range(5, 11):
                    image.putpixel((x, y), (220, 40, 30, 255))
            image.save(source)
            regions.write_text(
                json.dumps(
                    {
                        "schema": "ai_studio.raster2d.region_review.v1",
                        "regions": [
                            {"id": "button", "rect": [3, 3, 10, 10]},
                        ],
                    }
                ),
                encoding="utf-8",
            )

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--regions",
                    str(regions),
                    "--output-dir",
                    str(output_dir),
                    "--prefix",
                    "sheet",
                    "--key-color",
                    "#ff00ff",
                    "--manifest-output",
                    str(manifest),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            crop = Image.open(output_dir / "slices" / "sheet_button.png").convert("RGBA")
            self.assertEqual(crop.getpixel((0, 0))[3], 0)
            self.assertEqual(crop.getpixel((5, 5))[3], 255)
            data = json.loads(manifest.read_text(encoding="utf-8"))
            self.assertEqual(data["slices"][0]["alpha"]["mode"], "key_matte")
            self.assertEqual(data["slices"][0]["alpha"]["status"], "applied")

    def test_generation_alpha_mode_marks_region_without_key_matte(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "sheet.png"
            regions = root / "regions.json"
            output_dir = root / "out"
            manifest = root / "out" / "manifest.json"

            image = Image.new("RGBA", (16, 16), (255, 0, 255, 255))
            for y in range(5, 11):
                for x in range(5, 11):
                    image.putpixel((x, y), (220, 40, 30, 255))
            image.save(source)
            regions.write_text(
                json.dumps(
                    {
                        "schema": "ai_studio.raster2d.region_review.v1",
                        "regions": [
                            {"id": "soft_glow", "rect": [3, 3, 10, 10], "alpha": {"mode": "generation"}},
                        ],
                    }
                ),
                encoding="utf-8",
            )

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--regions",
                    str(regions),
                    "--output-dir",
                    str(output_dir),
                    "--prefix",
                    "sheet",
                    "--key-color",
                    "#ff00ff",
                    "--manifest-output",
                    str(manifest),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            crop = Image.open(output_dir / "slices" / "sheet_soft_glow.png").convert("RGBA")
            self.assertEqual(crop.getpixel((0, 0)), (255, 0, 255, 255))
            data = json.loads(manifest.read_text(encoding="utf-8"))
            self.assertEqual(data["slices"][0]["alpha"]["mode"], "generation")
            self.assertEqual(data["slices"][0]["alpha"]["status"], "needs_generation")


if __name__ == "__main__":
    unittest.main()
