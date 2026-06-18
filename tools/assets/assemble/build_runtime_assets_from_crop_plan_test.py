import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "tools/assets/assemble/build_runtime_assets_from_crop_plan.py"


def run_script(cwd: Path, script: Path, *args: str):
    return subprocess.run(
        [sys.executable, str(script), *args],
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def write_source(path: Path) -> None:
    image = Image.new("RGBA", (180, 90), (0, 255, 0, 255))
    draw = ImageDraw.Draw(image)
    draw.ellipse((20, 20, 60, 60), fill=(220, 40, 30, 255))
    draw.rectangle((110, 24, 150, 64), fill=(40, 80, 220, 255))
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def write_soft_source(path: Path) -> None:
    # A wide radial glow fading into the green key -> soft fractional alpha that
    # route_cutout flags as dual_plate (key_matte would flatten it).
    import numpy as np

    size = 96
    yy, xx = np.mgrid[0:size, 0:size]
    r = np.sqrt((xx - size / 2.0) ** 2 + (yy - size / 2.0) ** 2)
    t = np.clip(1.0 - r / 42.0, 0.0, 1.0)[..., None]
    img = t * np.array([255, 150, 0], dtype=float) + (1.0 - t) * np.array([0, 255, 0], dtype=float)
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(np.clip(img, 0, 255).astype("uint8"), "RGB").convert("RGBA").save(path)


class BuildRuntimeAssetsFromCropPlanTest(unittest.TestCase):
    def test_builds_runtime_assets_and_manifests_from_crop_plan(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_source(root / "source.png")
            plan = {
                "schema": "game.runtime_crop_plan",
                "version": 1,
                "source": "source.png",
                "source_id": "test_source",
                "source_role": "isolated_icon_sheet",
                "output_dir": "assets/runtime/test-icons",
                "crops": [
                    {
                        "id": "icon_health",
                        "kind": "icon",
                        "rect": [10, 10, 60, 60],
                        "output": "assets/runtime/test-icons/icon_health.png",
                        "trim": {"mode": "alpha_bounds", "padding": 6},
                        "chroma_key": {"mode": "border_connected", "key": "#00ff00", "tolerance": 8},
                        "atlas": {"pack_group": "ui_icons_core", "allow_rotation": False, "extrude": 2, "shape_padding": 2},
                    },
                    {
                        "id": "decor_blue_block",
                        "kind": "decor",
                        "rect": [100, 14, 60, 60],
                        "output": "assets/runtime/test-icons/decor_blue_block.png",
                        "trim": {"mode": "alpha_bounds", "padding": 6},
                        "chroma_key": {"mode": "border_connected", "key": "#00ff00", "tolerance": 8},
                        "atlas": {"pack_group": "ui_decor", "allow_rotation": False, "extrude": 2, "shape_padding": 2},
                    },
                ],
            }
            (root / "plan.json").write_text(json.dumps(plan), encoding="utf-8")
            result = run_script(
                root,
                SCRIPT,
                "--crop-plan",
                "plan.json",
                "--crop-manifest",
                "crop_manifest.json",
                "--asset-manifest",
                "asset_manifest.json",
                "--art-job",
                "gamedesign/projects/test/art_requests/test.json",
                "--contact-sheet",
                "contact.png",
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            crop_manifest = json.loads((root / "crop_manifest.json").read_text(encoding="utf-8"))
            asset_manifest = json.loads((root / "asset_manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(crop_manifest["sources"][0]["crops"][0]["isolate_component"], "source_component_bbox")
            self.assertEqual(crop_manifest["sources"][0]["crops"][1]["kind"], "decor_overlay")
            assets = {asset["id"]: asset for asset in asset_manifest["assets"]}
            self.assertEqual(assets["icon_health"]["kind"], "icon")
            self.assertEqual(assets["decor_blue_block"]["kind"], "decor_overlay")
            self.assertEqual(assets["icon_health"]["pack_group"], "ui_icons_core")
            self.assertEqual(assets["decor_blue_block"]["anchor"], "center")
            self.assertTrue((root / "contact.png").exists())

    def test_route_warns_on_soft_crop_and_strict_route_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_soft_source(root / "soft.png")
            plan = {
                "schema": "game.runtime_crop_plan",
                "version": 1,
                "source": "soft.png",
                "source_id": "soft",
                "source_role": "isolated_icon_sheet",
                "output_dir": "assets/runtime/soft",
                "crops": [
                    {
                        "id": "glow_fx",
                        "kind": "decor",
                        "rect": [4, 4, 88, 88],
                        "output": "assets/runtime/soft/glow_fx.png",
                        "trim": {"padding": 6},
                        "chroma_key": {"key": "#00ff00"},
                        "atlas": {"pack_group": "ui_fx"},
                    }
                ],
            }
            (root / "plan.json").write_text(json.dumps(plan), encoding="utf-8")
            common = [
                "--crop-plan", "plan.json",
                "--crop-manifest", "cm.json",
                "--asset-manifest", "am.json",
                "--art-job", "gamedesign/projects/test/art_requests/test.json",
            ]
            # Default: warn (the build still succeeds) and the route warning is emitted.
            warn = run_script(root, SCRIPT, *common)
            self.assertEqual(warn.returncode, 0, warn.stdout + warn.stderr)
            self.assertIn("WARN route", warn.stderr)
            self.assertIn("dual_plate", warn.stderr)
            # --strict-route: hard-fail on the soft crop instead of hard-keying it.
            strict = run_script(root, SCRIPT, *common, "--strict-route")
            self.assertNotEqual(strict.returncode, 0)
            self.assertIn("route error", strict.stdout + strict.stderr)


if __name__ == "__main__":
    unittest.main()
