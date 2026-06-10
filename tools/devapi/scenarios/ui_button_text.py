#!/usr/bin/env python3
"""Click the main Game 67 button and verify state/UI changes."""

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

            button0 = game.result("ui.element", {"id": "main.do67"})
            scenario.check("do67 button available", button0.get("role") == "button" and button0.get("enabled") is True, button0)
            state0 = game.observe()

            state = scenario.click_and_observe("main.do67", wait_frames=2)
            button1 = game.result("ui.element", {"id": "main.do67"})
            label1 = game.result("ui.element", {"id": "main.coins"})

            scenario.check("button remains readable", button1.get("role") == "button" and button1.get("id") == "main.do67", button1)
            scenario.check("coin label updates", label1.get("role") == "label" and "coins" in str(label1.get("text", "")), label1)
            scenario.check("state increments meme coins", state.get("meme_coins", 0) > state0.get("meme_coins", 0), state)
            return finish(scenario.ok)
    except DevApiError as exc:
        return fail_devapi(exc)


if __name__ == "__main__":
    raise SystemExit(main())
