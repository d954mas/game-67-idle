#!/usr/bin/env python3
"""Verify Better Crate progression and stuck-board recovery in native 67 World."""

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


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    output = sys.argv[2] if len(sys.argv) > 2 else "build/captures/scenarios/better_crate_progression.png"
    try:
        with running_game(port=port, fresh_state=True) as game:
            scenario = Scenario(game)
            state0 = game.result("game.reset_playtest")
            scenario.check("better crate starts locked", world(state0).get("better_crate_state") == "locked", world(state0))
            scenario.check("speed HUD starts locked", world(state0).get("progress_upgrade_title") == "SPEED", world(state0))
            scenario.check("speed HUD shows lock", world(state0).get("progress_upgrade_value") == "LOCK", world(state0))
            scenario.check("spawn CTA starts as box tap", world(state0).get("spawn_action_label") == "TAP BOX", world(state0))

            set_field(game, "tutorial.done", True)
            set_field(game, "collection_discovered_count", 3)
            set_field(game, "highest_variant_order", 3)
            set_field(game, "wallet.soft", 1000)
            locked_without_speed = game.result("game.state")
            scenario.check(
                "better crate requires faster spawn",
                world(locked_without_speed).get("better_crate_state") == "locked",
                world(locked_without_speed),
            )
            scenario.check("speed HUD ready to buy", world(locked_without_speed).get("progress_upgrade_title") == "SPEED", world(locked_without_speed))
            scenario.check("speed HUD names buy cost", world(locked_without_speed).get("progress_upgrade_value") == "BUY25", world(locked_without_speed))

            set_field(game, "faster_spawn_bought", True)
            ready = game.result("game.state")
            scenario.check("better crate becomes ready", world(ready).get("can_buy_better_crate") is True, world(ready))
            scenario.check("crate HUD names next level", world(ready).get("progress_upgrade_title") == "BOX L1", world(ready))
            scenario.check("crate HUD names buy price", world(ready).get("progress_upgrade_value") == "BUY702", world(ready))
            bought1 = game.result("game.action.buy_better_crate")
            scenario.check("better crate level 1 buys", bought1.get("better_crate_level") == 1, bought1)
            scenario.check("crate HUD advances to level 2", world(bought1).get("progress_upgrade_title") == "BOX L2", world(bought1))
            scenario.check("crate HUD switches to need state", str(world(bought1).get("progress_upgrade_value", "")).startswith("NEED"), world(bought1))
            spawned_berry = game.result("game.action.spawn_67")
            scenario.check("level 1 crate spawns berry", spawned_berry.get("count_berry_67") == 1, spawned_berry)
            primary_capture = scenario.capture(output, wait_frames=80)

            set_field(game, "wallet.soft", 5000)
            bought2 = game.result("game.action.buy_better_crate")
            scenario.check("better crate level 2 buys", bought2.get("better_crate_level") == 2, bought2)
            spawned_banana = game.result("game.action.spawn_67")
            scenario.check("level 2 crate spawns banana", spawned_banana.get("count_banana_67") == 1, spawned_banana)

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
            stuck = game.result("game.state")
            scenario.check("stuck board detected", world(stuck).get("can_recycle_lowest") is True, world(stuck))
            scenario.check("stuck board CTA is clear", world(stuck).get("spawn_action_label") == "FREE SLOT", world(stuck))
            output_path = Path(output)
            suffix = output_path.suffix or ".png"
            stuck_output = output_path.with_name(f"{output_path.stem}_stuck{suffix}")
            stuck_capture = scenario.capture(str(stuck_output), wait_frames=80)
            recycled = game.result("game.action.recycle_lowest")
            scenario.check("recycle frees one slot", world(recycled).get("board_used") == 11, world(recycled))
            scenario.check("recycle removes lowest tiny", recycled.get("count_tiny_67") == 0, recycled)
            scenario.check("crate can spawn after recycle", world(recycled).get("can_spawn") is True, world(recycled))
            scenario.check("spawn CTA returns to box tap", world(recycled).get("spawn_action_label") == "TAP BOX", world(recycled))

            print("screenshot:", primary_capture)
            print("stuck screenshot:", stuck_capture)
            return finish(scenario.ok)
    except DevApiError as exc:
        return fail_devapi(exc)


if __name__ == "__main__":
    raise SystemExit(main())
