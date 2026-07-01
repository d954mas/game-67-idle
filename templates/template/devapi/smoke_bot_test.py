import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import smoke_bot  # noqa: E402


class FakeGame:
    def __init__(self):
        self.open = False
        self.clicks = []
        self.waits = []
        self.render_enabled = True

    def result(self, method, params=None):
        if method == "endpoints":
            return {"commands": [{"method": name} for name in smoke_bot.REQUIRED_METHODS]}
        if method == "command.describe":
            return {"method": params["method"], "params_shape": "{}", "result_shape": "{}"}
        if method == "render.info":
            return {"enabled": self.render_enabled}
        if method == "render.set_enabled":
            self.render_enabled = bool(params["enabled"])
            return {"enabled": self.render_enabled}
        if method == "ui.tree":
            nodes = [{"id_string": "settings/gear"}]
            return {"nodes": nodes}
        if method == "game.state":
            return {
                "schema": "template.game_state.snapshot.v1",
                "state": {
                    "player": {"x": 0, "z": 0, "yaw": 0, "spawned": True},
                    "settings": {"master_volume": 0.8, "music_volume": 0.7, "sfx_volume": 0.9},
                },
            }
        if method == "frame.current":
            return {"frame": 1}
        raise AssertionError(f"unexpected method: {method}")

    def wait_frames(self, frames=1):
        self.waits.append(frames)
        return {"frame": len(self.waits)}

    def click_ui(self, element_id, button="left", wait_frames=1, observe="frame.current"):
        self.clicks.append((element_id, button, wait_frames, observe))
        if element_id == "settings/gear":
            self.open = True
        return {"frame": len(self.clicks)}

    def capture_screenshot(self, output, wait_frames=1, audit=False):
        Path(output).parent.mkdir(parents=True, exist_ok=True)
        Path(output).write_bytes(b"\x89PNG\r\n\x1a\nfake")
        return output


class SmokeBotTest(unittest.TestCase):
    def test_extract_endpoint_methods_accepts_engine_shape(self):
        listing = {"commands": [{"method": "ui.tree"}, {"method": 7}, {"group": "bad"}, "legacy.ping"]}
        self.assertEqual(smoke_bot.extract_endpoint_methods(listing), {"ui.tree", "legacy.ping"})

    def test_missing_required_methods_reports_sorted_names(self):
        missing = smoke_bot.missing_required_methods({"endpoints", "ui.tree"})
        self.assertEqual(missing, sorted(set(smoke_bot.REQUIRED_METHODS) - {"endpoints", "ui.tree"}))

    def test_find_ui_node_uses_stable_id_string(self):
        tree = {"nodes": [{"id_string": "settings/gear", "label": "Settings"}]}
        self.assertEqual(smoke_bot.find_ui_node(tree, "settings/gear"), tree["nodes"][0])

    def test_validate_game_state_requires_template_snapshot_shape(self):
        state = {"schema": "template.game_state.snapshot.v1", "state": {"player": {}, "settings": {}}}
        self.assertIs(smoke_bot.validate_game_state(state), state)
        with self.assertRaises(smoke_bot.DevApiError):
            smoke_bot.validate_game_state({"schema": "wrong"})

    def test_default_executable_uses_ai_studio_game_exe_override(self):
        old = os.environ.get("AI_STUDIO_GAME_EXE")
        os.environ["AI_STUDIO_GAME_EXE"] = "C:/tmp/game.exe"
        try:
            self.assertEqual(smoke_bot.default_executable(), Path("C:/tmp/game.exe"))
        finally:
            if old is None:
                os.environ.pop("AI_STUDIO_GAME_EXE", None)
            else:
                os.environ["AI_STUDIO_GAME_EXE"] = old

    def test_run_smoke_toggles_render_and_writes_summary(self):
        with tempfile.TemporaryDirectory() as tmp:
            game = FakeGame()
            summary = smoke_bot.run_smoke(game, Path(tmp), audit=False)
            self.assertEqual(summary["stable_ui_id"], "settings/gear")
            self.assertEqual(summary["action"], "render.set_enabled false -> true")
            self.assertEqual(summary["game_state"]["schema"], "template.game_state.snapshot.v1")
            self.assertTrue(summary["render_enabled"]["enabled"])
            self.assertTrue(Path(summary["summary"]).exists())
            self.assertTrue(Path(summary["screenshot"]).exists())


if __name__ == "__main__":
    unittest.main()
