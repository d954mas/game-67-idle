#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/assets/audit_generated_ui_assets.py"


class GeneratedUiAssetAuditTest(unittest.TestCase):
    def run_audit(self, root: Path, manifest: dict, *extra_args: str) -> subprocess.CompletedProcess[str]:
        manifest_path = root / "manifest.json"
        manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
        return subprocess.run(
            ["python", str(SCRIPT), "--crop-manifest", str(manifest_path), *extra_args],
            cwd=root,
            text=True,
            capture_output=True,
            check=False,
        )

    def write_manifest(self, output: str, extra: dict | None = None) -> dict:
        crop = {
            "id": "icon_test",
            "kind": "icon",
            "rect": [0, 0, 32, 32],
            "output": output,
            "semantic_role": "test",
            "size_class": "32px source",
            "trim_padding": 6,
            "isolate_component": "center",
        }
        if extra:
            crop.update(extra)
        return {
            "schema": "game.art_crop_manifest",
            "version": 1,
            "sources": [{"id": "source", "path": "source.png", "crops": [crop]}],
        }

    def test_clean_icon_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.ellipse((8, 8, 23, 23), fill=(220, 40, 40, 255))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png"))
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("pass: checked 1", result.stdout)

    def test_profile_writes_timing_report(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.ellipse((8, 8, 23, 23), fill=(220, 40, 40, 255))
            image.save(out_dir / "icon.png")

            result = self.run_audit(
                root,
                self.write_manifest("assets/icon.png"),
                "--profile",
                "--json-output",
                "audit.json",
                "--report",
                "audit.md",
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("profile: slowest asset `icon_test`", result.stdout)
            report = json.loads((root / "audit.json").read_text(encoding="utf-8"))
            self.assertEqual(report["verdict"], "pass")
            self.assertIn("timing_ms", report)
            self.assertIn("timing_ms", report["assets"][0])
            self.assertIn("edge_green_spill", report["assets"][0]["timing_ms"])
            self.assertIn("transparent_edge_bad_rgb", report["assets"][0]["timing_ms"])
            self.assertIn("transparent_nonzero_rgb", report["assets"][0]["timing_ms"])
            markdown = (root / "audit.md").read_text(encoding="utf-8")
            self.assertIn("## Timing", markdown)

    def test_profile_output_keeps_audit_report_stable(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.ellipse((8, 8, 23, 23), fill=(220, 40, 40, 255))
            image.save(out_dir / "icon.png")

            result = self.run_audit(
                root,
                self.write_manifest("assets/icon.png"),
                "--profile",
                "--profile-output",
                "tmp/profile/generated-ui-audit-profile.json",
                "--json-output",
                "audit.json",
                "--report",
                "audit.md",
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("wrote profile telemetry: tmp/profile/generated-ui-audit-profile.json", result.stdout)
            self.assertIn("profile: slowest asset `icon_test`", result.stdout)
            report = json.loads((root / "audit.json").read_text(encoding="utf-8"))
            self.assertEqual(report["verdict"], "pass")
            self.assertNotIn("timing_ms", report)
            self.assertNotIn("timing_ms", report["assets"][0])
            markdown = (root / "audit.md").read_text(encoding="utf-8")
            self.assertNotIn("## Timing", markdown)
            profile = json.loads((root / "tmp/profile/generated-ui-audit-profile.json").read_text(encoding="utf-8"))
            self.assertEqual(profile["schema"], "game.generated_ui_asset_audit_profile")
            self.assertEqual(profile["verdict"], "pass")
            self.assertIn("timing_ms", profile)
            self.assertIn("timing_ms", profile["assets"][0])

    def test_clipped_icon_fails_padding_check(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((0, 8, 20, 23), fill=(220, 40, 40, 255))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png"))
            self.assertEqual(result.returncode, 1)
            self.assertIn("too close to left edge", result.stdout)

    def test_key_color_edge_fringe_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(220, 40, 40, 255))
            image.putpixel((8, 16), (230, 20, 230, 255))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png"))
            self.assertEqual(result.returncode, 1)
            self.assertIn("key-color edge fringe remains", result.stdout)

    def test_soft_purple_edge_halo_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((8, 16), (130, 8, 96, 255))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png"))
            self.assertEqual(result.returncode, 1)
            self.assertIn("purple edge halo remains", result.stdout)

    def test_dark_one_pixel_purple_edge_halo_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((23, 16), (64, 0, 64, 255))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png"))
            self.assertEqual(result.returncode, 1)
            self.assertIn("purple edge halo remains", result.stdout)

    def test_near_black_purple_edge_halo_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((23, 16), (38, 2, 45, 255))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png"))
            self.assertEqual(result.returncode, 1)
            self.assertIn("purple edge halo remains", result.stdout)

    def test_maroon_magenta_edge_spill_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((16, 8), (128, 48, 72, 255))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png"))
            self.assertEqual(result.returncode, 1)
            self.assertIn("purple edge halo remains", result.stdout)

    def test_dark_magenta_edge_spill_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((23, 16), (55, 20, 45, 255))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png"))
            self.assertEqual(result.returncode, 1)
            self.assertIn("purple edge halo remains", result.stdout)

    def test_transparent_key_rgb_near_edge_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((7, 16), (255, 0, 255, 0))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png"))
            self.assertEqual(result.returncode, 1)
            self.assertIn("transparent edge keeps key/purple/green RGB", result.stdout)

    def test_fully_transparent_nonzero_rgb_fails_even_when_color_is_neutral(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((2, 2), (12, 9, 7, 0))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png"))
            self.assertEqual(result.returncode, 1)
            self.assertIn("fully transparent pixels keep nonzero RGB", result.stdout)

    def test_default_green_screen_edge_spill_fails_without_manifest_key(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((8, 16), (19, 205, 9, 255))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png"))
            self.assertEqual(result.returncode, 1)
            self.assertIn("green-screen edge spill remains", result.stdout)

    def test_default_green_screen_transparent_rgb_fails_without_manifest_key(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((7, 16), (0, 255, 0, 0))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png"))
            self.assertEqual(result.returncode, 1)
            self.assertIn("transparent edge keeps key/purple/green RGB", result.stdout)

    def test_preserve_green_edges_allows_visible_green_but_not_transparent_rgb(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((8, 16), (19, 205, 9, 255))
            image.putpixel((7, 16), (0, 255, 0, 0))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png", {"preserve_green_edges": True}))
            self.assertEqual(result.returncode, 1)
            self.assertIn("fully transparent pixels keep nonzero RGB", result.stdout)

    def test_preserve_green_edges_allows_visible_green_when_transparent_rgb_is_clean(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((8, 16), (19, 205, 9, 255))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png", {"preserve_green_edges": True}))
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_manifest_source_key_green_edge_fringe_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((8, 16), (0, 255, 0, 255))
            image.save(out_dir / "icon.png")

            manifest = self.write_manifest("assets/icon.png")
            manifest["green_screen"] = {"mode": "chroma_key", "key": "#00ff00"}
            result = self.run_audit(root, manifest)
            self.assertEqual(result.returncode, 1)
            self.assertIn("source key edge fringe remains", result.stdout)

    def test_manifest_source_key_green_spill_edge_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((8, 16), (19, 205, 9, 255))
            image.save(out_dir / "icon.png")

            manifest = self.write_manifest("assets/icon.png")
            manifest["green_screen"] = {"mode": "chroma_key", "key": "#00ff00"}
            result = self.run_audit(root, manifest)
            self.assertEqual(result.returncode, 1)
            self.assertIn("source key edge fringe remains", result.stdout)

    def test_manifest_source_key_muted_green_edge_cast_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((8, 16), (57, 88, 5, 255))
            image.save(out_dir / "icon.png")

            manifest = self.write_manifest("assets/icon.png")
            manifest["green_screen"] = {"mode": "chroma_key", "key": "#00ff00"}
            result = self.run_audit(root, manifest)
            self.assertEqual(result.returncode, 1)
            self.assertIn("source key edge fringe remains", result.stdout)

    def test_manifest_source_key_green_transparent_rgb_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(180, 120, 60, 255))
            image.putpixel((7, 16), (0, 255, 0, 0))
            image.save(out_dir / "icon.png")

            manifest = self.write_manifest("assets/icon.png")
            manifest["green_screen"] = {"mode": "chroma_key", "key": "#00ff00"}
            result = self.run_audit(root, manifest)
            self.assertEqual(result.returncode, 1)
            self.assertIn("transparent edge keeps key/purple/green RGB", result.stdout)

    def test_preserve_purple_edges_still_fails_source_key_edge_fringe(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(120, 70, 180, 255))
            image.putpixel((8, 16), (0, 255, 0, 255))
            image.save(out_dir / "icon.png")

            manifest = self.write_manifest("assets/icon.png", {"preserve_purple_edges": True})
            manifest["green_screen"] = {"mode": "chroma_key", "key": "#00ff00"}
            result = self.run_audit(root, manifest)
            self.assertEqual(result.returncode, 1)
            self.assertIn("source key edge fringe remains", result.stdout)

    def test_preserve_purple_edges_still_fails_source_key_transparent_rgb(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(120, 70, 180, 255))
            image.putpixel((7, 16), (0, 255, 0, 0))
            image.save(out_dir / "icon.png")

            manifest = self.write_manifest("assets/icon.png", {"preserve_purple_edges": True})
            manifest["green_screen"] = {"mode": "chroma_key", "key": "#00ff00"}
            result = self.run_audit(root, manifest)
            self.assertEqual(result.returncode, 1)
            self.assertIn("transparent edge keeps key/purple/green RGB", result.stdout)

    def test_preserve_purple_edges_does_not_allow_transparent_rgb(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out_dir = root / "assets"
            out_dir.mkdir()
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 8, 23, 23), fill=(120, 70, 180, 255))
            image.putpixel((7, 16), (64, 0, 64, 0))
            image.save(out_dir / "icon.png")

            result = self.run_audit(root, self.write_manifest("assets/icon.png", {"preserve_purple_edges": True}))
            self.assertEqual(result.returncode, 1)
            self.assertIn("fully transparent pixels keep nonzero RGB", result.stdout)


if __name__ == "__main__":
    unittest.main()
