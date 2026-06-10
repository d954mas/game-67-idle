#!/usr/bin/env python3
"""Open settings modal and verify slider clicks update state."""

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
            scenario.require_endpoint("ui.element")
            scenario.require_endpoint("ui.click")
            scenario.require_endpoint("game.state.get")

            open_button = game.result("ui.element", {"id": "settings.open"})
            scenario.check("settings open button", open_button.get("role") == "button" and open_button.get("enabled") is True, open_button)

            game.click_ui("settings.open", wait_frames=2)
            modal = game.result("ui.element", {"id": "settings.modal"})
            master = game.result("ui.element", {"id": "settings.master_volume"})
            sfx = game.result("ui.element", {"id": "settings.sfx_volume"})
            scenario.check("settings modal visible", modal.get("role") == "dialog" and modal.get("visible") is True, modal)
            scenario.check("master slider visible", master.get("role") == "slider" and master.get("visible") is True, master)
            scenario.check("sfx slider visible", sfx.get("role") == "slider" and sfx.get("visible") is True, sfx)

            coins_before = game.result("game.state.get", {"doc": "game", "path": "meme_coins"})
            game.click_ui("main.do67", wait_frames=2)
            coins_after = game.result("game.state.get", {"doc": "game", "path": "meme_coins"})
            scenario.check("settings overlay blocks underlying ui", coins_after == coins_before, {"before": coins_before, "after": coins_after})

            game.click_ui("settings.master_volume", wait_frames=2)
            master_value = game.result("game.state.get", {"doc": "game", "path": "settings.master_volume"})
            scenario.check("master slider updates state", 0.45 <= master_value <= 0.55, master_value)

            game.click_ui("settings.sfx_volume", wait_frames=2)
            sfx_value = game.result("game.state.get", {"doc": "game", "path": "settings.sfx_volume"})
            scenario.check("sfx slider updates state", 0.45 <= sfx_value <= 0.55, sfx_value)

            game.click_ui("settings.close", wait_frames=2)
            closed = game.request("ui.element", {"id": "settings.modal"})
            scenario.check("settings modal closes", closed.get("ok") is False, closed)
            return finish(scenario.ok)
    except DevApiError as exc:
        return fail_devapi(exc)


if __name__ == "__main__":
    raise SystemExit(main())
