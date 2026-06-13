#!/usr/bin/env python3
"""Drive native 67 World runtime actions through the one-hour progression target."""

from __future__ import annotations

import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from base import Scenario, fail_devapi, finish
from devapi_client import DevApiError, running_game


MIN_TARGET_MINUTES = 55.0
MAX_TARGET_MINUTES = 60.0


def world(state: dict[str, Any]) -> dict[str, Any]:
    data = state.get("world_67", {})
    return data if isinstance(data, dict) else {}


def play_to_cosmic(game, scenario: Scenario) -> dict[str, Any]:
    endpoints = game.result("endpoints")
    scenario.check("runtime exposes bulk passive tick", "game.action.tick_passive" in endpoints, endpoints)
    scenario.check("runtime exposes one-hour progression bot", "game.action.run_one_hour_progression" in endpoints, endpoints)

    state = game.result("game.reset_playtest")
    variants = world(state).get("variants", [])
    scenario.check("runtime exposes 30 variants", isinstance(variants, list) and len(variants) == 30, variants)
    report = game.result("game.action.run_one_hour_progression")
    final_state = report.get("state", {})
    final_world = world(final_state) if isinstance(final_state, dict) else {}
    report.update(
        {
            "checked_at": datetime.now(timezone.utc).isoformat(),
            "target_window_minutes": [MIN_TARGET_MINUTES, MAX_TARGET_MINUTES],
            "final_collection_discovered_count": report.get("collection_discovered_count"),
            "final_next_goal": final_world.get("next_goal"),
            "final_better_crate_level": report.get("better_crate_level"),
            "final_board_used": final_world.get("board_used"),
            "final_wallet_soft": final_state.get("wallet", {}).get("soft") if isinstance(final_state, dict) else None,
            "final_state": final_state,
        }
    )
    return report


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    report_path = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("build/reports/one_hour_progression_runtime.json")
    screenshot_path = sys.argv[3] if len(sys.argv) > 3 else "build/captures/scenarios/one_hour_progression_runtime.png"
    report_path.parent.mkdir(parents=True, exist_ok=True)

    try:
        with running_game(port=port, fresh_state=True) as game:
            scenario = Scenario(game)
            report = play_to_cosmic(game, scenario)
            final_minutes = float(report["final_minutes"])
            scenario.check("runtime reaches Cosmic 67", report["final_collection_discovered_count"] == 30, report)
            scenario.check("runtime reports WORLD COMPLETE", report["final_next_goal"] == "WORLD COMPLETE", report)
            scenario.check("runtime has a Cosmic 67 on board", int(report.get("count_cosmic_67") or 0) >= 1, report)
            scenario.check(
                "runtime reaches cap inside one-hour target window",
                MIN_TARGET_MINUTES <= final_minutes <= MAX_TARGET_MINUTES,
                report,
            )
            scenario.check("runtime bought Better Crate progression", int(report.get("final_better_crate_level") or 0) >= 20, report)
            path = scenario.capture(screenshot_path, wait_frames=130)
            report["screenshot"] = path
            report["passed"] = scenario.ok
            report_path.write_text(json.dumps(report, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
            print("report:", report_path)
            print("screenshot:", path)
            return finish(scenario.ok)
    except DevApiError as exc:
        report_path.write_text(
            json.dumps({"checked_at": datetime.now(timezone.utc).isoformat(), "passed": False, "error": str(exc)}, indent=2)
            + "\n",
            encoding="utf-8",
        )
        return fail_devapi(exc)


if __name__ == "__main__":
    raise SystemExit(main())
