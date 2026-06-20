#!/usr/bin/env python3
"""DevAPI smoke for the first Mech Builder Battler playable slice."""

from __future__ import annotations

import sys
from argparse import ArgumentParser
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "devapi"))

from devapi_client import DevApiError, running_game  # noqa: E402
from pixel_health import assert_pixel_health  # noqa: E402


SHOT_DIR = ROOT / "build" / "captures"
SUITE_ORDER = [
    "contract",
    "asset-load",
    "visual-framing",
    "movement",
    "combat-pacing",
    "reward-loop",
    "upgrade-special",
]


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


def wait_for_screen(game, screen: str, max_frames: int = 3600, step_frames: int = 60) -> dict:
    state = game.result("game.state")
    waited = 0
    while state.get("screen") != screen and waited < max_frames:
        game.wait_frames(step_frames)
        waited += step_frames
        state = game.result("game.state")
    state["waited_frames"] = waited
    return state


def parse_args(argv: list[str]):
    parser = ArgumentParser(description="Mech Builder Battler DevAPI smoke suites.")
    parser.add_argument("port", nargs="?", type=int, default=9124)
    parser.add_argument(
        "--suite",
        action="append",
        default=[],
        help=f"run one suite; repeatable or comma-separated ({', '.join(SUITE_ORDER)})",
    )
    parser.add_argument("--movement-only", action="store_true", help="legacy alias for suites through combat-pacing")
    parser.add_argument("--list-suites", action="store_true")
    args = parser.parse_args(argv)
    if args.list_suites:
        print("\n".join(SUITE_ORDER))
        raise SystemExit(0)
    if args.movement_only:
        args.suites = ["contract", "asset-load", "visual-framing", "movement", "combat-pacing"]
        return args
    suites: list[str] = []
    for raw in args.suite:
        suites.extend(part.strip() for part in raw.split(",") if part.strip())
    unknown = sorted(set(suites) - set(SUITE_ORDER))
    if unknown:
        parser.error(f"unknown suite(s): {', '.join(unknown)}")
    selected = set(suites)
    args.suites = [suite for suite in SUITE_ORDER if suite in selected] if selected else list(SUITE_ORDER)
    return args


def suite_heading(name: str) -> None:
    print(f"SUITE {name}")


def run_contract(game, ctx: dict) -> bool:
    if ctx.get("contract"):
        return True
    suite_heading("contract")
    SHOT_DIR.mkdir(parents=True, exist_ok=True)
    ok = True
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
    ok &= check("starts in hangar", state.get("screen") == "hangar", state)
    tree = game.result("ui.tree")
    ids = {node.get("id") for node in tree.get("nodes", [])}
    ok &= check("hangar ui", {"action.battle", "slot.shoulder"}.issubset(ids), sorted(ids))
    capture(game, "mech_t0021_hangar_smoke.png")
    ctx["contract"] = True
    ctx["state"] = state
    return ok


def run_asset_load(game, ctx: dict) -> bool:
    ok = run_contract(game, ctx)
    if ctx.get("asset-load"):
        return ok
    suite_heading("asset-load")
    state = game.result("game.state")
    ok &= check("mesh mech ready", state.get("mesh_mech_ready") is True, state)
    capture(game, "mech_t0025_hero_toy_material_smoke.png", wait_frames=1)
    capture(game, "mech_t0026_hero_modular_overlay_hangar_smoke.png", wait_frames=1)
    capture(game, "mech_t0027_assault_walker_hero_hangar_smoke.png", wait_frames=1)
    capture(game, "mech_t0032_assault_kitbash_hangar_smoke.png", wait_frames=1)
    capture(game, "mech_t0033_stylized_studs_world_hangar_smoke.png", wait_frames=1)
    capture(game, "mech_t0034_textured_floor_hangar_smoke.png", wait_frames=1)
    capture(game, "mech_t0035_asset_first_sentinel_hangar_smoke.png", wait_frames=1)
    capture(game, "mech_t0036_kenney_space_props_hangar_smoke.png", wait_frames=1)
    capture(game, "mech_t0037_station_plastic_hangar_smoke.png", wait_frames=1)
    capture(game, "mech_t0029_arena_dressing_hangar_smoke.png", wait_frames=1)
    capture(game, "mech_t0030_lighting_material_hangar_smoke.png", wait_frames=1)
    ctx["asset-load"] = True
    ctx["state"] = state
    return ok


def start_battle(game, ctx: dict) -> tuple[dict, bool]:
    if ctx.get("battle"):
        return ctx["state"], True
    ok = run_contract(game, ctx)
    state = game.click_ui("action.battle", wait_frames=12)
    ok &= check("battle starts", state.get("screen") == "battle" and state.get("alive_drones", 0) > 0, state)
    ctx["battle"] = True
    ctx["state"] = state
    return state, ok


def run_visual_framing(game, ctx: dict) -> bool:
    state, ok = start_battle(game, ctx)
    if ctx.get("visual-framing"):
        return ok
    suite_heading("visual-framing")
    capture(game, "mech_t0032_assault_kitbash_battle_entry_smoke.png", wait_frames=1)
    capture(game, "mech_t0034_textured_floor_battle_entry_smoke.png", wait_frames=1)
    capture(game, "mech_t0030_lighting_material_battle_smoke.png", wait_frames=1)
    capture(game, "mech_t0031_combat_clarity_battle_smoke.png", wait_frames=1)
    capture(game, "mech_t0032_assault_kitbash_battle_smoke.png", wait_frames=1)
    capture(game, "mech_t0033_stylized_studs_world_battle_smoke.png", wait_frames=1)
    capture(game, "mech_t0034_textured_floor_battle_smoke.png", wait_frames=1)
    capture(game, "mech_t0021_battle_smoke.png", wait_frames=1)
    capture(game, "mech_t0024_robot_enemy_asset_smoke.png", wait_frames=1)
    capture(game, "mech_t0027_assault_walker_battle_smoke.png", wait_frames=1)
    capture(game, "mech_t0029_arena_dressing_battle_smoke.png", wait_frames=1)
    ctx["visual-framing"] = True
    ctx["state"] = state
    return ok


def run_movement(game, ctx: dict) -> bool:
    state, ok = start_battle(game, ctx)
    if ctx.get("movement"):
        return ok
    suite_heading("movement")
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
    capture(game, "mech_t0038_hero_material_attack_read_smoke.png", wait_frames=1)
    capture(game, "mech_t0039_hero_motion_attack_smoke.png", wait_frames=1)
    capture(game, "mech_t0028_assault_walker_cannon_recoil_smoke.png", wait_frames=1)
    capture(game, "mech_t0028_assault_walker_early_motion_smoke.png", wait_frames=1)
    capture(game, "mech_t0023_moving_strafe_smoke.png", wait_frames=1)
    capture(game, "mech_t0026_hero_modular_motion_smoke.png", wait_frames=1)
    capture(game, "mech_t0027_assault_walker_motion_smoke.png", wait_frames=1)
    game.result("input.key", {"key": "D", "down": False})
    game.result("input.key", {"key": "W", "down": False})
    game.wait_frames(8)
    game.wait_frames(22)
    capture(game, "mech_t0023_cannon_attack_smoke.png")
    ctx["movement"] = True
    ctx["state"] = game.result("game.state")
    return ok


def run_combat_pacing(game, ctx: dict) -> bool:
    ok = run_movement(game, ctx)
    if ctx.get("combat-pacing"):
        return ok
    suite_heading("combat-pacing")
    game.wait_frames(260)
    state = game.result("game.state")
    ok &= check(
        "first battle remains playable after movement review",
        state.get("screen") == "battle" and state.get("alive_drones", 0) > 0,
        state,
    )
    capture(game, "mech_t0040_larger_slower_battle_smoke.png", wait_frames=1)
    ctx["combat-pacing"] = True
    ctx["state"] = state
    return ok


def run_reward_loop(game, ctx: dict) -> bool:
    _, ok = start_battle(game, ctx)
    if ctx.get("reward-loop"):
        return ok
    suite_heading("reward-loop")
    state = wait_for_screen(game, "reward", max_frames=4200)
    ok &= check("first battle reward", state.get("screen") == "reward" and state.get("salvage") == 120, state)
    capture(game, "mech_t0021_reward_smoke.png")

    state = game.click_ui("action.reward_continue", wait_frames=8)
    ok &= check("upgrade screen", state.get("screen") == "upgrade", state)
    capture(game, "mech_t0021_upgrade_smoke.png")
    ctx["battle"] = False
    ctx["reward-loop"] = True
    ctx["upgrade"] = True
    ctx["state"] = state
    return ok


def run_upgrade_special(game, ctx: dict) -> bool:
    ok = run_reward_loop(game, ctx)
    if ctx.get("upgrade-special"):
        return ok
    suite_heading("upgrade-special")
    state = game.click_ui("action.attach_rockets", wait_frames=10)
    ok &= check("rockets equipped", state.get("screen") == "retest" and state.get("rockets_equipped") is True, state)
    capture(game, "mech_t0021_retest_smoke.png")

    state = game.click_ui("action.retest", wait_frames=14)
    ok &= check("retest battle starts", state.get("screen") == "battle" and state.get("battle_index") == 1, state)
    game.wait_frames(150)
    state = game.result("game.action.use_special")
    ok &= check("rocket attack fired", state.get("rockets_equipped") is True and state.get("heat", 0) > 0.3, state)
    capture(game, "mech_t0021_rockets_smoke.png", wait_frames=1)
    capture(game, "mech_t0023_rocket_attack_smoke.png", wait_frames=1)
    ctx["upgrade-special"] = True
    ctx["state"] = state
    return ok


SUITE_RUNNERS = {
    "contract": run_contract,
    "asset-load": run_asset_load,
    "visual-framing": run_visual_framing,
    "movement": run_movement,
    "combat-pacing": run_combat_pacing,
    "reward-loop": run_reward_loop,
    "upgrade-special": run_upgrade_special,
}


def main() -> int:
    args = parse_args(sys.argv[1:])
    SHOT_DIR.mkdir(parents=True, exist_ok=True)
    ok = True
    with running_game(port=args.port, fresh_state=True, window_size="1280x720") as game:
        ctx: dict = {}
        for suite in args.suites:
            ok &= SUITE_RUNNERS[suite](game, ctx)
    return 0 if ok else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except DevApiError as exc:
        print(f"FAIL devapi: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
