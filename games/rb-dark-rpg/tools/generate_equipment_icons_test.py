#!/usr/bin/env python3

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageDraw

import generate_equipment_icons as equipment_icons


class EquipmentIconGenerationTest(unittest.TestCase):
    def test_icon_specs_cover_current_slots_and_gear_items(self) -> None:
        items_doc = equipment_icons.load_items_doc(equipment_icons.DEFAULT_ITEMS_PATH)
        specs = equipment_icons.collect_icon_specs(items_doc)

        slot_ids = [slot["id"] for slot in items_doc["equipment_slots"]]
        expected_slot_asset_ids = {f"asset_slot_icon_{slot_id}_empty" for slot_id in slot_ids}
        expected_gear_asset_ids = {
            item["icon_asset_id"]
            for item in items_doc["items"]
            if item.get("kind") == "gear"
        }
        expected_reward_item_asset_ids = {
            item["icon_asset_id"]
            for item in items_doc["items"]
            if item.get("kind") != "gear" and item.get("icon_asset_id")
        }

        self.assertEqual({spec.asset_id for spec in specs.slots}, expected_slot_asset_ids)
        self.assertEqual({spec.asset_id for spec in specs.gear}, expected_gear_asset_ids)
        self.assertEqual({spec.asset_id for spec in specs.reward_items}, expected_reward_item_asset_ids)
        self.assertEqual(specs.cell.asset_id, "asset_equipment_slot_cell")
        self.assertEqual([spec.asset_id for spec in specs.reward_tokens], ["asset_reward_xp"])
        self.assertEqual(len(specs.slots), 12)
        self.assertEqual(len(specs.gear), 15)
        self.assertEqual(len(specs.reward_items), 6)

    def test_generated_pack_writes_manifest_and_rgba_icons(self) -> None:
        items_doc = equipment_icons.load_items_doc(equipment_icons.DEFAULT_ITEMS_PATH)
        specs = equipment_icons.collect_icon_specs(items_doc)

        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp) / "equipment_icons_01"
            cell_source = Path(tmp) / "cell_source.png"
            slots_source = Path(tmp) / "slots_source_sheet.png"
            gear_source = Path(tmp) / "gear_source_sheet.png"
            reward_source = Path(tmp) / "reward_source_sheet.png"
            self._write_fake_cell(cell_source)
            self._write_fake_sheet(slots_source, columns=4, rows=3)
            self._write_fake_sheet(gear_source, columns=5, rows=3)
            self._write_fake_sheet(reward_source, columns=7, rows=1)

            manifest = equipment_icons.write_icon_pack(specs, out_dir, cell_source, slots_source, gear_source, reward_source)

            self.assertEqual(manifest["schema"], "rb-dark-rpg.equipment_icons.v1")
            self.assertEqual(manifest["cell"]["asset_id"], "asset_equipment_slot_cell")
            self.assertEqual(len(manifest["slots"]), 12)
            self.assertEqual(len(manifest["gear_items"]), 15)
            self.assertEqual(len(manifest["reward_tokens"]), 1)
            self.assertEqual(len(manifest["reward_items"]), 6)
            self.assertEqual(manifest["source_sheets"][0]["origin"], "ai")
            self.assertEqual(manifest["source_sheets"][0]["license"], "project-internal generated asset")
            self.assertEqual(manifest["contact_sheet"]["origin"], "derived")
            self.assertEqual(manifest["contact_sheet"]["license"], "project-internal generated asset")
            for entry in [manifest["cell"], *manifest["slots"], *manifest["gear_items"], *manifest["reward_tokens"], *manifest["reward_items"]]:
                icon_path = out_dir / entry["file"]
                self.assertTrue(icon_path.exists(), entry["asset_id"])
                self.assertEqual(len(entry["sha256"]), 64)
                with Image.open(icon_path) as img:
                    self.assertEqual(img.size, (equipment_icons.ICON_SIZE, equipment_icons.ICON_SIZE))
                    self.assertEqual(img.mode, "RGBA")

    @staticmethod
    def _write_fake_sheet(path: Path, columns: int, rows: int) -> None:
        cell = 96
        image = Image.new("RGB", (columns * cell, rows * cell), (0, 255, 0))
        draw = ImageDraw.Draw(image)
        for row in range(rows):
            for col in range(columns):
                x0 = col * cell + 18
                y0 = row * cell + 18
                draw.rounded_rectangle([x0, y0, x0 + 60, y0 + 60], radius=8, fill=(32, 24, 18), outline=(170, 110, 52), width=3)
                draw.ellipse([x0 + 22, y0 + 20, x0 + 42, y0 + 44], fill=(190, 150, 85))
        image.save(path)

    @staticmethod
    def _write_fake_cell(path: Path) -> None:
        image = Image.new("RGB", (128, 128), (0, 255, 0))
        draw = ImageDraw.Draw(image)
        draw.rounded_rectangle([20, 20, 108, 108], radius=12, fill=(22, 17, 13), outline=(170, 110, 52), width=6)
        image.save(path)


if __name__ == "__main__":
    unittest.main()
