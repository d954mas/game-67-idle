import unittest
from pathlib import Path
from contextlib import contextmanager

from ai_studio.runtime_automation.responsive_viewports import ViewportCase
from ai_studio.runtime_automation.window_layout_capture import capture_window
from ai_studio.runtime_automation.window_layout_contract_test import tree


class FakeGame:
    def __init__(self):
        self.calls = []

    def result(self, method, params=None):
        self.calls.append((method, params))
        if method == "ui.tree":
            return tree()
        return {"queued": True}

    def wait_frames(self, frames):
        self.calls.append(("frame.wait", frames))

    @contextmanager
    def player_gated(self):
        self.calls.append(("player.gate", True))
        yield self
        self.calls.append(("player.gate", False))

    def capture_screenshot(self, path, wait_frames=0):
        self.calls.append(("capture", path))


class WindowLayoutCaptureTest(unittest.TestCase):
    def test_opens_scene_and_probes_wheel_without_game_branches(self):
        game = FakeGame()
        result = capture_window(game, {"id": "settings/window", "scene": "settings"},
                                ViewportCase("portrait", 390, 844), Path("proof"))
        self.assertEqual(game.calls[0], ("game.scene.show", {"scene": "settings"}))
        self.assertIn(("ui.scroll", {"id": "settings/window", "dy": 2}), game.calls)
        self.assertIn("settings/window", result["after_scroll"])


if __name__ == "__main__":
    unittest.main()
