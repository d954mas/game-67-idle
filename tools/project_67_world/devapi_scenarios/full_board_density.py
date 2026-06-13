#!/usr/bin/env python3
"""Capture a dense full-board 67 World state for native visual QA."""

from __future__ import annotations

import sys

from typing import Any

from base import Scenario, fail_devapi, finish
from devapi_client import DevApiError, running_game


def set_field(game, path: str, value: Any) -> None:
    game.result("game.state.set", {"doc": "game", "path": path, "value": value})


def world(state: dict[str, Any]) -> dict[str, Any]:
    data = state.get("world_67", {})
    return data if isinstance(data, dict) else {}


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    output = sys.argv[2] if len(sys.argv) > 2 else "build/captures/scenarios/full_board_density.png"
    try:
        with running_game(port=port, fresh_state=True) as game:
            scenario = Scenario(game)
            game.result("game.reset_playtest")
            set_field(game, "tutorial.done", True)
            set_field(game, "collection_discovered_count", 12)
            set_field(game, "highest_variant_order", 12)
            for path in (
                "count_tiny_67",
                "count_berry_67",
                "count_banana_67",
                "count_smoothie_67",
                "count_cool_67",
                "count_portal_67",
                "count_mystery_67",
                "count_jelly_67",
                "count_lemon_67",
                "count_watermelon_67",
                "count_bubblegum_67",
                "count_sticker_67",
            ):
                set_field(game, path, 1)
            state = game.result("game.state")
            scenario.check("full board has 12 slots used", world(state).get("board_used") == 12, world(state))
            scenario.check("full board is stuck", world(state).get("can_recycle_lowest") is True, world(state))
            scenario.check("full board CTA is free slot", world(state).get("spawn_action_label") == "FREE SLOT", world(state))
            path = scenario.capture(output, wait_frames=30)
            print("screenshot:", path)
            return finish(scenario.ok)
    except DevApiError as exc:
        return fail_devapi(exc)


if __name__ == "__main__":
    raise SystemExit(main())
