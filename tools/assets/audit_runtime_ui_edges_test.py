#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "assets" / "audit_runtime_ui_edges.py"


class RuntimeUiEdgeAuditTests(unittest.TestCase):
    def run_audit(self, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["py", "-3.12", str(SCRIPT), *args],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )

    def test_clean_image_passes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            image = Path(tmp) / "clean.png"
            Image.new("RGBA", (8, 8), (20, 80, 20, 255)).save(image)
            result = self.run_audit("--image", str(image), "--family", "purple")
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
            report = json.loads(result.stdout)
            self.assertEqual(report["verdict"], "pass")
            self.assertEqual(report["bad_pixels"], 0)

    def test_purple_pixel_fails_with_sample_coordinates(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            image = Path(tmp) / "bad.png"
            im = Image.new("RGBA", (8, 8), (20, 80, 20, 255))
            im.putpixel((3, 4), (140, 20, 150, 255))
            im.save(image)
            result = self.run_audit("--image", str(image), "--family", "purple")
            self.assertEqual(result.returncode, 1)
            report = json.loads(result.stdout)
            self.assertEqual(report["verdict"], "fail")
            self.assertEqual(report["bad_pixels"], 1)
            self.assertEqual(report["samples"][0]["x"], 3)
            self.assertEqual(report["samples"][0]["y"], 4)

    def test_crop_limits_audit_region(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            image = Path(tmp) / "crop.png"
            im = Image.new("RGBA", (10, 10), (20, 80, 20, 255))
            im.putpixel((1, 1), (140, 20, 150, 255))
            im.putpixel((8, 8), (140, 20, 150, 255))
            im.save(image)
            result = self.run_audit("--image", str(image), "--crop", "6,6,10,10", "--family", "purple")
            self.assertEqual(result.returncode, 1)
            report = json.loads(result.stdout)
            self.assertEqual(report["bad_pixels"], 1)
            self.assertEqual(report["samples"][0]["x"], 8)
            self.assertEqual(report["samples"][0]["y"], 8)

    def test_key_color_family_uses_tolerance(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            image = Path(tmp) / "key.png"
            im = Image.new("RGBA", (4, 4), (20, 80, 20, 255))
            im.putpixel((2, 2), (255, 0, 255, 255))
            im.save(image)
            result = self.run_audit("--image", str(image), "--family", "key", "--key-color", "255,0,255")
            self.assertEqual(result.returncode, 1)
            report = json.loads(result.stdout)
            self.assertEqual(report["bad_pixels"], 1)


if __name__ == "__main__":
    unittest.main()
