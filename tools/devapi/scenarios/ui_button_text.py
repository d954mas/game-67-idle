#!/usr/bin/env python3
"""Verify the seed button is visible and changes state."""

from __future__ import annotations

import sys

from base import Scenario
from devapi_client import running_game


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    ok = True
    with running_game(port=port, fresh_state=True) as game:
        scenario = Scenario(game)
        tree = game.result("ui.tree")
        ids = {node.get("id") for node in tree}
        ok &= scenario.check("seed button in ui tree", "seed.cycle" in ids, tree)
        state0 = game.result("game.state")
        state1 = game.click_ui("seed.cycle", wait_frames=2)
        ok &= scenario.check("seed button increments state", state1.get("test_ui_clicks", 0) > state0.get("test_ui_clicks", 0), state1)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
