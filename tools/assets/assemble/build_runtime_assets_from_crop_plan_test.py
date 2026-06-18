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


if __name__ == "__main__":
    unittest.main()
