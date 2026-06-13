#!/usr/bin/env python3
"""Run a native child-test readiness review for 67 World."""

from __future__ import annotations

import json
import hashlib
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from base import Scenario, fail_devapi, finish
from devapi_client import DevApiError, running_game


ROOT = Path(__file__).resolve().parents[3]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def file_status(path: Path) -> dict[str, Any]:
    exists = path.exists()
    return {
        "exists": exists,
        "bytes": path.stat().st_size if exists else 0,
        "sha256": sha256(path) if exists else "",
    }


def set_field(game, path: str, value: Any) -> None:
    game.result("game.state.set", {"doc": "game", "path": path, "value": value})


def world(state: dict[str, Any]) -> dict[str, Any]:
    data = state.get("world_67", {})
    return data if isinstance(data, dict) else {}


def fill_stuck_board(game) -> dict[str, Any]:
    game.result("game.reset_playtest")
    set_field(game, "tutorial.done", True)
    set_field(game, "collection_discovered_count", 12)
    set_field(game, "highest_variant_order", 12)
    for path in (
        "count_tiny_67",
        "count_berry_67",
        "count_banana_67",
        "count_smoothie_67",
        "count_cool_67",
        "count_portal_67",
        "count_mystery_67",
        "count_jelly_67",
        "count_lemon_67",
        "count_watermelon_67",
        "count_bubblegum_67",
        "count_sticker_67",
    ):
        set_field(game, path, 1)
    return game.result("game.state")


def package_status() -> dict[str, Any]:
    package_dir = ROOT / "build" / "release" / "67-world-pc" / "67-world"
    package_zip = ROOT / "build" / "release" / "67-world-pc" / "67-world-pc.zip"
    exe = package_dir / "67-world.exe"
    art_pack = package_dir / "assets" / "world67_art.ntpack"
    start_here = package_dir / "START_HERE.bat"
    readme = package_dir / "README.txt"
    parent_observer_guide = package_dir / "PARENT_OBSERVER_GUIDE.md"
    acceptance_kit = package_dir / "CHILD_TEST_ACCEPTANCE.md"
    child_test_launcher = package_dir / "START_CHILD_TEST_FRESH.bat"
    child_test_result_template = package_dir / "CHILD_TEST_RESULT_TEMPLATE.md"
    child_test_report_script = package_dir / "CREATE_CHILD_TEST_REPORT.ps1"
    child_test_report_launcher = package_dir / "CREATE_CHILD_TEST_REPORT.bat"
    self_check_script = package_dir / "VERIFY_PACKAGE.ps1"
    self_check_launcher = package_dir / "VERIFY_PACKAGE.bat"
    runner = package_dir / "RUN_67_WORLD.bat"
    manifest = package_dir / "release_manifest.json"
    checksums = package_dir / "CHECKSUMS.txt"
    exe_status = file_status(exe)
    art_pack_status = file_status(art_pack)
    manifest_status = file_status(manifest)
    checksums_status = file_status(checksums)
    zip_status = file_status(package_zip)
    return {
        "package_dir": str(package_dir),
        "package_zip": str(package_zip),
        "exe_exists": exe_status["exists"],
        "exe_bytes": exe_status["bytes"],
        "exe_sha256": exe_status["sha256"],
        "art_pack_exists": art_pack_status["exists"],
        "art_pack_bytes": art_pack_status["bytes"],
        "art_pack_sha256": art_pack_status["sha256"],
        "start_here_exists": start_here.exists(),
        "start_here_bytes": start_here.stat().st_size if start_here.exists() else 0,
        "readme_exists": readme.exists(),
        "parent_observer_guide_exists": parent_observer_guide.exists(),
        "parent_observer_guide_bytes": parent_observer_guide.stat().st_size if parent_observer_guide.exists() else 0,
        "acceptance_kit_exists": acceptance_kit.exists(),
        "acceptance_kit_bytes": acceptance_kit.stat().st_size if acceptance_kit.exists() else 0,
        "child_test_launcher_exists": child_test_launcher.exists(),
        "child_test_launcher_bytes": child_test_launcher.stat().st_size if child_test_launcher.exists() else 0,
        "child_test_result_template_exists": child_test_result_template.exists(),
        "child_test_result_template_bytes": child_test_result_template.stat().st_size if child_test_result_template.exists() else 0,
        "child_test_report_script_exists": child_test_report_script.exists(),
        "child_test_report_script_bytes": child_test_report_script.stat().st_size if child_test_report_script.exists() else 0,
        "child_test_report_launcher_exists": child_test_report_launcher.exists(),
        "child_test_report_launcher_bytes": child_test_report_launcher.stat().st_size if child_test_report_launcher.exists() else 0,
        "self_check_script_exists": self_check_script.exists(),
        "self_check_script_bytes": self_check_script.stat().st_size if self_check_script.exists() else 0,
        "self_check_launcher_exists": self_check_launcher.exists(),
        "self_check_launcher_bytes": self_check_launcher.stat().st_size if self_check_launcher.exists() else 0,
        "runner_exists": runner.exists(),
        "manifest_exists": manifest_status["exists"],
        "manifest_bytes": manifest_status["bytes"],
        "manifest_sha256": manifest_status["sha256"],
        "checksums_exists": checksums_status["exists"],
        "checksums_bytes": checksums_status["bytes"],
        "checksums_sha256": checksums_status["sha256"],
        "zip_exists": zip_status["exists"],
        "zip_bytes": zip_status["bytes"],
        "zip_sha256": zip_status["sha256"],
    }


def audio_status(game, endpoints: list[str]) -> dict[str, Any]:
    audio_methods = [method for method in endpoints if "audio" in method.lower() or "sound" in method.lower()]
    status = {
        "implemented": bool(audio_methods),
        "devapi_methods": audio_methods,
        "volume_settings_present": True,
        "blocker": None if audio_methods else "No implemented engine audio API or game audio playback endpoint was found.",
    }
    if "game.audio.status" in audio_methods:
        runtime_status = game.result("game.audio.status")
        status.update(
            {
                "implemented": bool(runtime_status.get("implemented")),
                "initialized": bool(runtime_status.get("initialized")),
                "device_enabled": bool(runtime_status.get("device_enabled")),
                "backend": runtime_status.get("backend"),
                "total_play_count": runtime_status.get("total_play_count"),
                "cue_play_counts": runtime_status.get("cue_play_counts", {}),
                "blocker": None if runtime_status.get("implemented") else "Audio endpoint exists but backend is not implemented.",
            }
        )
    return status


def run_desktop_review(port: int, capture_dir: Path, report: dict[str, Any]) -> bool:
    with running_game(port=port, fresh_state=True) as game:
        scenario = Scenario(game)
        endpoints = game.result("endpoints")
        endpoint_list = endpoints if isinstance(endpoints, list) else []
        report["audio_initial"] = audio_status(game, endpoint_list)

        state0 = game.result("game.reset_playtest")
        report["start_state"] = {
            "ftue_step": world(state0).get("ftue_step"),
            "spawn_action_label": world(state0).get("spawn_action_label"),
            "next_goal": world(state0).get("next_goal"),
        }
        scenario.check("desktop starts at spawn FTUE", world(state0).get("ftue_step") == "spawn_first", world(state0))
        scenario.check("desktop starts with TAP BOX CTA", world(state0).get("spawn_action_label") == "TAP BOX", world(state0))

        game.click_ui("world.spawn", wait_frames=2)
        game.click_ui("world.spawn", wait_frames=2)
        two_spawns = game.result("game.state")
        scenario.check("two spawns create merge hint", world(two_spawns).get("merge_hint_slot_a") == 0, world(two_spawns))
        game.click_ui("world.slot.00", wait_frames=2)
        merged = game.click_ui("world.slot.01", wait_frames=20)
        report["first_loop"] = {
            "collection_discovered_count": merged.get("collection_discovered_count"),
            "tutorial_done": merged.get("tutorial", {}).get("done"),
            "ftue_step": world(merged).get("ftue_step"),
            "next_goal": world(merged).get("next_goal"),
        }
        scenario.check("first merge discovers Berry 67", merged.get("collection_discovered_count", 0) >= 2, merged)
        scenario.check("first merge completes tutorial", merged.get("tutorial", {}).get("done") is True, merged.get("tutorial", {}))
        report["screenshots"]["desktop_first_loop"] = scenario.capture(str(capture_dir / "child_test_desktop_first_loop.png"), wait_frames=40)

        set_field(game, "wallet.soft", 25)
        ready = game.result("game.state")
        scenario.check("speed upgrade shows BUY25", world(ready).get("progress_upgrade_value") == "BUY25", world(ready))
        bought = game.click_ui("world.upgrade", wait_frames=20)
        report["upgrade"] = {
            "faster_spawn_bought": bought.get("faster_spawn_bought"),
            "progress_upgrade_title": world(bought).get("progress_upgrade_title"),
            "progress_upgrade_value": world(bought).get("progress_upgrade_value"),
        }
        scenario.check("speed upgrade purchases", bought.get("faster_spawn_bought") is True, bought)
        scenario.check("upgrade slot moves to box progression", world(bought).get("progress_upgrade_title") == "BOX L1", world(bought))
        report["screenshots"]["desktop_upgrade"] = scenario.capture(str(capture_dir / "child_test_desktop_upgrade.png"), wait_frames=40)

        set_field(game, "collection_discovered_count", 3)
        set_field(game, "highest_variant_order", 3)
        set_field(game, "wallet.soft", 1000)
        crate_ready = game.result("game.state")
        scenario.check("better crate can buy after speed", world(crate_ready).get("can_buy_better_crate") is True, world(crate_ready))
        crate_bought = game.result("game.action.buy_better_crate")
        spawned = game.result("game.action.spawn_67")
        report["better_crate"] = {
            "level": crate_bought.get("better_crate_level"),
            "spawned_berry": spawned.get("count_berry_67", 0) >= 1,
        }
        scenario.check("better crate level buys", crate_bought.get("better_crate_level") >= 1, crate_bought)
        scenario.check("better crate spawns higher variant", spawned.get("count_berry_67", 0) >= 1, spawned)

        stuck = fill_stuck_board(game)
        report["screenshots"]["desktop_stuck"] = scenario.capture(str(capture_dir / "child_test_desktop_stuck.png"), wait_frames=40)
        scenario.check("stuck board exposes FREE SLOT", world(stuck).get("spawn_action_label") == "FREE SLOT", world(stuck))
        recycled = game.click_ui("world.spawn", wait_frames=20)
        report["stuck_recovery"] = {
            "before_board_used": world(stuck).get("board_used"),
            "after_board_used": world(recycled).get("board_used"),
            "after_spawn_action_label": world(recycled).get("spawn_action_label"),
            "can_spawn_after_recycle": world(recycled).get("can_spawn"),
        }
        scenario.check("stuck recovery frees a slot", world(recycled).get("board_used") == 11, world(recycled))
        scenario.check("stuck recovery returns spawn CTA", world(recycled).get("spawn_action_label") == "TAP BOX", world(recycled))
        report["audio"] = audio_status(game, endpoint_list)
        cue_counts = report["audio"].get("cue_play_counts", {})
        scenario.check("audio backend is implemented", report["audio"].get("implemented") is True, report["audio"])
        scenario.check("audio played during desktop review", int(report["audio"].get("total_play_count") or 0) >= 6, report["audio"])
        scenario.check("spawn SFX played", int(cue_counts.get("spawn") or 0) >= 3, cue_counts)
        scenario.check("merge SFX played", int(cue_counts.get("merge") or 0) >= 1, cue_counts)
        scenario.check("upgrade SFX played", int(cue_counts.get("upgrade") or 0) >= 2, cue_counts)
        scenario.check("recycle SFX played", int(cue_counts.get("recycle") or 0) >= 1, cue_counts)

        return scenario.ok


def run_portrait_review(port: int, capture_dir: Path, report: dict[str, Any]) -> bool:
    with running_game(port=port, fresh_state=True, window_size="390x844") as game:
        scenario = Scenario(game)
        endpoints = game.result("endpoints")
        endpoint_list = endpoints if isinstance(endpoints, list) else []
        game.result("game.reset_playtest")
        game.result("game.action.spawn_67")
        game.result("game.action.spawn_67")
        game.result("game.action.merge_matching_67")
        game.result("frame.wait", {"frames": 20})
        first_loop = game.result("game.state")
        scenario.check("portrait first loop reaches Berry", first_loop.get("collection_discovered_count", 0) >= 2, first_loop)
        report["screenshots"]["portrait_first_loop"] = scenario.capture(str(capture_dir / "child_test_portrait_first_loop.png"), wait_frames=40)

        stuck = fill_stuck_board(game)
        scenario.check("portrait stuck board exposes FREE SLOT", world(stuck).get("spawn_action_label") == "FREE SLOT", world(stuck))
        report["screenshots"]["portrait_stuck"] = scenario.capture(str(capture_dir / "child_test_portrait_stuck.png"), wait_frames=40)
        portrait_audio = audio_status(game, endpoint_list)
        report["portrait_audio"] = portrait_audio
        scenario.check("portrait audio played", int(portrait_audio.get("total_play_count") or 0) >= 3, portrait_audio)
        return scenario.ok


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    report_path = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("build/reports/child_test_readiness.json")
    capture_dir = Path(sys.argv[3]) if len(sys.argv) > 3 else Path("build/captures/scenarios/child_test_readiness")
    report_path.parent.mkdir(parents=True, exist_ok=True)
    capture_dir.mkdir(parents=True, exist_ok=True)

    report: dict[str, Any] = {
        "checked_at": datetime.now(timezone.utc).isoformat(),
        "goal": "67 World child-test readiness review",
        "screenshots": {},
        "package": package_status(),
        "release_blockers": [],
    }

    try:
        desktop_ok = run_desktop_review(port, capture_dir, report)
        portrait_ok = run_portrait_review(port + 1, capture_dir, report)
        package = report["package"]
        package_ok = bool(
            package["exe_exists"]
            and package["art_pack_exists"]
            and package["start_here_exists"]
            and package["readme_exists"]
            and package["parent_observer_guide_exists"]
            and package["acceptance_kit_exists"]
            and package["child_test_launcher_exists"]
            and package["child_test_result_template_exists"]
            and package["child_test_report_script_exists"]
            and package["child_test_report_launcher_exists"]
            and package["self_check_script_exists"]
            and package["self_check_launcher_exists"]
            and package["runner_exists"]
            and package["manifest_exists"]
            and package["checksums_exists"]
            and package["zip_exists"]
            and package["zip_bytes"] > package["exe_bytes"]
        )
        report["package"]["ok"] = package_ok
        report["automated_review_passed"] = bool(desktop_ok and portrait_ok and package_ok)
        if not report.get("audio", {}).get("implemented"):
            report["release_blockers"].append("Audio playback is not implemented in the current engine/runtime.")
        report["release_blockers"].append("Manual child-test/user acceptance is still required.")
        report["release_ready"] = False
        report["ready_for_manual_child_test"] = report["automated_review_passed"]
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
        print("report:", report_path)
        print("screenshots:", capture_dir)
        return finish(desktop_ok and portrait_ok and package_ok)
    except DevApiError as exc:
        report["automated_review_passed"] = False
        report["release_ready"] = False
        report["release_blockers"].append(str(exc))
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
        return fail_devapi(exc)


if __name__ == "__main__":
    raise SystemExit(main())
