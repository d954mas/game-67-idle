import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
PACKER = ROOT / "tools/assets/build_ui_atlas_pack.py"
AUDIT = ROOT / "tools/assets/audit_ui_atlas_pack.py"


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


def run(script: Path, cwd: Path, *args: str):
    return subprocess.run(
        [sys.executable, str(script), *args],
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def build_pack(root: Path, assets, label_review: bool = False):
    manifest = root / "manifest.json"
    manifest.write_text(json.dumps({"schema": "game.asset_manifest", "version": 1, "assets": assets}), encoding="utf-8")
    args = [
        "--asset-manifest",
        "manifest.json",
        "--output-dir",
        "packed",
        "--json-output",
        "packed/atlas.json",
    ]
    if label_review:
        args.append("--label-review")
    result = run(PACKER, root, *args)
    if result.returncode != 0:
        raise AssertionError(result.stdout + result.stderr)
    return manifest, root / "packed/atlas.json"


class AuditUiAtlasPackTest(unittest.TestCase):
    def test_passes_valid_pack(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/panel.png")
            write_png(root / "assets/runtime/button.png", color=(0, 180, 80, 255))
            manifest, pack = build_pack(
                root,
                [
                    asset("panel", "assets/runtime/panel.png"),
                    asset("button", "assets/runtime/button.png"),
                ],
            )
            result = run(
                AUDIT,
                root,
                "--atlas-pack",
                str(pack.relative_to(root)),
                "--asset-manifest",
                str(manifest.relative_to(root)),
                "--json-output",
                "packed/audit.json",
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            audit = json.loads((root / "packed/audit.json").read_text(encoding="utf-8"))
            self.assertEqual(audit["verdict"], "pass")

    def test_rejects_broken_extrusion_pixels(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/panel.png")
            manifest, pack = build_pack(root, [asset("panel", "assets/runtime/panel.png")])
            pack_data = json.loads(pack.read_text(encoding="utf-8"))
            atlas_path = root / pack_data["atlases"][0]["path"]
            atlas = Image.open(atlas_path).convert("RGBA")
            entry = pack_data["atlases"][0]["entries"][0]
            x, y, _, _ = entry["atlas_rect"]
            atlas.putpixel((x, y - 1), (0, 0, 255, 255))
            atlas.save(atlas_path)

            result = run(AUDIT, root, "--atlas-pack", str(pack.relative_to(root)), "--asset-manifest", str(manifest.relative_to(root)))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("extrusion pixel mismatch", result.stdout + result.stderr)

    def test_rejects_missing_asset_coverage(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/panel.png")
            write_png(root / "assets/runtime/button.png", color=(0, 180, 80, 255))
            manifest, pack = build_pack(
                root,
                [
                    asset("panel", "assets/runtime/panel.png"),
                    asset("button", "assets/runtime/button.png"),
                ],
            )
            pack_data = json.loads(pack.read_text(encoding="utf-8"))
            pack_data["atlases"][0]["entries"] = [entry for entry in pack_data["atlases"][0]["entries"] if entry["id"] != "button"]
            pack.write_text(json.dumps(pack_data), encoding="utf-8")
            result = run(AUDIT, root, "--atlas-pack", str(pack.relative_to(root)), "--asset-manifest", str(manifest.relative_to(root)))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("missing packed asset id button", result.stdout + result.stderr)

    def test_accepts_alias_entries_reusing_target_region(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/button_base.png")
            write_png(root / "assets/runtime/button_primary.png")
            manifest, pack = build_pack(
                root,
                [
                    asset("button_base", "assets/runtime/button_base.png"),
                    asset("button_primary", "assets/runtime/button_primary.png", alias_of="button_base"),
                ],
            )
            result = run(AUDIT, root, "--atlas-pack", str(pack.relative_to(root)), "--asset-manifest", str(manifest.relative_to(root)), "--json-output", "packed/audit.json")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            audit = json.loads((root / "packed/audit.json").read_text(encoding="utf-8"))
            self.assertEqual(audit["verdict"], "pass")
            self.assertEqual(audit["atlases"][0]["alias_count"], 1)

    def test_passes_labeled_review_rects_outside_assets(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/panel.png")
            manifest, pack = build_pack(root, [asset("panel", "assets/runtime/panel.png")], label_review=True)
            result = run(
                AUDIT,
                root,
                "--atlas-pack",
                str(pack.relative_to(root)),
                "--asset-manifest",
                str(manifest.relative_to(root)),
                "--json-output",
                "packed/audit.json",
                "--report",
                "packed/audit.md",
                "--profile",
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            audit = json.loads((root / "packed/audit.json").read_text(encoding="utf-8"))
            self.assertEqual(audit["verdict"], "pass")
            self.assertIn("timing_ms", audit)
            self.assertIn("timing_ms", audit["atlases"][0])
            self.assertIn("labeled_preview_path", audit["atlases"][0])
            self.assertIn("profile: slowest atlas audit", result.stdout)
            markdown = (root / "packed/audit.md").read_text(encoding="utf-8")
            self.assertIn("## Timing", markdown)

    def test_rejects_labeled_review_pack_without_preview_image(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/panel.png")
            manifest, pack = build_pack(root, [asset("panel", "assets/runtime/panel.png")], label_review=True)
            pack_data = json.loads(pack.read_text(encoding="utf-8"))
            preview_path = root / pack_data["atlases"][0]["labeled_preview_path"]
            preview_path.unlink()
            result = run(AUDIT, root, "--atlas-pack", str(pack.relative_to(root)), "--asset-manifest", str(manifest.relative_to(root)))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("labeled preview image missing", result.stdout + result.stderr)

    def test_rejects_label_pixels_in_clean_atlas(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/panel.png")
            manifest, pack = build_pack(root, [asset("panel", "assets/runtime/panel.png")], label_review=True)
            pack_data = json.loads(pack.read_text(encoding="utf-8"))
            atlas_path = root / pack_data["atlases"][0]["path"]
            entry = pack_data["atlases"][0]["entries"][0]
            label_x, label_y, _, _ = entry["review_label"]["rect"]
            atlas = Image.open(atlas_path).convert("RGBA")
            atlas.putpixel((label_x, label_y), (255, 255, 255, 255))
            atlas.save(atlas_path)
            result = run(AUDIT, root, "--atlas-pack", str(pack.relative_to(root)), "--asset-manifest", str(manifest.relative_to(root)))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("review_label rect must be empty in clean atlas", result.stdout + result.stderr)

    def test_rejects_wrong_review_label_text(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/button_base.png")
            write_png(root / "assets/runtime/button_primary.png")
            manifest, pack = build_pack(
                root,
                [
                    asset("button_base", "assets/runtime/button_base.png"),
                    asset("button_primary", "assets/runtime/button_primary.png", alias_of="button_base"),
                ],
                label_review=True,
            )
            pack_data = json.loads(pack.read_text(encoding="utf-8"))
            entry = next(entry for entry in pack_data["atlases"][0]["entries"] if entry["id"] == "button_base")
            entry["review_label"]["text"] = "wrong_button"
            pack.write_text(json.dumps(pack_data), encoding="utf-8")
            result = run(AUDIT, root, "--atlas-pack", str(pack.relative_to(root)), "--asset-manifest", str(manifest.relative_to(root)))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("button_base review_label text must be `button_base (+button_primary)`", result.stdout + result.stderr)

    def test_rejects_overlapping_review_label_rects(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/panel.png")
            write_png(root / "assets/runtime/button.png", color=(0, 180, 80, 255))
            manifest, pack = build_pack(
                root,
                [
                    asset("panel", "assets/runtime/panel.png"),
                    asset("button", "assets/runtime/button.png"),
                ],
                label_review=True,
            )
            pack_data = json.loads(pack.read_text(encoding="utf-8"))
            entries = {entry["id"]: entry for entry in pack_data["atlases"][0]["entries"]}
            entries["button"]["review_label"]["rect"] = list(entries["panel"]["review_label"]["rect"])
            pack.write_text(json.dumps(pack_data), encoding="utf-8")
            result = run(AUDIT, root, "--atlas-pack", str(pack.relative_to(root)), "--asset-manifest", str(manifest.relative_to(root)))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("review_label rect overlaps review_label rect", result.stdout + result.stderr)

    def test_rejects_labeled_review_rect_over_asset(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/panel.png")
            manifest, pack = build_pack(root, [asset("panel", "assets/runtime/panel.png")], label_review=True)
            pack_data = json.loads(pack.read_text(encoding="utf-8"))
            entry = pack_data["atlases"][0]["entries"][0]
            entry["review_label"]["rect"] = list(entry["padded_rect"])
            pack.write_text(json.dumps(pack_data), encoding="utf-8")
            result = run(AUDIT, root, "--atlas-pack", str(pack.relative_to(root)), "--asset-manifest", str(manifest.relative_to(root)))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("review_label rect overlaps padded_rect", result.stdout + result.stderr)

    def test_rejects_alias_rect_mismatch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_png(root / "assets/runtime/button_base.png")
            write_png(root / "assets/runtime/button_primary.png")
            manifest, pack = build_pack(
                root,
                [
                    asset("button_base", "assets/runtime/button_base.png"),
                    asset("button_primary", "assets/runtime/button_primary.png", alias_of="button_base"),
                ],
            )
            pack_data = json.loads(pack.read_text(encoding="utf-8"))
            for entry in pack_data["atlases"][0]["entries"]:
                if entry["id"] == "button_primary":
                    entry["atlas_rect"] = [value + 1 if index == 0 else value for index, value in enumerate(entry["atlas_rect"])]
            pack.write_text(json.dumps(pack_data), encoding="utf-8")
            result = run(AUDIT, root, "--atlas-pack", str(pack.relative_to(root)), "--asset-manifest", str(manifest.relative_to(root)))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("atlas_rect must reuse alias target button_base", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
