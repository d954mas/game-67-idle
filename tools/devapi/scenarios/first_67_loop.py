#!/usr/bin/env python3
"""Drive the first 67 World playable loop through DevAPI."""

from __future__ import annotations

import sys

from base import Scenario, fail_devapi, finish
from devapi_client import DevApiError, running_game


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    output = sys.argv[2] if len(sys.argv) > 2 else "build/captures/scenarios/first_67_loop.png"
    try:
        with running_game(port=port, fresh_state=True) as game:
            scenario = Scenario(game)
            state0 = game.result("game.reset_playtest")
            scenario.check("starts empty", state0.get("world_67", {}).get("board_used") == 0, state0)
            scenario.check("ftue starts at spawn", state0.get("world_67", {}).get("ftue_step") == "spawn_first", state0.get("world_67", {}))
            scenario.check("tutorial not done at start", state0.get("tutorial", {}).get("done") is False, state0.get("tutorial", {}))
            scenario.check("speed upgrade starts locked", state0.get("world_67", {}).get("faster_spawn_state") == "locked", state0.get("world_67", {}))

            s1 = game.click_ui("world.spawn", wait_frames=1)
            scenario.check("first spawn", s1.get("count_tiny_67") == 1, s1)
            scenario.check("ftue asks for second spawn", s1.get("world_67", {}).get("ftue_step") == "spawn_second", s1.get("world_67", {}))
            s2 = game.click_ui("world.spawn", wait_frames=1)
            scenario.check("second spawn", s2.get("count_tiny_67") == 2, s2)
            scenario.check("ftue asks for merge", s2.get("world_67", {}).get("ftue_step") == "merge_pair", s2.get("world_67", {}))
            scenario.check(
                "merge hint points at first pair",
                s2.get("world_67", {}).get("merge_hint_slot_a") == 0 and s2.get("world_67", {}).get("merge_hint_slot_b") == 1,
                s2.get("world_67", {}),
            )
            game.click_ui("world.slot.00", wait_frames=1)
            s3 = game.click_ui("world.slot.01", wait_frames=2)
            scenario.check("merge discovers berry", s3.get("count_berry_67") == 1 and s3.get("collection_discovered_count") >= 2, s3)
            scenario.check("tutorial completes on first merge", s3.get("tutorial", {}).get("done") is True, s3.get("tutorial", {}))
            scenario.check("next goal advances", s3.get("world_67", {}).get("next_goal") == "NEXT BANANA", s3.get("world_67", {}))
            scenario.check("merge hint clears after merge", s3.get("world_67", {}).get("merge_hint_slot_a") is None, s3.get("world_67", {}))
            scenario.check("speed upgrade starts saving", s3.get("world_67", {}).get("faster_spawn_state") == "saving", s3.get("world_67", {}))

            for _ in range(8):
                game.result("game.action.spawn_67")
            game.result("game.action.merge_matching_67")
            game.result("game.action.merge_matching_67")
            state = game.result("game.state")
            scenario.check("loop exposes progression", state.get("collection_discovered_count", 0) >= 2, state)
            discovered = state.get("collection_discovered_count", 0)
            variants = state.get("world_67", {}).get("variants", [])
            flags_match = all(item.get("discovered") is (idx < discovered) for idx, item in enumerate(variants))
            scenario.check("collection lock flags match progress", flags_match, variants)
            game.result("game.state.set", {"doc": "game", "path": "wallet.soft", "value": 25})
            ready = game.result("game.state")
            scenario.check("speed upgrade becomes ready", ready.get("world_67", {}).get("faster_spawn_state") == "ready", ready.get("world_67", {}))
            bought = game.click_ui("world.upgrade", wait_frames=2)
            scenario.check("speed upgrade buys", bought.get("faster_spawn_bought") is True, bought)
            scenario.check("speed upgrade shows bought", bought.get("world_67", {}).get("faster_spawn_state") == "bought", bought.get("world_67", {}))
            path = scenario.capture(output, wait_frames=130)
            print("screenshot:", path)
            return finish(scenario.ok)
    except DevApiError as exc:
        return fail_devapi(exc)


if __name__ == "__main__":
    raise SystemExit(main())
