import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
SCRIPT = ROOT / "ai_studio/assets/prep/crop/plan_prepared_crops_from_intake.py"


def write_intake(path: Path) -> None:
    path.write_text(
        json.dumps(
            {
                "schema": "game.source_sheet_intake_audit",
                "version": 1,
                "status": "pass",
                "source": "games/test/design/art/source.png",
                "size": [420, 300],
                "key_color": "#00ff00",
                "key_tolerance": 8,
                "components": [
                    {"id": "component_a", "bbox": [260, 38, 80, 70], "area_px": 4000},
                    {"id": "component_b", "bbox": [20, 62, 60, 60], "area_px": 3000},
                    {"id": "component_c", "bbox": [0, 230, 40, 50], "area_px": 2000},
                ],
            }
        )
        + "\n",
        encoding="utf-8",
    )


def run_tool(*args: str):
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


class PlanPreparedCropsFromIntakeTest(unittest.TestCase):
    def test_writes_named_row_major_crop_plan_with_clamped_padding(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            intake = root / "intake.json"
            output = root / "plan.json"
            report = root / "plan.md"
            write_intake(intake)
            result = run_tool(
                "--intake-audit",
                str(intake),
                "--ids",
                "left_icon,right_icon,bottom_icon",
                "--kind",
                "icon",
                "--source-id",
                "test-icons",
                "--source-role",
                "isolated icon sheet",
                "--output-dir",
                "assets/prepared/test-icons",
                "--json-output",
                str(output),
                "--report",
                str(report),
                "--padding",
                "12",
                "--row-tolerance",
                "80",
                "--pack-group",
                "ui_icons_core",
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            plan = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(plan["schema"], "game.prepared_crop_plan")
            self.assertEqual([crop["id"] for crop in plan["crops"]], ["left_icon", "right_icon", "bottom_icon"])
            self.assertEqual(plan["crops"][0]["source_component_id"], "component_b")
            self.assertEqual(plan["crops"][1]["source_component_id"], "component_a")
            self.assertEqual(plan["crops"][2]["rect"], [0, 218, 52, 74])
            self.assertEqual(plan["crops"][0]["atlas"]["pack_group"], "ui_icons_core")
            self.assertIn("left_icon", report.read_text(encoding="utf-8"))

    def test_rejects_id_count_mismatch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            intake = root / "intake.json"
            output = root / "plan.json"
            write_intake(intake)
            result = run_tool(
                "--intake-audit",
                str(intake),
                "--ids",
                "only_one",
                "--kind",
                "decor",
                "--source-id",
                "test-decor",
                "--source-role",
                "ui decor overlay sheet",
                "--output-dir",
                "assets/prepared/test-decor",
                "--json-output",
                str(output),
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("--ids count", result.stdout + result.stderr)

    def test_accepts_ids_file_to_avoid_long_command_lines(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            intake = root / "intake.json"
            ids_file = root / "ids.txt"
            output = root / "plan.json"
            write_intake(intake)
            ids_file.write_text("left_icon\nright_icon\nbottom_icon\n", encoding="utf-8")
            result = run_tool(
                "--intake-audit",
                str(intake),
                "--ids-file",
                str(ids_file),
                "--kind",
                "icon",
                "--source-id",
                "test-icons",
                "--source-role",
                "isolated icon sheet",
                "--output-dir",
                "assets/prepared/test-icons",
                "--json-output",
                str(output),
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            plan = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual([crop["id"] for crop in plan["crops"]], ["left_icon", "right_icon", "bottom_icon"])


if __name__ == "__main__":
    unittest.main()
