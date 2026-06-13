#!/usr/bin/env python3
"""Validate autosave persistence and fresh child-test isolation in native runtime."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from base import Scenario, fail_devapi, finish
from devapi_client import DevApiError, NATIVE_DEBUG_EXE, ROOT, running_game


def world(state: dict[str, Any]) -> dict[str, Any]:
    data = state.get("world_67", {})
    return data if isinstance(data, dict) else {}


def require_under_root(path: Path, root: Path) -> None:
    resolved = path.resolve()
    root_resolved = root.resolve()
    if not str(resolved).startswith(str(root_resolved) + os.sep):
        raise DevApiError(f"refusing to use path outside root: {resolved}")


def prepare_runtime_cwd() -> tuple[Path, Path]:
    root = Path(ROOT)
    runtime_cwd = root / "build" / "tmp" / "save_isolation_runtime"
    require_under_root(runtime_cwd, root / "build" / "tmp")
    if runtime_cwd.exists():
        shutil.rmtree(runtime_cwd)
    (runtime_cwd / "assets").mkdir(parents=True, exist_ok=True)

    candidates = [
        root / "build" / "game_seed" / "native-debug" / "assets" / "world67_art.ntpack",
        root / "build" / "game_seed" / "67-world-packs" / "world67_art.ntpack",
        root / "build" / "game_seed" / "native-release" / "assets" / "world67_art.ntpack",
    ]
    source_pack = next((candidate for candidate in candidates if candidate.exists()), None)
    if source_pack is None:
        raise DevApiError("world67_art.ntpack missing; build native assets before save isolation validation")
    shutil.copy2(source_pack, runtime_cwd / "assets" / "world67_art.ntpack")
    autosave_path = runtime_cwd / "build" / "saves" / "autosave" / "game.json"
    return runtime_cwd, autosave_path


def file_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def first_merge_via_ui(game) -> dict[str, Any]:
    game.wait_frames(3)
    game.click_ui("world.spawn", wait_frames=2)
    game.click_ui("world.spawn", wait_frames=2)
    game.click_ui("world.slot.00", wait_frames=2)
    return game.click_ui("world.slot.01", wait_frames=8)


def make_child_only_progress(game) -> dict[str, Any]:
    for _ in range(4):
        game.result("game.action.spawn_67")
        game.wait_frames(1)
    game.result("game.action.merge_matching_67")
    game.wait_frames(1)
    game.result("game.action.merge_matching_67")
    game.wait_frames(1)
    return game.result("game.action.merge_matching_67")


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    report_path = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("build/reports/package_save_isolation.json")
    screenshot_path = sys.argv[3] if len(sys.argv) > 3 else "build/captures/scenarios/package_save_isolation.png"
    report_path = Path(ROOT) / report_path if not report_path.is_absolute() else report_path
    report_path.parent.mkdir(parents=True, exist_ok=True)

    report: dict[str, Any] = {
        "checked_at": datetime.now(timezone.utc).isoformat(),
        "passed": False,
        "runtime": "native-debug DevAPI with isolated release-like cwd",
        "phases": {},
    }

    ok = True
    try:
        runtime_cwd, autosave_path = prepare_runtime_cwd()
        report["runtime_cwd"] = str(runtime_cwd)
        report["autosave_path"] = str(autosave_path)

        with running_game(
            port=port,
            exe=NATIVE_DEBUG_EXE,
            cwd=str(runtime_cwd),
            fresh_state=True,
            autosave_enabled=True,
        ) as game:
            scenario = Scenario(game)
            initial = game.result("game.state")
            ok &= scenario.check("isolated normal run starts clean", world(initial).get("board_used") == 0, world(initial))
            merged = first_merge_via_ui(game)
            ok &= scenario.check("normal gameplay creates Berry 67", merged.get("count_berry_67") == 1, merged)
            ok &= scenario.check("normal gameplay completes tutorial", merged.get("tutorial", {}).get("done") is True, merged.get("tutorial", {}))
            game.wait_frames(12)
            saved_state = game.result("game.state")
            ok &= scenario.check("normal autosave clears dirty state", saved_state.get("state_dirty") is False, saved_state)
            ok &= scenario.check("normal autosave file exists", autosave_path.exists() and autosave_path.stat().st_size > 0, str(autosave_path))
            ok &= scenario.ok
            report["phases"]["normal_autosave"] = {
                "collection_discovered_count": saved_state.get("collection_discovered_count"),
                "count_berry_67": saved_state.get("count_berry_67"),
                "tutorial_done": saved_state.get("tutorial", {}).get("done"),
                "autosave_size": autosave_path.stat().st_size if autosave_path.exists() else 0,
            }

        saved_hash = file_sha256(autosave_path)
        report["normal_autosave_hash"] = saved_hash

        with running_game(
            port=port,
            exe=NATIVE_DEBUG_EXE,
            cwd=str(runtime_cwd),
            fresh_state=False,
            autosave_enabled=True,
        ) as game:
            scenario = Scenario(game)
            game.wait_frames(6)
            loaded = game.result("game.state")
            ok &= scenario.check("normal restart loads Berry progress", loaded.get("count_berry_67") == 1, loaded)
            ok &= scenario.check("normal restart keeps tutorial done", loaded.get("tutorial", {}).get("done") is True, loaded.get("tutorial", {}))
            ok &= scenario.check("normal restart did not load Banana child state", loaded.get("count_banana_67") == 0, loaded)
            ok &= scenario.ok
            report["phases"]["normal_reload"] = {
                "collection_discovered_count": loaded.get("collection_discovered_count"),
                "count_berry_67": loaded.get("count_berry_67"),
                "count_banana_67": loaded.get("count_banana_67"),
                "tutorial_done": loaded.get("tutorial", {}).get("done"),
            }

        with running_game(
            port=port,
            exe=NATIVE_DEBUG_EXE,
            cwd=str(runtime_cwd),
            fresh_state=True,
            autosave_enabled=False,
        ) as game:
            scenario = Scenario(game)
            clean = game.result("game.state")
            ok &= scenario.check("fresh child-test run starts clean", world(clean).get("board_used") == 0, world(clean))
            ok &= scenario.check("fresh child-test ignores existing tutorial done", clean.get("tutorial", {}).get("done") is False, clean.get("tutorial", {}))
            child_progress = make_child_only_progress(game)
            ok &= scenario.check("fresh child-test can mutate in memory", child_progress.get("count_banana_67") == 1, child_progress)
            game.wait_frames(8)
            ok &= scenario.check("fresh no-autosave keeps original save hash", file_sha256(autosave_path) == saved_hash, str(autosave_path))
            ok &= scenario.ok
            report["phases"]["fresh_no_autosave_child_test"] = {
                "collection_discovered_count": child_progress.get("collection_discovered_count"),
                "count_berry_67": child_progress.get("count_berry_67"),
                "count_banana_67": child_progress.get("count_banana_67"),
                "state_dirty": child_progress.get("state_dirty"),
            }

        with running_game(
            port=port,
            exe=NATIVE_DEBUG_EXE,
            cwd=str(runtime_cwd),
            fresh_state=False,
            autosave_enabled=True,
        ) as game:
            scenario = Scenario(game)
            game.wait_frames(6)
            reloaded = game.result("game.state")
            ok &= scenario.check("normal reload after child-test keeps Berry save", reloaded.get("count_berry_67") == 1, reloaded)
            ok &= scenario.check("normal reload after child-test has no Banana overwrite", reloaded.get("count_banana_67") == 0, reloaded)
            ok &= scenario.check("normal reload after child-test keeps save hash", file_sha256(autosave_path) == saved_hash, str(autosave_path))
            captured = scenario.capture(screenshot_path, wait_frames=90)
            ok &= scenario.ok
            report["phases"]["normal_reload_after_child_test"] = {
                "collection_discovered_count": reloaded.get("collection_discovered_count"),
                "count_berry_67": reloaded.get("count_berry_67"),
                "count_banana_67": reloaded.get("count_banana_67"),
                "tutorial_done": reloaded.get("tutorial", {}).get("done"),
            }
            report["screenshot"] = captured

        report["passed"] = ok
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
        print("report:", report_path)
        print("screenshot:", report.get("screenshot"))
        return finish(ok)
    except DevApiError as exc:
        report["error"] = str(exc)
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
        return fail_devapi(exc)


if __name__ == "__main__":
    raise SystemExit(main())
