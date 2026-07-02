import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[5]
SCRIPT = ROOT / "ai_studio/assets/tools/image/bg_fix/normalize_background.py"


class NormalizeBackgroundTest(unittest.TestCase):
    def test_normalizes_only_border_connected_key_like_pixels(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "sheet.png"
            output = root / "normalized.png"
            report = root / "report.json"
            image = Image.new("RGBA", (16, 10), (245, 3, 242, 255))
            for y in range(3, 7):
                for x in range(4, 8):
                    image.putpixel((x, y), (40, 90, 220, 255))
            image.putpixel((5, 5), (246, 2, 241, 255))
            image.save(source)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--output",
                    str(output),
                    "--key-color",
                    "#ff00ff",
                    "--key-tolerance",
                    "16",
                    "--json-output",
                    str(report),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            normalized = Image.open(output).convert("RGBA")
            self.assertEqual(normalized.getpixel((0, 0)), (255, 0, 255, 255))
            self.assertEqual(normalized.getpixel((5, 5)), (246, 2, 241, 255))
            self.assertIn("pass: normalized", result.stdout)
            self.assertTrue(report.exists())

    def test_auto_detects_border_key_color(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "sheet.png"
            output = root / "normalized.png"
            report = root / "report.json"
            image = Image.new("RGBA", (14, 12), (10, 200, 50, 255))
            image.putpixel((0, 3), (12, 198, 52, 255))
            image.putpixel((13, 6), (12, 198, 52, 255))
            for y in range(4, 8):
                for x in range(5, 9):
                    image.putpixel((x, y), (40, 90, 220, 255))
            image.putpixel((6, 6), (12, 198, 52, 255))
            image.save(source)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--output",
                    str(output),
                    "--key-tolerance",
                    "8",
                    "--json-output",
                    str(report),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            normalized = Image.open(output).convert("RGBA")
            self.assertEqual(normalized.getpixel((0, 3)), (10, 200, 50, 255))
            self.assertEqual(normalized.getpixel((6, 6)), (12, 198, 52, 255))
            self.assertIn('"key_color": "#0ac832"', report.read_text(encoding="utf8"))

    def test_none_mode_preserves_whole_image_without_normalizing_border(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "full_image.png"
            output = root / "normalized.png"
            report = root / "report.json"
            image = Image.new("RGBA", (12, 8), (30, 60, 90, 255))
            for x in range(12):
                image.putpixel((x, 0), (10 + x, 30, 200, 255))
                image.putpixel((x, 7), (200, 20 + x, 40, 255))
            image.save(source)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--output",
                    str(output),
                    "--mode",
                    "none",
                    "--json-output",
                    str(report),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(list(Image.open(output).convert("RGBA").getdata()), list(image.getdata()))
            data = report.read_text(encoding="utf8")
            self.assertIn('"mode": "passthrough_no_background"', data)
            self.assertIn('"changed_pixels": 0', data)


if __name__ == "__main__":
    unittest.main()
