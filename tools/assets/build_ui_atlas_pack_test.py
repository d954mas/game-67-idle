import json
import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/assets/build_ui_atlas_pack.py"


def load_builder_module():
    spec = importlib.util.spec_from_file_location("build_ui_atlas_pack_module", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def write_png(path: Path, size=(8, 6), color=(220, 40, 30, 255)) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.new("RGBA", size, color).save(path)


def asset(asset_id: str, path: str, **overrides):
    data = {
        "id": asset_id,
        "kind": "slice9",
        "path": path,
        "pack_group": "ui_common",
        "source_crop": asset_id,
        "original_size": [8, 6],
        "trim_rect": [0, 0, 8, 6],
        "slice9": {"left": 2, "top": 2, "right": 2, "bottom": 2},
        "content": {"x": 2, "y": 2, "w": 4, "h": 2},
        "usage_policy": {"size_class": "compact_only", "min_size": [32, 24]},
        "atlas_policy": {
            "trim_mode": "alpha",
            "alpha_bleed": True,
            "premultiply_alpha": True,
            "extrude": 1,
            "shape_padding": 2,
            "border_padding": 1,
            "scale_variant": "1x",
            "allow_rotation": False,
            "trim_preserves_slice9": True,
        },
    }
    data.update(overrides)
    return data


def run_pack(cwd: Path, *args: str):
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


class BuildUiAtlasPackTest(unittest.TestCase):
    def test_packs_assets_with_extruded_padding_and_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/panel.png", color=(255, 0, 0, 255))
            write_png(root / "assets/runtime/button.png", color=(0, 180, 80, 255))
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "schema": "game.asset_manifest",
                        "version": 1,
                        "assets": [
                            asset("panel", "assets/runtime/panel.png"),
                            asset("button", "assets/runtime/button.png"),
                        ],
                    }
                ),
                encoding="utf-8",
            )
            result = run_pack(
                root,
                "--asset-manifest",
                "manifest.json",
                "--output-dir",
                "packed",
                "--json-output",
                "packed/atlas.json",
                "--report",
                "packed/atlas.md",
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            pack = json.loads((root / "packed/atlas.json").read_text(encoding="utf-8"))
            self.assertEqual(pack["schema"], "game.ui_atlas_pack")
            self.assertEqual(len(pack["atlases"]), 1)
            atlas_info = pack["atlases"][0]
            self.assertEqual(atlas_info["entry_count"], 2)
            atlas = Image.open(root / atlas_info["path"]).convert("RGBA")
            entries = {entry["id"]: entry for entry in atlas_info["entries"]}
            panel = entries["panel"]
            self.assertEqual(panel["atlas_rect"][2:], [8, 6])
            self.assertEqual(panel["padded_rect"][2:], [10, 8])
            self.assertEqual(panel["slice9"]["left"], 2)
            x, y, _, _ = panel["padded_rect"]
            self.assertEqual(atlas.getpixel((x, y)), (255, 0, 0, 255))
            self.assertTrue((root / "packed/atlas.md").exists())

    def test_packs_separate_pack_groups(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/panel.png")
            write_png(root / "assets/runtime/icon.png", size=(5, 5), color=(30, 60, 240, 255))
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "schema": "game.asset_manifest",
                        "version": 1,
                        "assets": [
                            asset("panel", "assets/runtime/panel.png", pack_group="ui_panels"),
                            asset("icon", "assets/runtime/icon.png", kind="icon", pack_group="ui_icons", original_size=[5, 5], trim_rect=[0, 0, 5, 5], atlas_policy={**asset("x", "x")["atlas_policy"], "allow_rotation": True}),
                        ],
                    }
                ),
                encoding="utf-8",
            )
            result = run_pack(root, "--asset-manifest", "manifest.json", "--output-dir", "packed")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            pack = json.loads((root / "packed/ui-atlas-pack.json").read_text(encoding="utf-8"))
            self.assertEqual({atlas["pack_group"] for atlas in pack["atlases"]}, {"ui_panels", "ui_icons"})

    def test_alias_entries_reuse_physical_region(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/button_base.png")
            write_png(root / "assets/runtime/button_primary.png")
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "schema": "game.asset_manifest",
                        "version": 1,
                        "assets": [
                            asset("button_base", "assets/runtime/button_base.png"),
                            asset("button_primary", "assets/runtime/button_primary.png", alias_of="button_base"),
                        ],
                    }
                ),
                encoding="utf-8",
            )
            result = run_pack(root, "--asset-manifest", "manifest.json", "--output-dir", "packed", "--json-output", "packed/atlas.json")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            pack = json.loads((root / "packed/atlas.json").read_text(encoding="utf-8"))
            atlas_info = pack["atlases"][0]
            entries = {entry["id"]: entry for entry in atlas_info["entries"]}
            self.assertEqual(atlas_info["entry_count"], 2)
            self.assertEqual(atlas_info["physical_entry_count"], 1)
            self.assertEqual(atlas_info["alias_count"], 1)
            self.assertEqual(entries["button_primary"]["alias_of"], "button_base")
            self.assertEqual(entries["button_primary"]["atlas_rect"], entries["button_base"]["atlas_rect"])
            self.assertEqual(entries["button_primary"]["padded_rect"], entries["button_base"]["padded_rect"])

    def test_label_review_lists_alias_ids_on_physical_asset(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/button_base.png")
            write_png(root / "assets/runtime/button_primary.png")
            write_png(root / "assets/runtime/button_secondary.png")
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "schema": "game.asset_manifest",
                        "version": 1,
                        "assets": [
                            asset("button_base", "assets/runtime/button_base.png"),
                            asset("button_secondary", "assets/runtime/button_secondary.png", alias_of="button_base"),
                            asset("button_primary", "assets/runtime/button_primary.png", alias_of="button_base"),
                        ],
                    }
                ),
                encoding="utf-8",
            )
            result = run_pack(root, "--asset-manifest", "manifest.json", "--output-dir", "packed", "--json-output", "packed/atlas.json", "--label-review")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            pack = json.loads((root / "packed/atlas.json").read_text(encoding="utf-8"))
            entries = {entry["id"]: entry for entry in pack["atlases"][0]["entries"]}
            self.assertEqual(entries["button_base"]["review_label"]["text"], "button_base (+button_primary,button_secondary)")
            self.assertNotIn("review_label", entries["button_primary"])
            self.assertNotIn("review_label", entries["button_secondary"])

    def test_label_review_wraps_long_ids_without_widening_tile(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            base_id = "fantasy_panel_primary_inventory_frame"
            hover_id = "fantasy_panel_primary_inventory_frame_hovered"
            selected_id = "fantasy_panel_primary_inventory_frame_selected"
            write_png(root / "assets/runtime/base.png")
            write_png(root / "assets/runtime/hover.png")
            write_png(root / "assets/runtime/selected.png")
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "schema": "game.asset_manifest",
                        "version": 1,
                        "assets": [
                            asset(base_id, "assets/runtime/base.png"),
                            asset(hover_id, "assets/runtime/hover.png", alias_of=base_id),
                            asset(selected_id, "assets/runtime/selected.png", alias_of=base_id),
                        ],
                    }
                ),
                encoding="utf-8",
            )
            result = run_pack(root, "--asset-manifest", "manifest.json", "--output-dir", "packed", "--json-output", "packed/atlas.json", "--label-review")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            pack = json.loads((root / "packed/atlas.json").read_text(encoding="utf-8"))
            atlas_info = pack["atlases"][0]
            entries = {entry["id"]: entry for entry in atlas_info["entries"]}
            label = entries[base_id]["review_label"]
            self.assertEqual(label["text"], f"{base_id} (+{hover_id},{selected_id})")
            self.assertGreater(len(label["lines"]), 3)
            self.assertLessEqual(label["rect"][2], 80)
            self.assertTrue(all(line for line in label["lines"]))
            self.assertNotIn("review_label", entries[hover_id])
            preview = Image.open(root / atlas_info["labeled_preview_path"]).convert("RGBA")
            label_x, label_y, _, _ = label["rect"]
            self.assertGreater(preview.getpixel((label_x, label_y))[3], 0)

    def test_label_review_marks_manifest_as_review_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/panel.png")
            manifest = root / "manifest.json"
            manifest.write_text(json.dumps({"schema": "game.asset_manifest", "version": 1, "assets": [asset("panel", "assets/runtime/panel.png")]}), encoding="utf-8")
            result = run_pack(root, "--asset-manifest", "manifest.json", "--output-dir", "packed", "--json-output", "packed/atlas.json", "--report", "packed/atlas.md", "--label-review", "--profile")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            pack = json.loads((root / "packed/atlas.json").read_text(encoding="utf-8"))
            self.assertEqual(pack["purpose"], "review_validation_atlas_not_engine_runtime_pack")
            self.assertTrue(pack["label_overlay"])
            self.assertIn("atlas_efficiency", pack)
            self.assertGreater(pack["atlas_efficiency"]["occupancy_ratio"], 0)
            self.assertIn("timing_ms", pack)
            self.assertGreaterEqual(pack["timing_ms"]["total"], 0)
            atlas_info = pack["atlases"][0]
            self.assertTrue(atlas_info["label_overlay"])
            self.assertIn("labeled_preview_path", atlas_info)
            self.assertGreater(atlas_info["occupancy_ratio"], 0)
            self.assertIn("timing_ms", atlas_info)
            entries = {entry["id"]: entry for entry in atlas_info["entries"]}
            label = entries["panel"]["review_label"]
            self.assertEqual(label["text"], "panel")
            self.assertGreaterEqual(label["font_size"], 12)
            self.assertIn(label["placement"], {"bottom", "right"})
            padded_x, padded_y, padded_w, padded_h = entries["panel"]["padded_rect"]
            label_x, label_y, label_w, label_h = label["rect"]
            self.assertFalse(
                label_x < padded_x + padded_w
                and label_x + label_w > padded_x
                and label_y < padded_y + padded_h
                and label_y + label_h > padded_y
            )
            atlas = Image.open(root / atlas_info["path"]).convert("RGBA")
            preview = Image.open(root / atlas_info["labeled_preview_path"]).convert("RGBA")
            self.assertEqual(atlas.getpixel((label_x, label_y))[3], 0)
            self.assertGreater(preview.getpixel((label_x, label_y))[3], 0)
            report = (root / "packed/atlas.md").read_text(encoding="utf-8")
            self.assertIn("## Labeled Preview Policy", report)
            self.assertIn("Names are drawn only on labeled preview PNGs", report)
            self.assertIn("- mode: `label_overlay_only`", report)
            self.assertIn("- allowed_delta: `review_label_rects_only`", report)
            self.assertIn("- debug_outlines: `false`", report)
            self.assertIn("## Atlas Efficiency", report)
            self.assertIn("## Timing", report)
            self.assertIn("## Asset Id Index", report)
            self.assertIn("### ui_common", report)
            self.assertIn("labeled_preview: `packed/ui_common-labeled.png`", report)
            self.assertIn("- `panel`: kind=slice9", report)
            self.assertIn("label_rect=", report)
            self.assertIn("label_placement=", report)
            self.assertIn("label_lines=['panel']", report)
            self.assertIn("profile: slowest atlas group", result.stdout)

    def test_label_review_can_use_right_side_free_space_for_small_assets(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/tiny_icon.png", size=(12, 12), color=(40, 120, 220, 255))
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "schema": "game.asset_manifest",
                        "version": 1,
                        "assets": [
                            asset(
                                "tiny_icon",
                                "assets/runtime/tiny_icon.png",
                                kind="icon",
                                original_size=[12, 12],
                                trim_rect=[0, 0, 12, 12],
                            )
                        ],
                    }
                ),
                encoding="utf-8",
            )
            result = run_pack(root, "--asset-manifest", "manifest.json", "--output-dir", "packed", "--json-output", "packed/atlas.json", "--label-review")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            pack = json.loads((root / "packed/atlas.json").read_text(encoding="utf-8"))
            entry = pack["atlases"][0]["entries"][0]
            label = entry["review_label"]
            self.assertEqual(label["placement"], "right")
            padded_x, padded_y, padded_w, padded_h = entry["padded_rect"]
            label_x, label_y, _, _ = label["rect"]
            self.assertGreaterEqual(label_x, padded_x + padded_w)
            self.assertEqual(label_y, padded_y)

    def test_atomic_image_write_keeps_existing_png_on_save_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = root / "atlas.png"
            write_png(target, size=(4, 4), color=(10, 20, 30, 255))
            builder = load_builder_module()

            def failing_save(_image, path, *args, **kwargs):
                Path(path).write_bytes(b"partial-png")
                raise RuntimeError("simulated interrupted save")

            with patch.object(Image.Image, "save", failing_save):
                with self.assertRaises(RuntimeError):
                    builder.save_image_atomic(Image.new("RGBA", (4, 4), (200, 0, 0, 255)), target)

            restored = Image.open(target).convert("RGBA")
            self.assertEqual(restored.getpixel((0, 0)), (10, 20, 30, 255))
            self.assertEqual(list(root.glob(".atlas.png.*.tmp")), [])

    def test_fails_when_asset_exceeds_max_size(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/huge.png", size=(80, 80))
            manifest = root / "manifest.json"
            manifest.write_text(
                json.dumps({"schema": "game.asset_manifest", "version": 1, "assets": [asset("huge", "assets/runtime/huge.png", original_size=[80, 80], trim_rect=[0, 0, 80, 80])]}),
                encoding="utf-8",
            )
            result = run_pack(root, "--asset-manifest", "manifest.json", "--output-dir", "packed", "--max-size", "64")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("exceeds max atlas size", result.stderr + result.stdout)


if __name__ == "__main__":
    unittest.main()
