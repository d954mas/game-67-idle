#!/usr/bin/env python3
"""DevAPI smoke for the first Mech Builder Battler playable slice."""

from __future__ import annotations

import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "devapi"))

from devapi_client import DevApiError, running_game  # noqa: E402
from pixel_health import assert_pixel_health  # noqa: E402


SHOT_DIR = ROOT / "build" / "captures"


def check(name: str, condition: object, detail: object = None) -> bool:
    if condition:
        print(f"PASS {name}")
        return True
    print(f"FAIL {name}: {detail!r}", file=sys.stderr)
    return False


def capture(game, name: str) -> str:
    path = game.capture_screenshot(f"build/captures/{name}", wait_frames=6, audit=False)
    assert_pixel_health(path)
    print(f"SHOT {path}")
    return path


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9124
    SHOT_DIR.mkdir(parents=True, exist_ok=True)
    ok = True
    with running_game(port=port, fresh_state=True, window_size="1280x720") as game:
        endpoints = game.endpoint_methods()
        required = {
            "game.state",
            "game.capture.framebuffer",
            "game.action.start_battle",
            "game.action.use_special",
            "game.action.buy_rockets",
            "ui.tree",
            "ui.click",
            "frame.wait",
        }
        ok &= check("required endpoints", required.issubset(endpoints), sorted(required - endpoints))

        state = game.result("game.state")
        ok &= check("starts in hangar", state.get("screen") == "hangar", state)
        tree = game.result("ui.tree")
        ids = {node.get("id") for node in tree.get("nodes", [])}
        ok &= check("hangar ui", {"action.battle", "slot.shoulder"}.issubset(ids), sorted(ids))
        capture(game, "mech_t0021_hangar_smoke.png")

        state = game.click_ui("action.battle", wait_frames=12)
        ok &= check("battle starts", state.get("screen") == "battle" and state.get("alive_drones", 0) > 0, state)
        capture(game, "mech_t0021_battle_smoke.png")

        game.wait_frames(260)
        state = game.result("game.state")
        ok &= check("first battle reward", state.get("screen") == "reward" and state.get("salvage") == 120, state)
        capture(game, "mech_t0021_reward_smoke.png")

        state = game.click_ui("action.reward_continue", wait_frames=8)
        ok &= check("upgrade screen", state.get("screen") == "upgrade", state)
        capture(game, "mech_t0021_upgrade_smoke.png")

        state = game.click_ui("action.attach_rockets", wait_frames=10)
        ok &= check("rockets equipped", state.get("screen") == "retest" and state.get("rockets_equipped") is True, state)
        capture(game, "mech_t0021_retest_smoke.png")

        state = game.click_ui("action.retest", wait_frames=14)
        ok &= check("retest battle starts", state.get("screen") == "battle" and state.get("battle_index") == 1, state)
        capture(game, "mech_t0021_rockets_smoke.png")

    return 0 if ok else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except DevApiError as exc:
        print(f"FAIL devapi: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
