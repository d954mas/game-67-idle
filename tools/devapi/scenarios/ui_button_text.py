#!/usr/bin/env python3
"""Verify the 67 World buttons are visible and change state."""

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
        ok &= scenario.check("67 controls in ui tree", {"world.spawn", "world.slot.00", "world.slot.01", "world.upgrade"}.issubset(ids), tree)
        state0 = game.result("game.state")
        state1 = game.click_ui("world.spawn", wait_frames=2)
        ok &= scenario.check("spawn button adds tiny 67", state1.get("count_tiny_67", 0) > state0.get("count_tiny_67", 0), state1)
        game.click_ui("world.spawn", wait_frames=1)
        game.click_ui("world.slot.00", wait_frames=1)
        state2 = game.click_ui("world.slot.01", wait_frames=2)
        ok &= scenario.check("merge button discovers berry 67", state2.get("count_berry_67", 0) >= 1, state2)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
