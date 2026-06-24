#!/usr/bin/env python3
"""Capture Blockside Heat state screenshots through native DevAPI."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "devapi"))

from devapi_client import running_game  # noqa: E402


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=9152)
    parser.add_argument("--out-dir", default="tmp/blockside-heat")
    parser.add_argument("--window-size", default="1280x720")
    parser.add_argument(
        "--exe",
        default=str(ROOT / "build" / "game_seed" / "native-debug" / "game_seed.exe"),
    )
    args = parser.parse_args()

    out_dir = (ROOT / args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    report: dict[str, object] = {
        "schema": "blockside_heat.capture_states",
        "window_size": args.window_size,
        "screenshots": {},
        "states": {},
    }

    with running_game(port=args.port, exe=args.exe, window_size=args.window_size) as game:
        game.wait_frames(120)
        report["screenshots"]["first_screen"] = game.capture_screenshot(
            str(out_dir / "first-native-screenshot-latest.png"),
            wait_frames=2,
            audit=True,
        )
        report["states"]["first_screen"] = game.result("game.state")["state"]

        report["states"]["in_car_movement"] = game.result("game.action.drive_probe")["state"]
        drive_state = report["states"]["in_car_movement"]
        require(drive_state["in_vehicle"] is True, "drive probe did not enter vehicle")
        require(abs(drive_state["car_x"] + 1.0) > 0.75 or abs(drive_state["car_z"] + 0.8) > 0.75, "drive probe did not move the car")
        require(abs(drive_state["car_speed"]) > 0.05, "drive probe did not report vehicle speed")
        game.wait_frames(2)
        report["screenshots"]["in_car_movement"] = game.capture_screenshot(
            str(out_dir / "in-car-movement-latest.png"),
            wait_frames=1,
            audit=True,
        )
        report["states"]["vehicle_route_complete"] = game.result("game.action.drive_package_route")["state"]
        route_state = report["states"]["vehicle_route_complete"]
        require(route_state["package_delivered"] is True, "vehicle route did not deliver package")
        require(route_state["second_job_unlocked"] is True, "vehicle route did not unlock second job")
        require(route_state["next_job"] == "rita_repo_tip", "vehicle route did not expose next job")
        require(route_state["cash"] >= 75, "vehicle route did not award cash")
        report["screenshots"]["vehicle_route_complete"] = game.capture_screenshot(
            str(out_dir / "vehicle-route-complete-latest.png"),
            wait_frames=1,
            audit=True,
        )

        game.result("game.reset_playtest")
        game.wait_frames(4)

        game.result("game.action.pickup_package")
        game.wait_frames(2)
        report["screenshots"]["pursuit_pressure"] = game.capture_screenshot(
            str(out_dir / "pursuit-pressure-latest.png"),
            wait_frames=1,
            audit=True,
        )
        report["states"]["pursuit_pressure"] = game.result("game.state")["state"]
        pressure_state = report["states"]["pursuit_pressure"]
        require(pressure_state["roadblock_active"] is True, "pursuit pressure did not raise a roadblock")
        require(pressure_state["wanted_level"] >= 1, "pursuit pressure did not raise wanted level")

        game.result("game.action.fire")
        game.wait_frames(2)
        report["screenshots"]["pickup_stress"] = game.capture_screenshot(
            str(out_dir / "pickup-stress-latest.png"),
            wait_frames=1,
            audit=True,
        )
        report["states"]["pickup_stress"] = game.result("game.state")["state"]
        response_state = report["states"]["pickup_stress"]
        require(response_state["roadblock_active"] is False, "toy blaster did not clear roadblock")
        require(response_state["roadblock_cleared"] is True, "toy blaster response was not recorded")

        game.result("game.action.complete_job")
        game.wait_frames(4)
        report["screenshots"]["reward_active"] = game.capture_screenshot(
            str(out_dir / "job-complete-latest.png"),
            wait_frames=1,
            audit=True,
        )
        report["states"]["reward_active"] = game.result("game.state")["state"]
        reward_state = report["states"]["reward_active"]
        require(reward_state["job_stage"] == "second_ready", "job completion did not advance story stage")
        require(reward_state["second_job_unlocked"] is True, "job completion did not unlock second job")
        require(reward_state["next_job"] == "rita_repo_tip", "job completion did not expose Rita repo tip")
        report["ui_tree_second_ready"] = game.result("ui.tree")
        ui_nodes = report["ui_tree_second_ready"].get("nodes", [])
        require(any(node.get("text") == "JOB: RITA REPO" for node in ui_nodes), "ui tree did not expose second job label")

        report["states"]["rita_contact"] = game.result("game.action.talk_rita")["state"]
        game.wait_frames(4)
        report["screenshots"]["rita_contact"] = game.capture_screenshot(
            str(out_dir / "rita-contact-latest.png"),
            wait_frames=1,
            audit=True,
        )
        rita_state = report["states"]["rita_contact"]
        require(rita_state["job_stage"] == "repo_intro", "Rita contact did not start repo intro")
        require(rita_state["repo_intro_active"] is True, "Rita contact did not set repo intro active")
        require(rita_state["next_job"] == "repo_red_compact", "Rita contact did not expose repo-car next action")
        report["ui_tree_rita_contact"] = game.result("ui.tree")
        repo_ui_nodes = report["ui_tree_rita_contact"].get("nodes", [])
        require(any(node.get("text") == "JOB: RED COMPACT" for node in repo_ui_nodes), "ui tree did not expose repo intro label")

        report["states"]["repo_drive"] = game.result("game.action.enter_repo_car")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_drive"] = game.capture_screenshot(
            str(out_dir / "repo-drive-latest.png"),
            wait_frames=1,
            audit=True,
        )
        repo_drive_state = report["states"]["repo_drive"]
        require(repo_drive_state["job_stage"] == "repo_drive", "repo car entry did not start repo drive")
        require(repo_drive_state["in_vehicle"] is True, "repo car entry did not put player in vehicle")
        require(repo_drive_state["repo_drive_active"] is True, "repo drive was not marked active")
        require(repo_drive_state["next_job"] == "repo_scout_curb", "repo drive did not expose scout target")
        report["ui_tree_repo_drive"] = game.result("ui.tree")
        drive_ui_nodes = report["ui_tree_repo_drive"].get("nodes", [])
        require(any(node.get("text") == "JOB: CURB SCOUT" for node in drive_ui_nodes), "ui tree did not expose repo driving label")

        report["states"]["repo_scout_complete"] = game.result("game.action.complete_repo_scout")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_scout_complete"] = game.capture_screenshot(
            str(out_dir / "repo-scout-complete-latest.png"),
            wait_frames=1,
            audit=True,
        )
        scout_state = report["states"]["repo_scout_complete"]
        require(scout_state["job_stage"] == "repo_scout_complete", "curb arrival did not complete repo scout")
        require(scout_state["repo_scout_complete"] is True, "curb arrival did not mark scout complete")
        require(scout_state["cash"] >= 100, "curb arrival did not add scout cash")
        require(scout_state["next_job"] == "repo_stash_van", "curb arrival did not expose next repo hook")
        report["ui_tree_scout_complete"] = game.result("ui.tree")
        scout_ui_nodes = report["ui_tree_scout_complete"].get("nodes", [])
        require(any(node.get("text") == "JOB: STASH SPOTTED" for node in scout_ui_nodes), "ui tree did not expose scout completion label")

        report["states"]["stash_lead"] = game.result("game.action.open_stash_lead")["state"]
        game.wait_frames(4)
        report["screenshots"]["stash_lead"] = game.capture_screenshot(
            str(out_dir / "stash-lead-latest.png"),
            wait_frames=1,
            audit=True,
        )
        stash_state = report["states"]["stash_lead"]
        require(stash_state["job_stage"] == "stash_lead", "stash interaction did not open stash lead")
        require(stash_state["stash_lead_active"] is True, "stash interaction did not mark stash lead active")
        require(stash_state["next_job"] == "repo_van_rumor", "stash interaction did not expose van rumor")
        report["ui_tree_stash_lead"] = game.result("ui.tree")
        stash_ui_nodes = report["ui_tree_stash_lead"].get("nodes", [])
        require(any(node.get("text") == "JOB: VAN RUMOR" for node in stash_ui_nodes), "ui tree did not expose van rumor label")

        report["states"]["van_rumor"] = game.result("game.action.follow_van_rumor")["state"]
        game.wait_frames(4)
        report["screenshots"]["van_rumor"] = game.capture_screenshot(
            str(out_dir / "van-rumor-latest.png"),
            wait_frames=1,
            audit=True,
        )
        rumor_state = report["states"]["van_rumor"]
        require(rumor_state["job_stage"] == "van_rumor", "east marker did not confirm van rumor")
        require(rumor_state["van_rumor_active"] is True, "east marker did not mark van rumor active")
        require(rumor_state["next_job"] == "repo_market_watch", "van rumor did not expose market watch")
        report["ui_tree_van_rumor"] = game.result("ui.tree")
        rumor_ui_nodes = report["ui_tree_van_rumor"].get("nodes", [])
        require(any(node.get("text") == "JOB: MARKET WATCH" for node in rumor_ui_nodes), "ui tree did not expose market watch label")

        report["states"]["market_watch"] = game.result("game.action.start_market_watch")["state"]
        game.wait_frames(4)
        report["screenshots"]["market_watch"] = game.capture_screenshot(
            str(out_dir / "market-watch-latest.png"),
            wait_frames=1,
            audit=True,
        )
        market_state = report["states"]["market_watch"]
        require(market_state["job_stage"] == "market_watch", "market point did not start stakeout")
        require(market_state["market_watch_active"] is True, "market point did not mark stakeout active")
        require(market_state["next_job"] == "repo_courier_watch", "market watch did not expose courier watch")
        report["ui_tree_market_watch"] = game.result("ui.tree")
        market_ui_nodes = report["ui_tree_market_watch"].get("nodes", [])
        require(any(node.get("text") == "JOB: COURIER WATCH" for node in market_ui_nodes), "ui tree did not expose courier watch label")

        report["states"]["courier_spotted"] = game.result("game.action.spot_courier")["state"]
        game.wait_frames(4)
        report["screenshots"]["courier_spotted"] = game.capture_screenshot(
            str(out_dir / "courier-spotted-latest.png"),
            wait_frames=1,
            audit=True,
        )
        courier_state = report["states"]["courier_spotted"]
        require(courier_state["job_stage"] == "courier_spotted", "courier cue did not advance story")
        require(courier_state["courier_spotted"] is True, "courier cue did not mark courier spotted")
        require(courier_state["next_job"] == "repo_tail_route", "courier spotting did not expose tail route")
        report["ui_tree_courier_spotted"] = game.result("ui.tree")
        courier_ui_nodes = report["ui_tree_courier_spotted"].get("nodes", [])
        require(any(node.get("text") == "JOB: TAIL ROUTE" for node in courier_ui_nodes), "ui tree did not expose tail route label")

        report["states"]["tail_route"] = game.result("game.action.start_tail_route")["state"]
        game.wait_frames(4)
        report["screenshots"]["tail_route"] = game.capture_screenshot(
            str(out_dir / "tail-route-latest.png"),
            wait_frames=1,
            audit=True,
        )
        tail_state = report["states"]["tail_route"]
        require(tail_state["job_stage"] == "tail_route", "tail marker did not start route")
        require(tail_state["tail_route_active"] is True, "tail marker did not mark route active")
        require(tail_state["next_job"] == "repo_tail_turns", "tail route did not expose turn-watch follow-up")
        report["ui_tree"] = game.result("ui.tree")
        tail_ui_nodes = report["ui_tree"].get("nodes", [])
        require(any(node.get("text") == "JOB: SAFE DISTANCE" for node in tail_ui_nodes), "ui tree did not expose safe-distance label")

        report["states"]["tail_turn"] = game.result("game.action.watch_tail_turn")["state"]
        game.wait_frames(4)
        report["screenshots"]["tail_turn"] = game.capture_screenshot(
            str(out_dir / "tail-turn-latest.png"),
            wait_frames=1,
            audit=True,
        )
        turn_state = report["states"]["tail_turn"]
        require(turn_state["job_stage"] == "tail_turn", "turn marker did not advance tail state")
        require(turn_state["tail_turn_watch"] is True, "turn marker did not mark turn watched")
        require(turn_state["next_job"] == "repo_tail_intercept", "turn watch did not expose intercept follow-up")
        report["ui_tree_tail_turn"] = game.result("ui.tree")
        turn_ui_nodes = report["ui_tree_tail_turn"].get("nodes", [])
        require(any(node.get("text") == "JOB: TURN WATCH" for node in turn_ui_nodes), "ui tree did not expose turn-watch label")

        report["states"]["tail_pressure"] = game.result("game.action.tail_pressure")["state"]
        game.wait_frames(4)
        report["screenshots"]["tail_pressure"] = game.capture_screenshot(
            str(out_dir / "tail-pressure-latest.png"),
            wait_frames=1,
            audit=True,
        )
        pressure_state = report["states"]["tail_pressure"]
        require(pressure_state["job_stage"] == "tail_pressure", "pressure marker did not advance tail state")
        require(pressure_state["tail_pressure_active"] is True, "pressure marker did not mark pressure active")
        require(pressure_state["wanted_level"] == 2, "pressure marker did not raise wanted pressure")
        require(pressure_state["next_job"] == "repo_tail_stop", "tail pressure did not expose stop follow-up")
        report["ui_tree_tail_pressure"] = game.result("ui.tree")
        pressure_ui_nodes = report["ui_tree_tail_pressure"].get("nodes", [])
        require(any(node.get("text") == "JOB: TAIL PRESSURE" for node in pressure_ui_nodes), "ui tree did not expose tail-pressure label")

        report["states"]["tail_stop"] = game.result("game.action.tail_stop")["state"]
        game.wait_frames(4)
        report["screenshots"]["tail_stop"] = game.capture_screenshot(
            str(out_dir / "tail-stop-latest.png"),
            wait_frames=1,
            audit=True,
        )
        stop_state = report["states"]["tail_stop"]
        require(stop_state["job_stage"] == "tail_stop", "stop marker did not resolve tail state")
        require(stop_state["tail_stop_resolved"] is True, "stop marker did not mark tail resolved")
        require(stop_state["cash"] == 175, "stop marker did not award courier payoff")
        require(stop_state["next_job"] == "repo_target_handoff", "tail stop did not expose handoff follow-up")
        report["ui_tree_tail_stop"] = game.result("ui.tree")
        stop_ui_nodes = report["ui_tree_tail_stop"].get("nodes", [])
        require(any(node.get("text") == "JOB: COURIER STOPPED" for node in stop_ui_nodes), "ui tree did not expose courier-stopped label")

        report["states"]["target_handoff"] = game.result("game.action.target_handoff")["state"]
        game.wait_frames(4)
        report["screenshots"]["target_handoff"] = game.capture_screenshot(
            str(out_dir / "target-handoff-latest.png"),
            wait_frames=1,
            audit=True,
        )
        handoff_state = report["states"]["target_handoff"]
        require(handoff_state["job_stage"] == "target_handoff", "handoff marker did not advance story")
        require(handoff_state["target_handoff_active"] is True, "handoff marker did not mark target handoff")
        require(handoff_state["cash"] == 185, "handoff marker did not award handoff payoff")
        require(handoff_state["next_job"] == "repo_green_coupe", "handoff did not expose green coupe objective")
        report["ui_tree_target_handoff"] = game.result("ui.tree")
        handoff_ui_nodes = report["ui_tree_target_handoff"].get("nodes", [])
        require(any(node.get("text") == "JOB: TARGET HANDOFF" for node in handoff_ui_nodes), "ui tree did not expose target-handoff label")

        report["states"]["green_coupe_approach"] = game.result("game.action.green_coupe_approach")["state"]
        game.wait_frames(4)
        report["screenshots"]["green_coupe_approach"] = game.capture_screenshot(
            str(out_dir / "green-coupe-approach-latest.png"),
            wait_frames=1,
            audit=True,
        )
        coupe_state = report["states"]["green_coupe_approach"]
        require(coupe_state["job_stage"] == "green_coupe_approach", "green coupe marker did not advance story")
        require(coupe_state["green_coupe_approach"] is True, "green coupe marker did not mark approach state")
        require(coupe_state["next_job"] == "repo_green_coupe_entry", "green coupe approach did not expose entry follow-up")
        report["ui_tree_green_coupe"] = game.result("ui.tree")
        coupe_ui_nodes = report["ui_tree_green_coupe"].get("nodes", [])
        require(any(node.get("text") == "JOB: GREEN COUPE" for node in coupe_ui_nodes), "ui tree did not expose green-coupe label")

        report["states"]["green_coupe_entry"] = game.result("game.action.green_coupe_entry")["state"]
        game.wait_frames(4)
        report["screenshots"]["green_coupe_entry"] = game.capture_screenshot(
            str(out_dir / "green-coupe-entry-latest.png"),
            wait_frames=1,
            audit=True,
        )
        entry_state = report["states"]["green_coupe_entry"]
        require(entry_state["job_stage"] == "green_coupe_claimed", "green coupe entry did not advance story")
        require(entry_state["green_coupe_claimed"] is True, "green coupe entry did not mark target claimed")
        require(entry_state["in_vehicle"] is True, "green coupe entry did not put player in target car")
        require(entry_state["cash"] == 220, "green coupe entry did not award target claim payoff")
        require(entry_state["next_job"] == "repo_escape_start", "green coupe entry did not expose escape follow-up")
        report["ui_tree_green_coupe_entry"] = game.result("ui.tree")
        entry_ui_nodes = report["ui_tree_green_coupe_entry"].get("nodes", [])
        require(any(node.get("text") == "JOB: COUPE CLAIMED" for node in entry_ui_nodes), "ui tree did not expose green-coupe claimed label")

        report["states"]["green_coupe_escape"] = game.result("game.action.green_coupe_escape")["state"]
        game.wait_frames(4)
        report["screenshots"]["green_coupe_escape"] = game.capture_screenshot(
            str(out_dir / "green-coupe-escape-latest.png"),
            wait_frames=1,
            audit=True,
        )
        escape_state = report["states"]["green_coupe_escape"]
        require(escape_state["job_stage"] == "green_coupe_escape", "escape marker did not advance story")
        require(escape_state["green_coupe_escaped"] is True, "escape marker did not mark heat lost")
        require(escape_state["wanted_level"] == 0, "escape marker did not clear wanted pressure")
        require(escape_state["cash"] == 250, "escape marker did not award escape payoff")
        require(escape_state["next_job"] == "repo_dropoff_call", "escape marker did not expose drop-off follow-up")
        report["ui_tree_green_coupe_escape"] = game.result("ui.tree")
        escape_ui_nodes = report["ui_tree_green_coupe_escape"].get("nodes", [])
        require(any(node.get("text") == "JOB: HEAT LOST" for node in escape_ui_nodes), "ui tree did not expose heat-lost label")

        report["states"]["repo_dropoff_call"] = game.result("game.action.repo_dropoff_call")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_dropoff_call"] = game.capture_screenshot(
            str(out_dir / "repo-dropoff-call-latest.png"),
            wait_frames=1,
            audit=True,
        )
        call_state = report["states"]["repo_dropoff_call"]
        require(call_state["job_stage"] == "repo_dropoff_call", "drop-off call did not advance story")
        require(call_state["repo_dropoff_call"] is True, "drop-off call did not mark call state")
        require(call_state["next_job"] == "repo_dropoff_garage", "drop-off call did not expose garage follow-up")
        report["ui_tree_repo_dropoff_call"] = game.result("ui.tree")
        call_ui_nodes = report["ui_tree_repo_dropoff_call"].get("nodes", [])
        require(any(node.get("text") == "JOB: DROPOFF CALL" for node in call_ui_nodes), "ui tree did not expose drop-off call label")

        report["states"]["repo_dropoff_garage"] = game.result("game.action.repo_dropoff_garage")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_dropoff_garage"] = game.capture_screenshot(
            str(out_dir / "repo-dropoff-garage-latest.png"),
            wait_frames=1,
            audit=True,
        )
        garage_state = report["states"]["repo_dropoff_garage"]
        require(garage_state["job_stage"] == "repo_dropoff_garage", "garage drop-off did not advance story")
        require(garage_state["repo_dropoff_garage"] is True, "garage drop-off did not mark garage state")
        require(garage_state["cash"] == 340, "garage drop-off did not award payoff")
        require(garage_state["next_job"] == "repo_payout_meet", "garage drop-off did not expose payout follow-up")
        report["ui_tree_repo_dropoff_garage"] = game.result("ui.tree")
        garage_ui_nodes = report["ui_tree_repo_dropoff_garage"].get("nodes", [])
        require(any(node.get("text") == "JOB: GARAGE DONE" for node in garage_ui_nodes), "ui tree did not expose garage-done label")

        report["states"]["repo_payout_meet"] = game.result("game.action.repo_payout_meet")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_payout_meet"] = game.capture_screenshot(
            str(out_dir / "repo-payout-meet-latest.png"),
            wait_frames=1,
            audit=True,
        )
        payout_state = report["states"]["repo_payout_meet"]
        require(payout_state["job_stage"] == "repo_payout_meet", "payout meet did not advance story")
        require(payout_state["repo_payout_meet"] is True, "payout meet did not mark payout state")
        require(payout_state["cash"] == 400, "payout meet did not award payout")
        require(payout_state["next_job"] == "repo_next_lead", "payout meet did not expose next lead")
        report["ui_tree_repo_payout_meet"] = game.result("ui.tree")
        payout_ui_nodes = report["ui_tree_repo_payout_meet"].get("nodes", [])
        require(any(node.get("text") == "JOB: PAYOUT DONE" for node in payout_ui_nodes), "ui tree did not expose payout-done label")

        report["states"]["repo_next_lead"] = game.result("game.action.repo_next_lead")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_next_lead"] = game.capture_screenshot(
            str(out_dir / "repo-next-lead-latest.png"),
            wait_frames=1,
            audit=True,
        )
        next_lead_state = report["states"]["repo_next_lead"]
        require(next_lead_state["job_stage"] == "repo_next_lead", "post-payout lead did not advance story")
        require(next_lead_state["repo_next_lead"] is True, "post-payout lead did not mark lead state")
        require(next_lead_state["wanted_level"] == 1, "post-payout lead did not raise heat")
        require(next_lead_state["next_job"] == "repo_heat_watch", "post-payout lead did not expose next hook")
        report["ui_tree_repo_next_lead"] = game.result("ui.tree")
        next_lead_ui_nodes = report["ui_tree_repo_next_lead"].get("nodes", [])
        require(any(node.get("text") == "JOB: NEW LEAD" for node in next_lead_ui_nodes), "ui tree did not expose new-lead label")

        report["states"]["repo_heat_watch"] = game.result("game.action.repo_heat_watch")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_heat_watch"] = game.capture_screenshot(
            str(out_dir / "repo-heat-watch-latest.png"),
            wait_frames=1,
            audit=True,
        )
        heat_watch_state = report["states"]["repo_heat_watch"]
        require(heat_watch_state["job_stage"] == "repo_heat_watch", "heat watch did not advance story")
        require(heat_watch_state["repo_heat_watch"] is True, "heat watch did not mark watch state")
        require(heat_watch_state["wanted_level"] == 2, "heat watch did not raise wanted pressure")
        require(heat_watch_state["next_job"] == "repo_meet_intercept", "heat watch did not expose intercept hook")
        report["ui_tree_repo_heat_watch"] = game.result("ui.tree")
        heat_watch_ui_nodes = report["ui_tree_repo_heat_watch"].get("nodes", [])
        require(any(node.get("text") == "JOB: HEAT WATCH" for node in heat_watch_ui_nodes), "ui tree did not expose heat-watch label")

        report["states"]["repo_meet_intercept"] = game.result("game.action.repo_meet_intercept")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_meet_intercept"] = game.capture_screenshot(
            str(out_dir / "repo-meet-intercept-latest.png"),
            wait_frames=1,
            audit=True,
        )
        intercept_state = report["states"]["repo_meet_intercept"]
        require(intercept_state["job_stage"] == "repo_meet_intercept", "meet intercept did not advance story")
        require(intercept_state["repo_meet_intercept"] is True, "meet intercept did not mark intercept state")
        require(intercept_state["cash"] == 430, "meet intercept did not award intercept payoff")
        require(intercept_state["wanted_level"] == 2, "meet intercept did not preserve wanted pressure")
        require(intercept_state["next_job"] == "repo_getaway_route", "meet intercept did not expose getaway hook")
        report["ui_tree_repo_meet_intercept"] = game.result("ui.tree")
        intercept_ui_nodes = report["ui_tree_repo_meet_intercept"].get("nodes", [])
        require(any(node.get("text") == "JOB: MEET HIT" for node in intercept_ui_nodes), "ui tree did not expose meet-intercept label")

        report["states"]["repo_getaway_route"] = game.result("game.action.repo_getaway_route")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_getaway_route"] = game.capture_screenshot(
            str(out_dir / "repo-getaway-route-latest.png"),
            wait_frames=1,
            audit=True,
        )
        getaway_state = report["states"]["repo_getaway_route"]
        require(getaway_state["job_stage"] == "repo_getaway_route", "getaway route did not advance story")
        require(getaway_state["repo_getaway_route"] is True, "getaway route did not mark route state")
        require(getaway_state["cash"] == 450, "getaway route did not award route payoff")
        require(getaway_state["wanted_level"] == 1, "getaway route did not reduce wanted pressure")
        require(getaway_state["next_job"] == "repo_safehouse_drop", "getaway route did not expose safehouse hook")
        report["ui_tree_repo_getaway_route"] = game.result("ui.tree")
        getaway_ui_nodes = report["ui_tree_repo_getaway_route"].get("nodes", [])
        require(any(node.get("text") == "JOB: GETAWAY" for node in getaway_ui_nodes), "ui tree did not expose getaway-route label")

        report["states"]["repo_safehouse_drop"] = game.result("game.action.repo_safehouse_drop")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_safehouse_drop"] = game.capture_screenshot(
            str(out_dir / "repo-safehouse-drop-latest.png"),
            wait_frames=1,
            audit=True,
        )
        safehouse_state = report["states"]["repo_safehouse_drop"]
        require(safehouse_state["job_stage"] == "repo_safehouse_drop", "safehouse drop did not advance story")
        require(safehouse_state["repo_safehouse_drop"] is True, "safehouse drop did not mark drop state")
        require(safehouse_state["cash"] == 485, "safehouse drop did not award safehouse payoff")
        require(safehouse_state["wanted_level"] == 0, "safehouse drop did not clear wanted pressure")
        require(safehouse_state["next_job"] == "repo_final_call", "safehouse drop did not expose final call hook")
        report["ui_tree_repo_safehouse_drop"] = game.result("ui.tree")
        safehouse_ui_nodes = report["ui_tree_repo_safehouse_drop"].get("nodes", [])
        require(any(node.get("text") == "JOB: SAFEHOUSE" for node in safehouse_ui_nodes), "ui tree did not expose safehouse label")

        report["states"]["repo_final_call"] = game.result("game.action.repo_final_call")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_final_call"] = game.capture_screenshot(
            str(out_dir / "repo-final-call-latest.png"),
            wait_frames=1,
            audit=True,
        )
        final_call_state = report["states"]["repo_final_call"]
        require(final_call_state["job_stage"] == "repo_final_call", "final call did not advance story")
        require(final_call_state["repo_final_call"] is True, "final call did not mark call state")
        require(final_call_state["cash"] == 500, "final call did not award call payoff")
        require(final_call_state["wanted_level"] == 0, "final call did not keep wanted clear")
        require(final_call_state["next_job"] == "repo_next_score_lead", "final call did not expose next-score hook")
        report["ui_tree_repo_final_call"] = game.result("ui.tree")
        final_call_ui_nodes = report["ui_tree_repo_final_call"].get("nodes", [])
        require(any(node.get("text") == "JOB: RITA CALL" for node in final_call_ui_nodes), "ui tree did not expose final-call label")

        report["states"]["repo_next_score_lead"] = game.result("game.action.repo_next_score_lead")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_next_score_lead"] = game.capture_screenshot(
            str(out_dir / "repo-next-score-lead-latest.png"),
            wait_frames=1,
            audit=True,
        )
        next_score_state = report["states"]["repo_next_score_lead"]
        require(next_score_state["job_stage"] == "repo_next_score_lead", "next-score lead did not advance story")
        require(next_score_state["repo_next_score_lead"] is True, "next-score lead did not mark lead state")
        require(next_score_state["cash"] == 510, "next-score lead did not award lead payoff")
        require(next_score_state["wanted_level"] == 0, "next-score lead did not keep wanted clear")
        require(next_score_state["next_job"] == "repo_crew_pickup", "next-score lead did not expose crew-pickup hook")
        report["ui_tree_repo_next_score_lead"] = game.result("ui.tree")
        next_score_ui_nodes = report["ui_tree_repo_next_score_lead"].get("nodes", [])
        require(any(node.get("text") == "JOB: BIG SCORE" for node in next_score_ui_nodes), "ui tree did not expose next-score label")

        report["states"]["repo_crew_pickup"] = game.result("game.action.repo_crew_pickup")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_crew_pickup"] = game.capture_screenshot(
            str(out_dir / "repo-crew-pickup-latest.png"),
            wait_frames=1,
            audit=True,
        )
        crew_state = report["states"]["repo_crew_pickup"]
        require(crew_state["job_stage"] == "repo_crew_pickup", "crew pickup did not advance story")
        require(crew_state["repo_crew_pickup"] is True, "crew pickup did not mark crew state")
        require(crew_state["cash"] == 520, "crew pickup did not award crew payoff")
        require(crew_state["wanted_level"] == 0, "crew pickup did not keep wanted clear")
        require(crew_state["next_job"] == "repo_tool_cache", "crew pickup did not expose tool-cache hook")
        report["ui_tree_repo_crew_pickup"] = game.result("ui.tree")
        crew_ui_nodes = report["ui_tree_repo_crew_pickup"].get("nodes", [])
        require(any(node.get("text") == "JOB: CREW PICKUP" for node in crew_ui_nodes), "ui tree did not expose crew-pickup label")

        report["states"]["repo_tool_cache"] = game.result("game.action.repo_tool_cache")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_tool_cache"] = game.capture_screenshot(
            str(out_dir / "repo-tool-cache-latest.png"),
            wait_frames=1,
            audit=True,
        )
        cache_state = report["states"]["repo_tool_cache"]
        require(cache_state["job_stage"] == "repo_tool_cache", "tool cache did not advance story")
        require(cache_state["repo_tool_cache"] is True, "tool cache did not mark cache state")
        require(cache_state["cash"] == 530, "tool cache did not award cache payoff")
        require(cache_state["wanted_level"] == 0, "tool cache did not keep wanted clear")
        require(cache_state["next_job"] == "repo_score_staging", "tool cache did not expose score-staging hook")
        report["ui_tree_repo_tool_cache"] = game.result("ui.tree")
        cache_ui_nodes = report["ui_tree_repo_tool_cache"].get("nodes", [])
        require(any(node.get("text") == "JOB: TOOL CACHE" for node in cache_ui_nodes), "ui tree did not expose tool-cache label")

        report["states"]["repo_score_staging"] = game.result("game.action.repo_score_staging")["state"]
        game.wait_frames(4)
        report["screenshots"]["repo_score_staging"] = game.capture_screenshot(
            str(out_dir / "repo-score-staging-latest.png"),
            wait_frames=1,
            audit=True,
        )
        staging_state = report["states"]["repo_score_staging"]
        require(staging_state["job_stage"] == "repo_score_staging", "score staging did not advance story")
        require(staging_state["repo_score_staging"] is True, "score staging did not mark staging state")
        require(staging_state["cash"] == 550, "score staging did not award staging payoff")
        require(staging_state["wanted_level"] == 0, "score staging did not keep wanted clear")
        require(staging_state["next_job"] == "repo_score_target", "score staging did not expose target hook")
        report["ui_tree_repo_score_staging"] = game.result("ui.tree")
        staging_ui_nodes = report["ui_tree_repo_score_staging"].get("nodes", [])
        require(any(node.get("text") == "JOB: SCORE STAGED" for node in staging_ui_nodes), "ui tree did not expose score-staging label")

    report_path = out_dir / "capture-states-report.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(str(report_path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
