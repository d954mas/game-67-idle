#!/usr/bin/env python3
"""Agent-friendly native playtest harness.

Runs one fast observe -> act -> capture -> report pass through the native DevAPI.
Use this as the default evidence entry point before ad hoc runtime debugging.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from typing import Any

from devapi_client import DevApiError, running_game
from pixel_health import assert_pixel_health


def _safe_result(game: Any, method: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
    response = game.request(method, params or {})
    if response.get("ok") is not True:
        raise DevApiError(f"{method} failed: {response.get('error', response)}")
    result = response.get("result")
    if isinstance(result, dict):
        return result
    return {"value": result}


def _write_report(path: str, report: dict[str, Any]) -> str:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)
        handle.write("\n")
    return os.path.abspath(path)


def _summarize_state(state: dict[str, Any]) -> dict[str, Any]:
    keys = [
        "frame",
        "meme_coins",
        "status",
        "click_power",
        "income_per_second",
        "hands_skill",
        "comfort",
        "visual_stage",
        "first_upgrade_owned",
        "second_upgrade_owned",
        "third_upgrade_owned",
        "fourth_upgrade_owned",
        "fifth_upgrade_owned",
        "first_job_active",
        "first_job_ready",
        "active_job_id",
    ]
    return {key: state.get(key) for key in keys if key in state}


def _audit_agent_screenshot(path: str) -> Any:
    # Agent smoke should catch blank/flat captures without failing current primitive art.
    return assert_pixel_health(
        path,
        min_unique_colors=4,
        min_unique_buckets=4,
        min_luma_range=24.0,
        min_luma_stdev=5.0,
    )


def _wait_for_job_ready(game: Any, max_polls: int, wait_frames: int) -> dict[str, Any]:
    state: dict[str, Any] = {}
    for _ in range(max_polls):
        game.wait_frames(wait_frames)
        state = game.result("game.state")
        if state.get("first_job_ready") is True:
            break
    return state


def run_playtest(port: int, out_dir: str, full_loop: bool) -> tuple[bool, dict[str, Any]]:
    stamp = time.strftime("%Y%m%d_%H%M%S")
    out_dir_abs = os.path.abspath(out_dir)
    screenshots_dir = os.path.join(out_dir_abs, "screenshots")
    os.makedirs(screenshots_dir, exist_ok=True)

    checks: list[dict[str, Any]] = []
    screenshots: list[dict[str, Any]] = []
    states: dict[str, Any] = {}

    def check(name: str, condition: bool, detail: Any = None) -> bool:
        checks.append({"name": name, "ok": bool(condition), "detail": detail})
        print(("PASS" if condition else "FAIL"), name, "::", detail if detail is not None else "")
        return bool(condition)

    ok = True
    with running_game(port=port) as game:
        report: dict[str, Any] = {
            "ok": False,
            "mode": "full_loop" if full_loop else "quick",
            "port": port,
            "launch_log": game.launch_log_path,
            "out_dir": out_dir_abs,
            "screenshots": screenshots,
            "states": states,
            "checks": checks,
        }

        ok &= check("ping", game.request("ping").get("ok") is True)
        endpoints = game.result("endpoints")
        report["endpoints"] = endpoints
        required = {"game.state", "game.reset_playtest", "ui.tree", "ui.click", "frame.wait"}
        ok &= check("required endpoints", required <= set(endpoints), {"missing": sorted(required - set(endpoints))})

        game.result("game.reset_playtest")
        game.wait_frames(2)
        tree = game.result("ui.tree")
        ui_ids = {item.get("id") for item in tree if isinstance(item, dict)}
        required_ui = {"main.do67", "main.upgrade.first", "main.job.first"}
        ok &= check("required gameplay UI", required_ui <= ui_ids, {"missing": sorted(required_ui - ui_ids)})

        initial_state = game.result("game.state")
        states["initial"] = _summarize_state(initial_state)
        ok &= check(
            "fresh state",
            initial_state.get("meme_coins") == 0 and initial_state.get("status") == 1,
            states["initial"],
        )

        initial_path = game.capture_screenshot(os.path.join(screenshots_dir, f"agent_initial_{stamp}.png"), wait_frames=5)
        initial_health = _audit_agent_screenshot(initial_path)
        screenshots.append({"name": "initial", "path": initial_path, "pixel_health": initial_health.summary()})
        ok &= check("initial screenshot health", True, initial_health.summary())

        after_click = game.click_ui("main.do67", wait_frames=2)
        states["after_first_action"] = _summarize_state(after_click)
        ok &= check("first action changes state", after_click.get("meme_coins", 0) >= 1, states["after_first_action"])

        for _ in range(4):
            after_click = game.click_ui("main.do67", wait_frames=2)
        states["after_five_actions"] = _summarize_state(after_click)
        ok &= check("first reward threshold", after_click.get("meme_coins", 0) >= 5, states["after_five_actions"])

        after_upgrade = game.click_ui("main.upgrade.first", wait_frames=2)
        states["after_first_upgrade"] = _summarize_state(after_upgrade)
        ok &= check(
            "first upgrade changes progression",
            after_upgrade.get("status", 0) >= 2 and after_upgrade.get("first_upgrade_owned") is True,
            states["after_first_upgrade"],
        )

        upgrade_path = game.capture_screenshot(os.path.join(screenshots_dir, f"agent_after_upgrade_{stamp}.png"), wait_frames=5)
        upgrade_health = _audit_agent_screenshot(upgrade_path)
        screenshots.append({"name": "after_first_upgrade", "path": upgrade_path, "pixel_health": upgrade_health.summary()})
        ok &= check("upgrade screenshot health", True, upgrade_health.summary())

        if full_loop:
            after_job_start = game.click_ui("main.job.first", wait_frames=4)
            states["after_job_start"] = _summarize_state(after_job_start)
            ok &= check("job starts", after_job_start.get("first_job_active") is True, states["after_job_start"])

            ready_state = _wait_for_job_ready(game, max_polls=140, wait_frames=20)
            states["job_ready"] = _summarize_state(ready_state)
            ok &= check("job becomes ready", ready_state.get("first_job_ready") is True, states["job_ready"])

            if ready_state.get("first_job_ready") is True:
                after_claim = game.click_ui("main.claim", wait_frames=2)
                states["after_claim"] = _summarize_state(after_claim)
                ok &= check(
                    "claim changes progression",
                    after_claim.get("status", 0) >= 3 and after_claim.get("first_job_active") is False,
                    states["after_claim"],
                )
                final_path = game.capture_screenshot(os.path.join(screenshots_dir, f"agent_after_claim_{stamp}.png"), wait_frames=5)
                final_health = _audit_agent_screenshot(final_path)
                screenshots.append({"name": "after_claim", "path": final_path, "pixel_health": final_health.summary()})
                ok &= check("claim screenshot health", True, final_health.summary())

        report["ok"] = bool(ok)
        return bool(ok), report


def main() -> int:
    parser = argparse.ArgumentParser(description="Run an agent-friendly native DevAPI playtest pass.")
    parser.add_argument("port", nargs="?", type=int, default=9123)
    parser.add_argument("--out-dir", default="build/captures/agent_playtest")
    parser.add_argument("--full-loop", action="store_true", help="Also start and claim the first timed job.")
    args = parser.parse_args()

    try:
        ok, report = run_playtest(args.port, args.out_dir, args.full_loop)
        report_path = _write_report(os.path.join(args.out_dir, "agent_playtest_report.json"), report)
        print("report:", report_path)
        print("launch_log:", report.get("launch_log"))
        print("screenshots:")
        for shot in report.get("screenshots", []):
            print("-", shot.get("name"), shot.get("path"), "::", shot.get("pixel_health"))
        print("=== %s ===" % ("AGENT PLAYTEST PASSED" if ok else "AGENT PLAYTEST FAILED"))
        return 0 if ok else 1
    except DevApiError as exc:
        print("FAIL agent_playtest:", exc)
        return 1


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    raise SystemExit(main())
