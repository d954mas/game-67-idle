import tempfile
import unittest
from pathlib import Path

from ai_studio.runtime_automation.window_layout_contract import (
    check_contract, check_scroll_motion, check_source_no_scroll, check_studio_games,
    compare_windows, function_body,
)


def tree(scale=1.0, *, rearrange=False, clipped=False):
    height = 900 * scale
    nodes = [
        {"id": 1, "parent": 0, "id_string": "settings/window", "visible": True,
         "bounds": {"x": 100 * scale, "y": 150 * scale, "w": 400 * scale, "h": 500 * scale}},
        {"id": 2, "parent": 1, "id_string": "", "visible": True,
         "bounds": {"x": 100 * scale, "y": 150 * scale, "w": 400 * scale, "h": 500 * scale}},
        {"id": 3, "parent": 2, "id_string": "settings/music", "visible": True,
         "bounds": {"x": (290 if rearrange else 120) * scale, "y": 500 * scale,
                    "w": 180 * scale, "h": 45 * scale}},
        {"id": 4, "parent": 2, "id_string": "settings/sfx", "visible": True,
         "bounds": {"x": 120 * scale, "y": (640 if clipped else 430) * scale,
                    "w": 180 * scale, "h": 45 * scale}},
    ]
    return {"width": 600 * scale, "height": height, "truncated": False, "nodes": nodes}


class WindowLayoutContractTest(unittest.TestCase):
    def test_uniform_scaled_window_passes(self):
        self.assertEqual(compare_windows(tree(), tree(0.7), "settings/window"), [])

    def test_rearrangement_fails(self):
        errors = compare_windows(tree(), tree(0.7, rearrange=True), "settings/window")
        self.assertTrue(any("settings/music changes" in error for error in errors))

    def test_clipped_control_fails(self):
        errors = compare_windows(tree(), tree(0.7, clipped=True), "settings/window")
        self.assertTrue(any("settings/sfx is clipped" in error for error in errors))

    def test_undeclared_scroll_region_fails(self):
        before = tree()
        before["nodes"][2]["id_string"] = "settings/scroll"
        errors = compare_windows(before, tree(0.7), "settings/window")
        self.assertTrue(any("undeclared scroll region" in error for error in errors))

    def test_scroll_function_fails_but_comments_do_not(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
            path = Path(folder) / "settings.c"
            path.write_text("void settings_draw(void) { /* nt_ui_scroll_begin() */ hud_content_begin(); }", encoding="utf-8")
            self.assertTrue(check_source_no_scroll(path, "settings_draw"))
            path.write_text("void settings_draw(void) { /* nt_ui_scroll_begin() */ draw_rows(); }", encoding="utf-8")
            self.assertEqual(check_source_no_scroll(path, "settings_draw"), [])

    def test_function_extraction_ignores_literal_braces(self):
        body = function_body('void settings_draw(void) { label("}"); if (ok) { draw(); } }', "settings_draw")
        self.assertIn("draw()", body)

    def test_wheel_motion_is_forbidden_without_scroll(self):
        before = tree()
        after = tree()
        after["nodes"][2]["bounds"]["y"] -= 20
        errors = check_scroll_motion(before, after, "settings/window", None, set())
        self.assertTrue(any("outside no-scroll window" in error for error in errors))

    def test_wheel_motion_is_limited_to_declared_region(self):
        before = tree()
        before["nodes"].insert(2, {"id": 5, "parent": 2, "id_string": "settings/list/scroll", "visible": True,
                                    "bounds": {"x": 110, "y": 400, "w": 350, "h": 180}})
        before["nodes"][3]["parent"] = 5
        after = __import__("copy").deepcopy(before)
        after["nodes"][3]["bounds"]["y"] -= 20
        self.assertEqual(check_scroll_motion(before, after, "settings/window", "settings/list/scroll",
                                             {"settings/list/scroll"}), [])
        after["nodes"][4]["bounds"]["y"] -= 20
        self.assertTrue(check_scroll_motion(before, after, "settings/window", "settings/list/scroll",
                                            {"settings/list/scroll"}))

    def test_contract_covers_all_source_windows_and_modal_scenes(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
            root = Path(folder)
            (root / "src").mkdir()
            (root / "src" / "ui.c").write_text(
                'void settings_draw(void) { window("settings/window"); }\n'
                'void inventory_draw(void) { window("inventory/window"); }\n', encoding="utf-8")
            contract = {"schema": "ai_studio.ui_window_contracts.v1", "windows": [
                {"id": "settings/window", "scene": "settings", "source": {"file": "src/ui.c", "function": "settings_draw"}},
                {"id": "inventory/window", "scene": "inventory", "layout": "adaptive", "scroll": ["inventory/scroll"],
                 "reason": "Grid and detail panel need separate placement", "source": {"file": "src/ui.c", "function": "inventory_draw"}},
            ]}
            scenes = {"scenes": [{"id": "settings", "kind": "modal"}, {"id": "inventory", "kind": "modal"}]}
            self.assertEqual(check_contract(contract, root, scene_list=scenes), [])
            contract["windows"].pop()
            errors = check_contract(contract, root, scene_list=scenes)
            self.assertTrue(any("window roots without contracts" in error for error in errors))
            self.assertTrue(any("modal scenes without window contracts" in error for error in errors))

    def test_contract_requires_reason_and_checks_every_proof(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
            root = Path(folder)
            (root / "src").mkdir()
            (root / "src" / "ui.c").write_text('void settings_draw(void) { window("settings/window"); }', encoding="utf-8")
            contract = {"schema": "ai_studio.ui_window_contracts.v1", "windows": [
                {"id": "settings/window", "scene": "settings", "layout": "adaptive", "source": {"file": "src/ui.c", "function": "settings_draw"}}
            ]}
            self.assertTrue(any("needs a reason" in error for error in check_contract(contract, root)))
            contract["windows"][0]["layout"] = "single"
            proof = {"schema": "ai_studio.ui_window_proof.v1", "windows": [
                {"id": "settings/window", "portrait": tree(), "landscape": tree(0.7, rearrange=True)}
            ]}
            self.assertTrue(any("settings/music changes" in error for error in check_contract(contract, root, proof)))

    def test_studio_inventory_reports_missing_contracts(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
            game = Path(folder) / "games" / "private" / "one-game"
            (game / "src").mkdir(parents=True)
            self.assertTrue(any("one-game: missing" in error for error in check_studio_games(Path(folder))))
            (game / "design").mkdir()
            (game / "design" / "ui_window_contracts.json").write_text(
                '{"schema":"ai_studio.ui_window_contracts.v1","windows":[],"no_windows_reason":"No UI windows"}',
                encoding="utf-8")
            self.assertEqual(check_studio_games(Path(folder)), [])

    def test_contract_rejects_fake_root_and_malformed_open_wait(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
            root = Path(folder)
            (root / "src").mkdir()
            (root / "src" / "ui.c").write_text('void draw(void) { window("actual/window"); }', encoding="utf-8")
            contract = {"schema": "ai_studio.ui_window_contracts.v1", "windows": [{
                "id": "fake/window", "scene": "screen", "source": {"file": "src/ui.c", "function": "draw"},
                "open": [{"method": "game.scene.show", "waitFor": 1}],
            }]}
            errors = check_contract(contract, root)
            self.assertTrue(any("root ID literal is absent" in error for error in errors))
            self.assertTrue(any("open must contain" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
