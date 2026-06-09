#!/usr/bin/env python3
"""Click the test UI button and verify text changes."""

from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from devapi_client import DevApiError, running_game
from base import Scenario, fail_devapi, finish

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9123


def main() -> int:
    try:
        with running_game(port=PORT) as game:
            scenario = Scenario(game)
            scenario.require_endpoint("ui.tree")
            scenario.require_endpoint("ui.click")

            button0 = game.result("ui.element", {"id": "test.button"})
            scenario.check("button initial text", button0.get("text") == "Click me", button0)

            state = scenario.click_and_observe("test.button", wait_frames=2)
            button1 = game.result("ui.element", {"id": "test.button"})
            label1 = game.result("ui.element", {"id": "test.label"})

            scenario.check("button text changed", button1.get("text") == "Clicked 1", button1)
            scenario.check("label text changed", label1.get("text") == "Label: clicked 1", label1)
            scenario.check("state mirrors text", state.get("test_button_text") == "Clicked 1" and state.get("test_label_text") == "Label: clicked 1", state)
            return finish(scenario.ok)
    except DevApiError as exc:
        return fail_devapi(exc)


if __name__ == "__main__":
    raise SystemExit(main())
