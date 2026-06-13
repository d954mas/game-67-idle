#!/usr/bin/env python3
"""Verify 67 World release-track 30-variant progression through DevAPI."""

from __future__ import annotations

import sys

from typing import Any

from base import Scenario, fail_devapi, finish
from devapi_client import DevApiError, running_game


def set_field(game, path: str, value: Any) -> None:
    game.result("game.state.set", {"doc": "game", "path": path, "value": value})


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    output = sys.argv[2] if len(sys.argv) > 2 else "build/captures/scenarios/extended_67_progression.png"
    try:
        with running_game(port=port, fresh_state=True) as game:
            scenario = Scenario(game)
            state0 = game.result("game.reset_playtest")
            variants0 = state0.get("world_67", {}).get("variants", [])
            scenario.check("30 release-track variants are exposed", len(variants0) == 30, variants0)
            scenario.check("first batch still starts at tiny", variants0[0].get("id") == "tiny_67", variants0[:2])
            scenario.check("release track ends at cosmic", variants0[-1].get("id") == "cosmic_67", variants0[-3:])

            set_field(game, "collection_discovered_count", 7)
            set_field(game, "highest_variant_order", 7)
            set_field(game, "count_mystery_67", 2)
            after_jelly = game.result("game.action.merge_matching_67")
            scenario.check("mystery merge discovers jelly", after_jelly.get("count_jelly_67") == 1, after_jelly)
            scenario.check("jelly advances collection", after_jelly.get("collection_discovered_count") == 8, after_jelly)
            scenario.check("next goal advances to lemon", after_jelly.get("world_67", {}).get("next_goal") == "NEXT LEMON", after_jelly.get("world_67", {}))

            game.result("game.reset_playtest")
            set_field(game, "tutorial.done", True)
            set_field(game, "collection_discovered_count", 17)
            set_field(game, "highest_variant_order", 17)
            set_field(game, "faster_spawn_bought", True)
            set_field(game, "count_rocket_67", 2)
            after_rainbow = game.result("game.action.merge_matching_67")
            scenario.check("rocket merge discovers rainbow", after_rainbow.get("count_rainbow_67") == 1, after_rainbow)
            scenario.check("rainbow advances to order 18", after_rainbow.get("collection_discovered_count") == 18, after_rainbow)
            scenario.check("rainbow points to neon", after_rainbow.get("world_67", {}).get("next_goal") == "NEXT NEON", after_rainbow.get("world_67", {}))

            game.result("game.reset_playtest")
            set_field(game, "tutorial.done", True)
            set_field(game, "collection_discovered_count", 18)
            set_field(game, "highest_variant_order", 18)
            set_field(game, "faster_spawn_bought", True)
            set_field(game, "count_rainbow_67", 2)
            after_neon = game.result("game.action.merge_matching_67")
            scenario.check("rainbow merge discovers neon", after_neon.get("count_neon_67") == 1, after_neon)
            scenario.check("neon advances collection", after_neon.get("collection_discovered_count") == 19, after_neon)
            scenario.check("next goal advances to gummy", after_neon.get("world_67", {}).get("next_goal") == "NEXT GUMMY", after_neon.get("world_67", {}))

            game.result("game.reset_playtest")
            set_field(game, "tutorial.done", True)
            set_field(game, "collection_discovered_count", 29)
            set_field(game, "highest_variant_order", 29)
            set_field(game, "faster_spawn_bought", True)
            set_field(game, "count_golden_67", 2)
            after_cosmic = game.result("game.action.merge_matching_67")
            scenario.check("golden merge discovers cosmic", after_cosmic.get("count_cosmic_67") == 1, after_cosmic)
            scenario.check("cosmic is release-track cap", after_cosmic.get("collection_discovered_count") == 30, after_cosmic)
            scenario.check("release cap reports complete", after_cosmic.get("world_67", {}).get("next_goal") == "WORLD COMPLETE", after_cosmic.get("world_67", {}))

            path = scenario.capture(output, wait_frames=130)
            print("screenshot:", path)
            return finish(scenario.ok)
    except DevApiError as exc:
        return fail_devapi(exc)


if __name__ == "__main__":
    raise SystemExit(main())
