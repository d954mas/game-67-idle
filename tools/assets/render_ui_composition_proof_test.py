import json
import subprocess
import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/assets/render_ui_composition_proof.py"


def write_panel(path: Path) -> None:
    image = Image.new("RGBA", (40, 32), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 39, 31), fill=(80, 52, 34, 255))
    draw.rectangle((6, 6, 33, 25), fill=(138, 92, 52, 255))
    draw.rectangle((0, 0, 39, 31), outline=(232, 184, 92, 255), width=3)
    image.save(path)


def write_overlay(path: Path) -> None:
    image = Image.new("RGBA", (14, 14), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse((1, 1, 12, 12), fill=(40, 160, 180, 255), outline=(236, 204, 106, 255), width=2)
    image.save(path)


def write_large_overlay(path: Path) -> None:
    image = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.polygon([(32, 2), (62, 32), (32, 62), (2, 32)], fill=(40, 190, 230, 255), outline=(236, 204, 106, 255))
    image.save(path)


class RenderUiCompositionProofTests(unittest.TestCase):
    def test_renders_slice9_with_overlay_and_runtime_label(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            assets = root / "assets"
            assets.mkdir()
            write_panel(assets / "button.png")
            write_overlay(assets / "gem.png")
            manifest = {
                "schema": "game.asset_manifest",
                "version": 1,
                "assets": [
                    {
                        "id": "button",
                        "kind": "slice9",
                        "path": "assets/button.png",
                        "slice9": {"left": 8, "top": 8, "right": 8, "bottom": 8},
                        "content": {"x": 9, "y": 9, "w": 22, "h": 14},
                        "target_preview_sizes": [[96, 40]],
                        "usage_policy": {"min_size": [80, 36]},
                    },
                    {"id": "gem", "kind": "decor_overlay", "path": "assets/gem.png", "anchor": "top_center"},
                ],
            }
            layout = {
                "schema": "game.ui_composition_proof_layout",
                "version": 1,
                "items": [{"base_id": "button", "size": [96, 40], "label": "Go", "decor_overlays": [{"id": "gem", "anchor": "top_center", "allow_content_overlap": True}]}],
            }
            (root / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            (root / "layout.json").write_text(json.dumps(layout), encoding="utf-8")

            result = subprocess.run(
                [
                    "python",
                    str(SCRIPT),
                    "--asset-manifest",
                    "manifest.json",
                    "--layout",
                    "layout.json",
                    "--output",
                    "proof.png",
                    "--json-output",
                    "proof.json",
                    "--report",
                    "proof.md",
                    "--profile",
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("pass: wrote composition proof", result.stdout)
            self.assertTrue((root / "proof.png").exists())
            report = json.loads((root / "proof.json").read_text(encoding="utf-8"))
            self.assertEqual(report["verdict"], "pass")
            self.assertIn("timing_ms", report)
            self.assertIn("cache_stats", report)
            self.assertIn("timing_ms", report["items"][0])
            self.assertIn("overlays", report["items"][0])
            self.assertIn("profile: slowest composition item", result.stdout)
            markdown = (root / "proof.md").read_text(encoding="utf-8")
            self.assertIn("## Timing", markdown)
            self.assertIn("## Cache", markdown)
            self.assertIn("overlay `gem` source=[14, 14] render=[14, 14] mode=source", markdown)
            with Image.open(root / "proof.png") as proof:
                self.assertGreater(proof.width, 120)
                self.assertGreater(proof.height, 80)

    def test_reuses_cached_slice9_panels_for_repeated_items(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            assets = root / "assets"
            assets.mkdir()
            write_panel(assets / "button.png")
            manifest = {
                "schema": "game.asset_manifest",
                "version": 1,
                "assets": [
                    {
                        "id": "button",
                        "kind": "slice9",
                        "path": "assets/button.png",
                        "slice9": {"left": 8, "top": 8, "right": 8, "bottom": 8},
                        "content": {"x": 9, "y": 9, "w": 22, "h": 14},
                        "target_preview_sizes": [[96, 40]],
                    }
                ],
            }
            layout = {
                "schema": "game.ui_composition_proof_layout",
                "version": 1,
                "items": [
                    {"base_id": "button", "size": [96, 40], "label": "One"},
                    {"base_id": "button", "size": [96, 40], "label": "Two"},
                    {"base_id": "button", "size": [120, 40], "label": "Three"},
                ],
            }
            (root / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            (root / "layout.json").write_text(json.dumps(layout), encoding="utf-8")

            result = subprocess.run(
                [
                    "python",
                    str(SCRIPT),
                    "--asset-manifest",
                    "manifest.json",
                    "--layout",
                    "layout.json",
                    "--output",
                    "proof.png",
                    "--json-output",
                    "proof.json",
                    "--profile",
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads((root / "proof.json").read_text(encoding="utf-8"))
            self.assertEqual(report["verdict"], "pass")
            self.assertGreaterEqual(report["cache_stats"]["image_hits"], 1)
            self.assertGreaterEqual(report["cache_stats"]["panel_hits"], 1)
            self.assertEqual(report["cache_stats"]["panel_misses"], 2)
            self.assertGreater(report["cache_stats"]["resized_tile_hits"], 0)
            self.assertGreater(report["cache_stats"]["resized_tile_misses"], 0)

    def test_resizes_overlay_to_declared_runtime_size(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            assets = root / "assets"
            assets.mkdir()
            write_panel(assets / "button.png")
            write_large_overlay(assets / "large_gem.png")
            manifest = {
                "schema": "game.asset_manifest",
                "version": 1,
                "assets": [
                    {
                        "id": "button",
                        "kind": "slice9",
                        "path": "assets/button.png",
                        "slice9": {"left": 8, "top": 8, "right": 8, "bottom": 8},
                        "content": {"x": 14, "y": 9, "w": 18, "h": 14},
                        "target_preview_sizes": [[96, 40]],
                    },
                    {"id": "large_gem", "kind": "decor_overlay", "path": "assets/large_gem.png", "anchor": "left_mid"},
                ],
            }
            layout = {
                "schema": "game.ui_composition_proof_layout",
                "version": 1,
                "items": [
                    {
                        "base_id": "button",
                        "size": [96, 40],
                        "label": "Go",
                        "decor_overlays": [
                            {
                                "id": "large_gem",
                                "anchor": "left_mid",
                                "size": [16, 16],
                                "offset": [2, 0],
                                "allow_content_overlap": True,
                            }
                        ],
                    }
                ],
            }
            (root / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            (root / "layout.json").write_text(json.dumps(layout), encoding="utf-8")

            result = subprocess.run(
                [
                    "python",
                    str(SCRIPT),
                    "--asset-manifest",
                    "manifest.json",
                    "--layout",
                    "layout.json",
                    "--output",
                    "proof.png",
                    "--json-output",
                    "proof.json",
                    "--profile",
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads((root / "proof.json").read_text(encoding="utf-8"))
            overlay = report["items"][0]["overlays"][0]
            self.assertEqual(overlay["source_size"], [64, 64])
            self.assertEqual(overlay["render_size"], [16, 16])
            self.assertEqual(overlay["resize"]["mode"], "size")
            self.assertEqual(overlay["rect"], [2, 12, 16, 16])
            self.assertEqual(report["cache_stats"]["overlay_resize_misses"], 1)

    def test_fails_when_overlay_resize_policy_is_ambiguous(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            assets = root / "assets"
            assets.mkdir()
            write_panel(assets / "button.png")
            write_large_overlay(assets / "large_gem.png")
            manifest = {
                "schema": "game.asset_manifest",
                "version": 1,
                "assets": [
                    {
                        "id": "button",
                        "kind": "slice9",
                        "path": "assets/button.png",
                        "slice9": {"left": 8, "top": 8, "right": 8, "bottom": 8},
                        "content": {"x": 9, "y": 9, "w": 22, "h": 14},
                        "target_preview_sizes": [[96, 40]],
                    },
                    {"id": "large_gem", "kind": "decor_overlay", "path": "assets/large_gem.png", "anchor": "left_mid"},
                ],
            }
            layout = {
                "schema": "game.ui_composition_proof_layout",
                "version": 1,
                "items": [
                    {
                        "base_id": "button",
                        "size": [96, 40],
                        "label": "Go",
                        "decor_overlays": [{"id": "large_gem", "anchor": "left_mid", "size": [16, 16], "scale": 0.5}],
                    }
                ],
            }
            (root / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            (root / "layout.json").write_text(json.dumps(layout), encoding="utf-8")

            result = subprocess.run(
                [
                    "python",
                    str(SCRIPT),
                    "--asset-manifest",
                    "manifest.json",
                    "--layout",
                    "layout.json",
                    "--output",
                    "proof.png",
                    "--json-output",
                    "proof.json",
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 1)
            report = json.loads((root / "proof.json").read_text(encoding="utf-8"))
            self.assertEqual(report["verdict"], "fail")
            self.assertIn("must use only one of size, max_size, or scale", report["items"][0]["problems"][0])

    def test_fails_when_overlay_overlaps_content_without_allow_policy(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            assets = root / "assets"
            assets.mkdir()
            write_panel(assets / "button.png")
            write_overlay(assets / "gem.png")
            manifest = {
                "schema": "game.asset_manifest",
                "version": 1,
                "assets": [
                    {
                        "id": "button",
                        "kind": "slice9",
                        "path": "assets/button.png",
                        "slice9": {"left": 8, "top": 8, "right": 8, "bottom": 8},
                        "content": {"x": 9, "y": 9, "w": 22, "h": 14},
                        "target_preview_sizes": [[96, 40]],
                    },
                    {"id": "gem", "kind": "decor_overlay", "path": "assets/gem.png", "anchor": "center"},
                ],
            }
            layout = {
                "schema": "game.ui_composition_proof_layout",
                "version": 1,
                "items": [{"base_id": "button", "size": [96, 40], "label": "Go", "decor_overlays": ["gem"]}],
            }
            (root / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            (root / "layout.json").write_text(json.dumps(layout), encoding="utf-8")

            result = subprocess.run(
                [
                    "python",
                    str(SCRIPT),
                    "--asset-manifest",
                    "manifest.json",
                    "--layout",
                    "layout.json",
                    "--output",
                    "proof.png",
                    "--json-output",
                    "proof.json",
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 1)
            report = json.loads((root / "proof.json").read_text(encoding="utf-8"))
            self.assertEqual(report["verdict"], "fail")
            self.assertIn("overlaps content rect", report["items"][0]["problems"][0])

    def test_fails_when_label_does_not_fit_content_rect(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            assets = root / "assets"
            assets.mkdir()
            write_panel(assets / "button.png")
            manifest = {
                "schema": "game.asset_manifest",
                "version": 1,
                "assets": [
                    {
                        "id": "button",
                        "kind": "slice9",
                        "path": "assets/button.png",
                        "slice9": {"left": 8, "top": 8, "right": 8, "bottom": 8},
                        "content": {"x": 12, "y": 10, "w": 8, "h": 8},
                        "target_preview_sizes": [[80, 36]],
                    }
                ],
            }
            layout = {
                "schema": "game.ui_composition_proof_layout",
                "version": 1,
                "items": [{"base_id": "button", "size": [80, 36], "label": "This text is too long"}],
            }
            (root / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            (root / "layout.json").write_text(json.dumps(layout), encoding="utf-8")

            result = subprocess.run(
                [
                    "python",
                    str(SCRIPT),
                    "--asset-manifest",
                    "manifest.json",
                    "--layout",
                    "layout.json",
                    "--output",
                    "proof.png",
                    "--json-output",
                    "proof.json",
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 1)
            report = json.loads((root / "proof.json").read_text(encoding="utf-8"))
            self.assertEqual(report["verdict"], "fail")
            self.assertIn("does not fit content rect", report["items"][0]["problems"][0])


if __name__ == "__main__":
    unittest.main()
