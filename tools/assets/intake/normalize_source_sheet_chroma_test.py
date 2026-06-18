import tempfile
import unittest
from pathlib import Path
from subprocess import run

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "tools/assets/intake/normalize_source_sheet_chroma.py"
AUDIT = ROOT / "tools/assets/intake/audit_source_sheet_intake.py"


class NormalizeSourceSheetChromaTests(unittest.TestCase):
    def test_normalizes_border_connected_background_for_strict_audit(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            source = tmp_path / "source.png"
            output = tmp_path / "normalized.png"
            image = Image.new("RGBA", (256, 128), (250, 0, 250, 255))
            draw = ImageDraw.Draw(image)
            draw.rectangle((32, 32, 88, 88), fill=(80, 60, 40, 255))
            draw.rectangle((160, 32, 216, 88), fill=(80, 60, 40, 255))
            image.save(source)

            result = run(
                ["python", str(SCRIPT), "--source", str(source), "--output", str(output), "--key-tolerance", "16"],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            audit = run(
                [
                    "python",
                    str(AUDIT),
                    "--source",
                    str(output),
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
            self.assertEqual(audit.returncode, 0, audit.stdout + audit.stderr)


if __name__ == "__main__":
    unittest.main()
