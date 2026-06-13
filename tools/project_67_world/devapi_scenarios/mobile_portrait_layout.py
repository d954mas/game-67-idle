#!/usr/bin/env python3
"""Capture phone-like native portrait layouts for 67 World visual QA."""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

from base import Scenario, fail_devapi, finish
from devapi_client import DevApiError, running_game


def set_field(game, path: str, value: Any) -> None:
    game.result("game.state.set", {"doc": "game", "path": path, "value": value})


def world(state: dict[str, Any]) -> dict[str, Any]:
    data = state.get("world_67", {})
    return data if isinstance(data, dict) else {}


def output_with_suffix(output: str, suffix: str) -> str:
    path = Path(output)
    ext = path.suffix or ".png"
    return str(path.with_name(f"{path.stem}_{suffix}{ext}"))


def fill_stuck_board(game) -> dict[str, Any]:
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
    return game.result("game.state")


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    output = sys.argv[2] if len(sys.argv) > 2 else "build/captures/scenarios/mobile_portrait_layout.png"
    window_size = sys.argv[3] if len(sys.argv) > 3 else "390x844"

    try:
        with running_game(port=port, fresh_state=True, window_size=window_size) as game:
            scenario = Scenario(game)
            state0 = game.result("game.reset_playtest")
            scenario.check("portrait viewport requested", window_size == "390x844" or "x" in window_size, window_size)
            scenario.check("ftue starts at spawn", world(state0).get("ftue_step") == "spawn_first", world(state0))
            scenario.check("spawn CTA starts as box tap", world(state0).get("spawn_action_label") == "TAP BOX", world(state0))
            game.result("game.action.spawn_67")
            game.result("game.action.spawn_67")
            game.result("game.action.merge_matching_67")
            game.result("frame.wait", {"frames": 20})
            first_loop = game.result("game.state")
            scenario.check("first loop reaches berry", first_loop.get("collection_discovered_count", 0) >= 2, first_loop)
            first_capture = scenario.capture(output_with_suffix(output, "first_loop"), wait_frames=40)

            stuck = fill_stuck_board(game)
            scenario.check("portrait full board has 12 used", world(stuck).get("board_used") == 12, world(stuck))
            scenario.check("portrait stuck CTA is free slot", world(stuck).get("spawn_action_label") == "FREE SLOT", world(stuck))
            stuck_capture = scenario.capture(output_with_suffix(output, "stuck"), wait_frames=40)

            print("first-loop screenshot:", first_capture)
            print("stuck screenshot:", stuck_capture)
            return finish(scenario.ok)
    except DevApiError as exc:
        return fail_devapi(exc)


if __name__ == "__main__":
    raise SystemExit(main())
