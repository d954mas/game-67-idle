#!/usr/bin/env python3
"""Capture Ember Road visual proof states through the native DevAPI."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "devapi"))

from devapi_client import DevApiError, running_game  # noqa: E402
from state_capture import StateCapture  # noqa: E402

MATRIX_OUT = "gamedesign/projects/ember-road/visual/live_state_capture_matrix.json"
REPORT_OUT = "gamedesign/projects/ember-road/reviews/live_state_capture_report.json"
OUT_DIR = "build/captures/ember-road"

REQUIRED_STATES = (
    "first_screen",
    "hud_visible",
    "primary_action_ready",
    "primary_action_feedback",
    "reward_active",
    "locked_or_disabled_state",
    "transient_stress_state",
    "progression_panel_open",
    "modal_or_choice_open",
    "old_mine_scout_result",
    "old_mine_depth_encounter",
    "old_mine_next_delve_choice",
    "old_mine_delve_reward",
    "town_lantern_upgrade",
    "town_lantern_forged",
    "resume_or_reentry_state",
)


def as_dict(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def field(state: dict[str, Any], group: str, name: str, fallback: Any = None) -> Any:
    nested = state.get(group)
    if isinstance(nested, dict) and name in nested:
        return nested.get(name)
    return state.get(f"{group}_{name}", fallback)


def check(problems: list[str], condition: bool, label: str, detail: str = "") -> None:
    if not condition:
        problems.append(f"{label}{': ' + detail if detail else ''}")


def write_report(path: str, report: dict[str, Any]) -> None:
    full = ROOT / path
    full.parent.mkdir(parents=True, exist_ok=True)
    full.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def run(port: int, window_size: str) -> int:
    problems: list[str] = []
    report: dict[str, Any] = {
        "schema": "game.live_state_capture_report",
        "project": "ember-road",
        "window_size": window_size,
        "states": {},
        "checks": [],
    }

    with running_game(port=port, window_size=window_size, fresh_state=True) as game:
        methods = game.endpoint_methods()
        for method in (
            "game.reset_playtest",
            "game.action.accept_quest",
            "game.action.travel_north_road",
            "game.action.auto_battle",
            "game.action.enter_old_mine",
            "game.action.scout_old_mine",
            "game.action.resolve_old_mine_depth",
            "game.action.delve_old_mine",
            "game.action.return_old_gate",
            "game.action.forge_mine_lantern",
            "game.capture.framebuffer",
        ):
            check(problems, method in methods, "endpoint exists", method)

        sc = StateCapture(game, "ember-road", OUT_DIR).require(*REQUIRED_STATES)

        state = as_dict(game.result("game.reset_playtest"))
        check(problems, state.get("location") == "old_gate", "first state location", str(state.get("location")))
        check(problems, state.get("primary_action_id") == "ember.accept_quest", "first primary action", str(state.get("primary_action_id")))
        first = sc.capture("first_screen", audit=True, wait_frames=4)
        hud = sc.capture("hud_visible", audit=True)
        primary = sc.capture("primary_action_ready", audit=True)
        locked = sc.capture("locked_or_disabled_state", audit=True)
        report["states"]["first_screen"] = {"state": state, "evidence": first}
        report["states"]["hud_visible"] = {"evidence": hud}
        report["states"]["primary_action_ready"] = {"evidence": primary}
        report["states"]["locked_or_disabled_state"] = {
            "evidence": locked,
            "expected": "Old Mine plaque visible with LV 2 lock before quest completion.",
        }

        accepted = as_dict(game.result("game.action.accept_quest"))
        check(problems, accepted.get("quest_stage") == "accepted", "quest accepted", str(accepted.get("quest_stage")))
        check(problems, accepted.get("primary_action_id") == "ember.travel_north_road", "travel action after accept", str(accepted.get("primary_action_id")))
        feedback = sc.capture("primary_action_feedback", audit=True, wait_frames=4)
        report["states"]["primary_action_feedback"] = {"state": accepted, "evidence": feedback}

        travelled = as_dict(game.result("game.action.travel_north_road"))
        check(problems, travelled.get("location") == "north_road", "travel location", str(travelled.get("location")))
        check(problems, travelled.get("battle_state") == "ready", "battle ready", str(travelled.get("battle_state")))
        report["states"]["battle_ready"] = {"state": travelled}

        battle = as_dict(game.result("game.action.auto_battle"))
        check(problems, battle.get("battle_state") in {"victory", "low_health"}, "battle result", str(battle.get("battle_state")))
        check(problems, battle.get("quest_stage") == "wolf_defeated", "wolf objective complete", str(battle.get("quest_stage")))
        check(problems, field(battle, "reward", "item_ready") is True, "reward item ready", json.dumps(battle)[:240])
        reward = sc.capture("reward_active", audit=True, wait_frames=4)
        transient = sc.capture("transient_stress_state", audit=True)
        report["states"]["reward_active"] = {"state": battle, "evidence": reward}
        report["states"]["transient_stress_state"] = {
            "state": battle,
            "evidence": transient,
            "expected": "Victory/loot feedback and reward controls visible over the normal HUD.",
        }

        equipped = as_dict(game.result("game.action.equip_ring"))
        check(problems, field(equipped, "gear", "ring_equipped") is True, "ring equipped", json.dumps(equipped)[:240])
        claimed = as_dict(game.result("game.action.claim_reward"))
        check(problems, claimed.get("location") == "old_gate", "claim returns to Old Gate", str(claimed.get("location")))
        check(problems, claimed.get("quest_stage") == "completed", "quest completed", str(claimed.get("quest_stage")))
        check(problems, int(field(claimed, "hero", "level", 0)) >= 2, "level 2 reached", str(field(claimed, "hero", "level")))
        check(problems, int(field(claimed, "hero", "gold", 0)) >= 24, "gold awarded", str(field(claimed, "hero", "gold")))
        progress = sc.capture("progression_panel_open", audit=True, wait_frames=4)
        report["states"]["progression_panel_open"] = {
            "state": claimed,
            "evidence": progress,
            "expected": "Completed quest progression panel shows level, gold, ring status, and Old Mine route consequence.",
        }

        mine = as_dict(game.result("game.action.enter_old_mine"))
        check(problems, mine.get("location") == "old_mine", "Old Mine location reached", str(mine.get("location")))
        check(problems, mine.get("primary_action_id") == "ember.scout_old_mine", "Old Mine scout action", str(mine.get("primary_action_id")))
        check(problems, mine.get("modal_or_choice_open") is True, "choice surface state", json.dumps(mine)[:240])
        modal = sc.capture("modal_or_choice_open", audit=True, wait_frames=4)
        report["states"]["modal_or_choice_open"] = {
            "state": mine,
            "evidence": modal,
            "expected": "Old Mine entry choice surface shows Scout Entrance as active and Back to Old Gate as secondary.",
        }
        scout = as_dict(game.result("game.action.scout_old_mine"))
        check(problems, scout.get("location") == "old_mine", "scout remains at Old Mine", str(scout.get("location")))
        check(problems, scout.get("primary_action_id") == "ember.resolve_old_mine_depth", "Old Mine depth action after scout", str(scout.get("primary_action_id")))
        check(problems, scout.get("old_mine_scout_result_open") is True, "scout result state open", json.dumps(scout)[:240])
        check(problems, field(scout, "old_mine", "scouted") is True, "old mine scouted", json.dumps(scout)[:240])
        check(problems, int(field(scout, "old_mine", "depth", 0)) == 1, "old mine depth 1", str(field(scout, "old_mine", "depth")))
        check(problems, int(field(scout, "old_mine", "ember_shards", 0)) >= 3, "ember shards found", str(field(scout, "old_mine", "ember_shards")))
        result_capture = sc.capture("old_mine_scout_result", audit=True, wait_frames=4)
        report["states"]["old_mine_scout_result"] = {
            "state": scout,
            "evidence": result_capture,
            "expected": "Scout result shows depth 1, Cave Bat signs, ember shards, reward/progress feedback, and return action.",
        }
        depth = as_dict(game.result("game.action.resolve_old_mine_depth"))
        check(problems, depth.get("location") == "old_mine", "depth result remains at Old Mine", str(depth.get("location")))
        check(problems, depth.get("old_mine_depth_encounter_open") is True, "depth encounter result open", json.dumps(depth)[:240])
        check(problems, depth.get("old_mine_next_delve_choice_open") is True, "next delve choice open", json.dumps(depth)[:240])
        check(problems, depth.get("primary_action_id") == "ember.delve_old_mine", "delve after depth result", str(depth.get("primary_action_id")))
        check(problems, field(depth, "old_mine", "depth_resolved") is True, "old mine depth resolved", json.dumps(depth)[:240])
        check(problems, field(depth, "old_mine", "bat_defeated") is True, "cave bat defeated", json.dumps(depth)[:240])
        check(problems, int(field(depth, "old_mine", "bat_damage", 0)) == 3, "bat damage recorded", str(field(depth, "old_mine", "bat_damage")))
        check(problems, int(field(depth, "old_mine", "depth_gold", 0)) == 4, "depth gold recorded", str(field(depth, "old_mine", "depth_gold")))
        check(problems, int(field(depth, "old_mine", "ember_shards", 0)) >= 5, "depth shards added", str(field(depth, "old_mine", "ember_shards")))
        depth_capture = sc.capture("old_mine_depth_encounter", audit=True, wait_frames=4)
        report["states"]["old_mine_depth_encounter"] = {
            "state": depth,
            "evidence": depth_capture,
            "expected": "Depth 1 result shows Cave Bat defeated, ember cache reward, route state, and next delve action.",
        }
        next_delve_capture = sc.capture("old_mine_next_delve_choice", audit=True, wait_frames=4)
        report["states"]["old_mine_next_delve_choice"] = {
            "state": depth,
            "evidence": next_delve_capture,
            "expected": "Post-depth Old Mine screen shows DELVE/CACHE as the active next choice and Back to Old Gate as secondary.",
        }
        delve = as_dict(game.result("game.action.delve_old_mine"))
        check(problems, delve.get("location") == "old_mine", "delve reward remains at Old Mine", str(delve.get("location")))
        check(problems, delve.get("old_mine_delve_reward_open") is True, "delve reward open", json.dumps(delve)[:240])
        check(problems, delve.get("primary_action_id") == "ember.return_old_gate", "return after cache reward", str(delve.get("primary_action_id")))
        check(problems, field(delve, "old_mine", "cache_claimed") is True, "old mine cache claimed", json.dumps(delve)[:240])
        check(problems, int(field(delve, "old_mine", "delve_count", 0)) == 1, "old mine delve count", str(field(delve, "old_mine", "delve_count")))
        check(problems, int(field(delve, "old_mine", "last_delve_shards", 0)) == 1, "last delve shards", str(field(delve, "old_mine", "last_delve_shards")))
        delve_capture = sc.capture("old_mine_delve_reward", audit=True, wait_frames=4)
        report["states"]["old_mine_delve_reward"] = {
            "state": delve,
            "evidence": delve_capture,
            "expected": "Delve reward shows the cache payoff and makes return the next safe action.",
        }
        returned = as_dict(game.result("game.action.return_old_gate"))
        check(problems, returned.get("location") == "old_gate", "return from Old Mine", str(returned.get("location")))
        check(problems, returned.get("town_lantern_upgrade_open") is True, "town lantern upgrade open", json.dumps(returned)[:240])
        check(problems, returned.get("primary_action_id") == "ember.forge_mine_lantern", "forge lantern primary", str(returned.get("primary_action_id")))
        lantern_upgrade = sc.capture("town_lantern_upgrade", audit=True, wait_frames=4)
        report["states"]["town_lantern_upgrade"] = {
            "state": returned,
            "evidence": lantern_upgrade,
            "expected": "Old Gate shows a scene-anchored forge workbench, Mine Lantern, cache shards, and rail summary after returning with the first ember cache.",
        }
        lantern = as_dict(game.result("game.action.forge_mine_lantern"))
        check(problems, lantern.get("location") == "old_gate", "lantern forged in Old Gate", str(lantern.get("location")))
        check(problems, field(lantern, "gear", "mine_lantern") is True, "mine lantern forged", json.dumps(lantern)[:240])
        check(problems, field(lantern, "old_mine", "depth2_unlocked") is True or lantern.get("old_mine_depth2_unlocked") is True, "depth 2 unlocked", json.dumps(lantern)[:240])
        check(problems, int(field(lantern, "old_mine", "ember_shards", 99)) == 0, "lantern shards spent", str(field(lantern, "old_mine", "ember_shards")))
        lantern_forged = sc.capture("town_lantern_forged", audit=True, wait_frames=4)
        report["states"]["town_lantern_forged"] = {
            "state": lantern,
            "evidence": lantern_forged,
            "expected": "Old Gate shows the Mine Lantern equipped in the scene and Depth 2 route unlocked as the next promise.",
        }
        sc.mark_debt("resume_or_reentry_state", "Resume/re-entry behavior is out of scope for this first native slice.")

        matrix_path = sc.write_matrix(MATRIX_OUT)
        report["matrix"] = matrix_path
        report["covered"] = sc.covered
        report["debt"] = sc.debt
        report["launch_log"] = game.launch_log_path

    report["checks"] = [{"status": "fail", "problem": item} for item in problems]
    if not problems:
        report["checks"].append({"status": "pass", "summary": "all required capture states reached or explicitly marked as debt"})
    write_report(REPORT_OUT, report)

    print(f"matrix: {MATRIX_OUT}")
    print(f"report: {REPORT_OUT}")
    for entry in report["covered"]:
        print(f"covered: {entry['tag']} -> {entry['evidence']}")
    for entry in report["debt"]:
        print(f"debt: {entry['tag']} -> {entry['reason']}")
    if problems:
        for problem in problems:
            print(f"FAIL: {problem}", file=sys.stderr)
        return 1
    print("summary: state captures complete")
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=9123)
    parser.add_argument("--window-size", default="1280x720")
    args = parser.parse_args(argv)
    try:
        return run(port=args.port, window_size=args.window_size)
    except DevApiError as exc:
        print(f"devapi error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
