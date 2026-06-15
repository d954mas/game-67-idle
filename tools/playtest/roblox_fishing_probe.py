#!/usr/bin/env python3
"""Drive the first Splash Rods fishing loop through native DevAPI."""

from __future__ import annotations

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "devapi"))

from devapi_client import DevApiError, running_game  # noqa: E402


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def fishing(state: dict) -> dict:
    summary = state.get("fishing_summary")
    require(isinstance(summary, dict), "missing fishing_summary")
    return summary


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    screenshot = os.path.join(ROOT, "tmp", "roblox_fishing", "native_first_slice.png")
    with running_game(port=port, fresh_state=True, autosave_enabled=False, window_size="960x540") as client:
        state = client.result("game.reset_playtest")
        require(fishing(state)["phase"] == "ready", "reset did not enter ready phase")

        for _ in range(4):
            state = client.step("game.fishing.primary", wait_frames=3)
            require(fishing(state)["phase"] in {"bite", "reeling"}, "primary did not cast/hook")
            before_count = fishing(state)["backpack_count"]
            for _tap in range(5):
                state = client.step("game.fishing.primary", wait_frames=3)
                if fishing(state)["backpack_count"] > before_count:
                    break
            require(fishing(state)["backpack_count"] >= 1, "reel did not catch fish")

        state = client.step("game.fishing.sell_all", wait_frames=3)
        require(fishing(state)["coins"] >= 30, "sell did not produce enough coins for first upgrade")
        state = client.step("game.fishing.upgrade.better_line", wait_frames=3)
        require(fishing(state)["better_line_level"] >= 1, "upgrade did not buy Better Line")
        require(fishing(state)["backpack_slots"] >= 6, "upgrade did not expand backpack")

        state = client.step("game.fishing.primary", wait_frames=3)
        before_count = fishing(state)["backpack_count"]
        for _tap in range(5):
            state = client.step("game.fishing.primary", wait_frames=3)
            if fishing(state)["backpack_count"] > before_count:
                break
        require(fishing(state)["total_catches"] >= 5, "post-upgrade catch did not complete")

        path = client.capture_screenshot(screenshot, wait_frames=4, audit=True)
        state = client.observe()
        require(fishing(state).get("glb_props_ready") is True, "GLB props system not initialized")
        require(fishing(state).get("mesh_instances", 0) >= 16, "mesh renderer did not draw all GLB props")
        require(fishing(state).get("mesh_draw_groups", 0) >= 6, "mesh renderer draw groups too low")
        print(path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, DevApiError) as exc:
        print(f"roblox_fishing_probe failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
