#!/usr/bin/env python3
"""Smoke test for the 67 World runtime and universal DevAPI surface."""

from __future__ import annotations

import sys

from devapi_client import running_game


def check(name: str, condition: bool, detail: object = None) -> bool:
    if condition:
        print(f"PASS {name}")
        return True
    print(f"FAIL {name}: {detail!r}", file=sys.stderr)
    return False


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    ok = True
    with running_game(port=port, fresh_state=True) as game:
        endpoints = set(game.result("endpoints"))
        required = {
            "ping",
            "endpoints",
            "frame.wait",
            "ui.tree",
            "ui.click",
            "game.state",
            "game.reset_playtest",
            "game.action.spawn_67",
            "game.action.merge_matching_67",
            "game.action.buy_faster_spawn",
            "game.state.schema",
            "game.state.get",
            "game.state.set",
            "game.state.save",
            "game.state.load",
        }
        ok &= check("required endpoints", required.issubset(endpoints), sorted(required - endpoints))

        state0 = game.result("game.reset_playtest")
        ok &= check(
            "reset returns 67 world state",
            state0.get("wallet", {}).get("soft") == 0
            and state0.get("collection_discovered_count") == 1
            and state0.get("world_67", {}).get("board_used") == 0,
            state0,
        )

        tree = game.result("ui.tree")
        ids = {node.get("id") for node in tree}
        ok &= check("ui tree exposes 67 controls", {"root", "world.board", "world.slot.00", "world.slot.01", "world.spawn", "world.upgrade"}.issubset(ids), tree)

        clicked = game.click_ui("world.spawn", wait_frames=2)
        ok &= check("ui.click spawns tiny 67", clicked.get("count_tiny_67", 0) >= 1, clicked)
        game.click_ui("world.spawn", wait_frames=1)
        game.click_ui("world.slot.00", wait_frames=1)
        merged = game.click_ui("world.slot.01", wait_frames=2)
        ok &= check("ui.click merges to berry 67", merged.get("count_berry_67", 0) >= 1 and merged.get("collection_discovered_count", 0) >= 2, merged)

        patched = game.result("game.state.set", {"doc": "game", "path": "settings.master_volume", "value": 0.5})
        ok &= check("state set works", abs(patched.get("settings", {}).get("master_volume", 0.0) - 0.5) < 0.01, patched)

        shot = game.capture_screenshot("build/captures/devapi_smoke.png", wait_frames=2, audit=True)
        ok &= check("screenshot captured", bool(shot), shot)

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
