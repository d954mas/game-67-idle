#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import sys
from datetime import datetime, timezone
from typing import Any

sys.path.insert(0, "tools/devapi")
from devapi_client import running_game  # noqa: E402


REQUIRED_EVENTS = {
    "rune_session_start",
    "rune_ftue_first_action",
    "rune_combat_first_action",
    "rune_ftue_first_reward",
    "rune_upgrade_spark_ward_1",
    "rune_choice_bell_rope",
    "rune_route_reedmere_open",
    "rune_route_greenfen_open",
    "rune_level_warden_rank_2",
    "rune_upgrade_spark_ward_2",
    "rune_route_post_greenfen_choice",
    "rune_route_briar_clear",
    "rune_route_ashen_cairn_open",
}

FUTURE_IDLE_EVENTS = {
    "rune_stall_30s",
    "rune_session_stop",
}

TARGET_SECONDS = {
    "rune_ftue_first_action": 5,
    "rune_combat_first_action": 20,
    "rune_ftue_first_reward": 45,
    "rune_upgrade_spark_ward_1": 120,
    "rune_level_warden_rank_2": 300,
    "rune_upgrade_spark_ward_2": 420,
    "rune_route_post_greenfen_choice": 480,
    "rune_route_briar_clear": 540,
    "rune_route_ashen_cairn_open": 600,
}


def require(condition: bool, message: str, detail: object) -> None:
    if not condition:
        raise AssertionError(f"{message}: {detail!r}")


def telemetry_by_id(payload: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {event["id"]: event for event in payload.get("events", [])}


def run_first_session(game: Any) -> dict[str, Any]:
    state = game.result("game.reset_playtest")
    require(state["rune_summary"]["location"] == "miregate", "fresh location", state)

    # Wispfen intro and first reward.
    state = game.result("game.rune.scout")
    require(state["rune_summary"]["encounter"] == "mire_wisp", "first scout", state)
    state = game.result("game.rune.spark")
    state = game.result("game.rune.spark")
    require(state["rune"]["silver"] == 6 and state["rune"]["sparks"] == 1, "first reward", state)

    # Second wisp and side choice.
    state = game.result("game.rune.scout")
    state = game.result("game.rune.spark")
    state = game.result("game.rune.strike")
    state = game.result("game.rune.strike")
    require(state["rune_summary"]["can_choose_bell_rope"], "bell rope choice", state)
    state = game.result("game.rune.quest.bell_rope.kindness")
    require(state["rune"]["kindness_reputation"] == 1, "kindness reward", state)

    # First upgrade and routes.
    state = game.result("game.rune.upgrade.spark_ward")
    require(state["rune"]["spell_level"] == 1 and state["rune"]["tower_unlocked"], "Spark Ward I", state)
    state = game.result("game.rune.inspect_tower")
    require(state["rune"]["east_road_unlocked"], "Reedmere route", state)
    state = game.result("game.rune.scout_east")
    state = game.result("game.rune.spark")
    state = game.result("game.rune.strike")
    state = game.result("game.rune.strike")
    require(state["rune"]["east_road_safety"] == 1, "Reedmere clear", state)

    # Optional payoff into Greenfen.
    state = game.result("game.rune.light_moss_shrine")
    require(state["rune"]["spirit_favor"] == 1, "Moss Shrine", state)
    state = game.result("game.rune.open_causeway")
    require(state["rune"]["causeway_unlocked"], "Greenfen route", state)
    state = game.result("game.rune.rest")
    require(state["rune"]["hp"] == 20 and state["rune"]["mana"] == 12, "pre-Greenfen rest", state)
    state = game.result("game.rune.scout_greenfen")
    state = game.result("game.rune.spark")
    state = game.result("game.rune.spark")
    require(state["rune"]["player_level"] == 2 and state["rune_summary"]["hp_max"] == 24, "Warden Rank II", state)
    require(state["rune"]["rune_lore"] == 1, "Greenfen lore", state)
    state = game.result("game.rune.upgrade.spark_ward_2")
    require(state["rune"]["spell_level"] == 2 and state["rune_summary"]["spark_damage"] == 9, "Spark Ward II", state)
    require(state["rune_summary"]["can_choose_next_route"], "route choice available", state)
    state = game.result("game.rune.choose_briar_gate")
    require(state["rune_summary"]["route_choice"] == "briar_gate" and state["rune"]["main_quest_step"] == 14, "Briar Gate route choice", state)
    state = game.result("game.rune.scout_briar_gate")
    require(state["rune_summary"]["encounter"] == "briar_stalker" and state["rune"]["main_quest_step"] == 15, "Briar Gate first threat", state)
    state = game.result("game.rune.spark")
    state = game.result("game.rune.spark")
    require(state["rune"]["briar_gate_safety"] == 1 and state["rune"]["main_quest_step"] == 16, "Briar Gate clear", state)
    state = game.result("game.rune.discover_ashen_cairn")
    require(state["rune"]["ashen_cairn_unlocked"] and state["rune"]["main_quest_step"] == 18, "Ashen Cairn route hook", state)
    return game.result("game.rune.telemetry")


def build_report(telemetry: dict[str, Any], window_size: str, screenshot: str | None) -> tuple[dict[str, Any], bool]:
    events = telemetry_by_id(telemetry)
    missing = sorted(event_id for event_id in REQUIRED_EVENTS if events.get(event_id, {}).get("count", 0) < 1)
    future_nonzero = sorted(event_id for event_id in FUTURE_IDLE_EVENTS if events.get(event_id, {}).get("count", 0) != 0)
    metrics = []
    proxy_ok = True
    for event_id in sorted(REQUIRED_EVENTS | FUTURE_IDLE_EVENTS):
        event = events.get(event_id, {"count": 0, "first_frame": 0, "last_frame": 0})
        first_frame = int(event.get("first_frame", 0) or 0)
        seconds = round(first_frame / 60.0, 3) if first_frame > 0 else None
        target = TARGET_SECONDS.get(event_id)
        target_pass = None
        if target is not None:
            target_pass = seconds is not None and seconds <= target
            proxy_ok = proxy_ok and target_pass
        metrics.append({
            "id": event_id,
            "count": int(event.get("count", 0) or 0),
            "first_frame": first_frame,
            "last_frame": int(event.get("last_frame", 0) or 0),
            "proxy_seconds_at_60fps": seconds,
            "target_seconds": target,
            "target_pass": target_pass,
        })

    passed = not missing and not future_nonzero and proxy_ok
    report = {
        "schema": "rune_marches.playtest_probe_report",
        "version": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "mode": "native_automation_proxy",
        "window_size": window_size,
        "screenshot": screenshot,
        "source_packet": "gamedesign/projects/rune-marches/playtest/first_session_poki_packet.md",
        "note": "Proxy timings prove automation coverage only; human audience timings require real testers.",
        "passed": passed,
        "missing_required_events": missing,
        "future_events_unexpectedly_recorded": future_nonzero,
        "metrics": metrics,
    }
    return report, passed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=9123)
    parser.add_argument("--window-size", default="960x540")
    parser.add_argument("--output", default="tmp/rune_marches/playtest_probe_report.json")
    parser.add_argument("--screenshot", default="")
    args = parser.parse_args()

    screenshot_path = args.screenshot or None
    with running_game(port=args.port, fresh_state=True, window_size=args.window_size) as game:
        telemetry = run_first_session(game)
        if screenshot_path:
            game.capture_screenshot(screenshot_path, wait_frames=2, audit=True)

    report, passed = build_report(telemetry, args.window_size, screenshot_path)
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)
        f.write("\n")
    print(args.output)
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
