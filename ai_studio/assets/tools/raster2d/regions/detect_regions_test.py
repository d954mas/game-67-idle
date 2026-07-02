import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[5]
SCRIPT = ROOT / "ai_studio/assets/tools/raster2d/regions/detect_regions.py"


class DetectRegionsTest(unittest.TestCase):
    def test_detects_regions_with_padding_and_ignores_noise(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "sheet.png"
            output = root / "regions.json"
            image = Image.new("RGBA", (80, 50), (255, 0, 255, 255))
            for y in range(10, 20):
                for x in range(12, 22):
                    image.putpixel((x, y), (220, 40, 30, 255))
            for y in range(26, 36):
                for x in range(44, 56):
                    image.putpixel((x, y), (40, 90, 220, 255))
            image.putpixel((70, 5), (255, 255, 255, 255))
            image.save(source)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--key-color",
                    "#ff00ff",
                    "--key-tolerance",
                    "0",
                    "--min-area",
                    "8",
                    "--padding",
                    "2",
                    "--json-output",
                    str(output),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            data = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(data["schema"], "ai_studio.raster2d.detected_regions.v1")
            self.assertEqual(data["region_count"], 2)
            self.assertEqual([region["id"] for region in data["regions"]], ["region_001", "region_002"])
            self.assertEqual(data["regions"][0]["content_bbox"], [12, 10, 10, 10])
            self.assertEqual(data["regions"][0]["rect"], [10, 8, 14, 14])
            self.assertEqual(data["regions"][1]["content_bbox"], [44, 26, 12, 10])
            self.assertEqual(data["regions"][1]["rect"], [42, 24, 16, 14])

    def test_merges_close_fragments_when_requested(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "sheet.png"
            output = root / "regions.json"
            image = Image.new("RGBA", (48, 32), (255, 0, 255, 255))
            for y in range(10, 16):
                for x in range(8, 14):
                    image.putpixel((x, y), (20, 180, 80, 255))
            for y in range(10, 16):
                for x in range(17, 23):
                    image.putpixel((x, y), (20, 180, 80, 255))
            image.save(source)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--key-color",
                    "#ff00ff",
                    "--min-area",
                    "8",
                    "--merge-distance",
                    "3",
                    "--json-output",
                    str(output),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            data = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(data["region_count"], 1)
            self.assertEqual(data["regions"][0]["content_bbox"], [8, 10, 15, 6])


if __name__ == "__main__":
    unittest.main()
