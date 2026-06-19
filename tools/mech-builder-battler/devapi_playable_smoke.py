#!/usr/bin/env python3
"""DevAPI smoke for the first Mech Builder Battler playable slice."""

from __future__ import annotations

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


def capture(game, name: str, wait_frames: int = 6) -> str:
    path = game.capture_screenshot(f"build/captures/{name}", wait_frames=wait_frames, audit=False)
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
            "input.key",
        }
        ok &= check("required endpoints", required.issubset(endpoints), sorted(required - endpoints))

        game.wait_frames(20)
        state = game.result("game.state")
        ok &= check("mesh mech ready", state.get("mesh_mech_ready") is True, state)
        ok &= check("starts in hangar", state.get("screen") == "hangar", state)
        tree = game.result("ui.tree")
        ids = {node.get("id") for node in tree.get("nodes", [])}
        ok &= check("hangar ui", {"action.battle", "slot.shoulder"}.issubset(ids), sorted(ids))
        capture(game, "mech_t0021_hangar_smoke.png")
        capture(game, "mech_t0025_hero_toy_material_smoke.png", wait_frames=1)
        capture(game, "mech_t0026_hero_modular_overlay_hangar_smoke.png", wait_frames=1)
        capture(game, "mech_t0027_assault_walker_hero_hangar_smoke.png", wait_frames=1)
        capture(game, "mech_t0029_arena_dressing_hangar_smoke.png", wait_frames=1)
        capture(game, "mech_t0030_lighting_material_hangar_smoke.png", wait_frames=1)

        state = game.click_ui("action.battle", wait_frames=12)
        ok &= check("battle starts", state.get("screen") == "battle" and state.get("alive_drones", 0) > 0, state)
        start_x = float(state.get("mech_x", 0.0))
        start_z = float(state.get("mech_z", 0.0))
        game.result("input.key", {"key": "D", "down": True})
        game.result("input.key", {"key": "W", "down": True})
        game.wait_frames(8)
        game.wait_frames(10)
        state = game.result("game.state")
        moved_x = abs(float(state.get("mech_x", 0.0)) - start_x)
        moved_z = abs(float(state.get("mech_z", 0.0)) - start_z)
        moved = moved_x > 0.25 or moved_z > 0.25
        ok &= check("wasd movement changes mech position", moved, state)
        capture(game, "mech_t0030_lighting_material_battle_smoke.png", wait_frames=1)
        capture(game, "mech_t0021_battle_smoke.png", wait_frames=1)
        capture(game, "mech_t0024_robot_enemy_asset_smoke.png", wait_frames=1)
        capture(game, "mech_t0027_assault_walker_battle_smoke.png", wait_frames=1)
        capture(game, "mech_t0028_assault_walker_cannon_recoil_smoke.png", wait_frames=1)
        capture(game, "mech_t0029_arena_dressing_battle_smoke.png", wait_frames=1)
        capture(game, "mech_t0028_assault_walker_early_motion_smoke.png", wait_frames=1)
        capture(game, "mech_t0023_moving_strafe_smoke.png", wait_frames=1)
        capture(game, "mech_t0026_hero_modular_motion_smoke.png", wait_frames=1)
        capture(game, "mech_t0027_assault_walker_motion_smoke.png", wait_frames=1)
        game.result("input.key", {"key": "D", "down": False})
        game.result("input.key", {"key": "W", "down": False})
        game.wait_frames(8)
        game.wait_frames(22)
        capture(game, "mech_t0023_cannon_attack_smoke.png")

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
        game.wait_frames(18)
        state = game.result("game.action.use_special")
        ok &= check("rocket attack fired", state.get("rockets_equipped") is True and state.get("heat", 0) > 0.3, state)
        capture(game, "mech_t0021_rockets_smoke.png", wait_frames=1)
        capture(game, "mech_t0023_rocket_attack_smoke.png", wait_frames=1)

    return 0 if ok else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except DevApiError as exc:
        print(f"FAIL devapi: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
