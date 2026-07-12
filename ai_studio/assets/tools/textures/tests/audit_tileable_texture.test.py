import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[5]
SCRIPT = ROOT / "ai_studio" / "assets" / "tools" / "textures" / "audit_tileable_texture.py"


class AuditTileableTextureTest(unittest.TestCase):
    def test_passes_seamless_texture_and_writes_preview(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            source = tmp_path / "flat.png"
            preview = tmp_path / "preview.png"
            report = tmp_path / "report.md"
            json_output = tmp_path / "audit.json"
            Image.new("RGBA", (8, 8), (32, 96, 160, 255)).save(source)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--preview",
                    str(preview),
                    "--report",
                    str(report),
                    "--json-output",
                    str(json_output),
                    "--max-mean-edge-delta",
                    "0",
                    "--max-edge-delta",
                    "0",
                ],
                cwd=ROOT,
                check=True,
                text=True,
                capture_output=True,
            )

            payload = json.loads(result.stdout)
            self.assertEqual(payload["verdict"], "pass")
            with Image.open(preview) as preview_image:
                self.assertEqual(preview_image.size, (16, 16))
            self.assertTrue(report.exists())
            self.assertTrue(json_output.exists())

    def test_fails_visible_edge_mismatch(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            source = tmp_path / "mismatch.png"
            image = Image.new("RGBA", (8, 8), (0, 0, 0, 255))
            for y in range(8):
                image.putpixel((7, y), (255, 255, 255, 255))
            image.save(source)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--source",
                    str(source),
                    "--max-mean-edge-delta",
                    "8",
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )

            self.assertNotEqual(result.returncode, 0)
            payload = json.loads(result.stdout)
            self.assertEqual(payload["verdict"], "fail")
            self.assertIn("mean edge delta", payload["problems"][0])


if __name__ == "__main__":
    unittest.main()
