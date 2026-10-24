#!/usr/bin/env python3
"""Capture and check every game-owned UI window contract in two orientations."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from devapi_client import DevApiError, running_game  # noqa: E402
from responsive_viewports import ViewportCase, resolve_launch  # noqa: E402
from window_layout_contract import check_contract  # noqa: E402


def _slug(root_id: str) -> str:
    return re.sub(r"[^a-zA-Z0-9._-]+", "_", root_id)


def _tree(game: Any, context: str | None) -> dict[str, Any]:
    params = {"ctx": context} if context else None
    result = game.result("ui.tree", params)
    if not isinstance(result, dict):
        raise DevApiError("ui.tree returned no tree")
    return result


def _wait_for_window(game: Any, root_id: str, context: str | None) -> dict[str, Any]:
    last_error: Exception | None = None
    for _ in range(60):
        try:
            tree = _tree(game, context)
            if any(node.get("id_string") == root_id and node.get("visible", True) for node in tree.get("nodes", [])):
                return tree
        except DevApiError as exc:
            last_error = exc
        game.wait_frames(3)
    raise DevApiError(f"{root_id} did not open within 180 frames: {last_error or 'no visible root'}")


def capture_window(game: Any, entry: dict[str, Any], viewport: ViewportCase, out: Path) -> dict[str, Any]:
    root_id = entry["id"]
    context = entry.get("context")
    commands = entry.get("open") or [{"method": "game.scene.show", "params": {"scene": entry["scene"]}}]
    for command in commands:
        method = command["method"]
        if method.startswith("ui.") or method.startswith("input."):
            with game.player_gated():
                game.result(method, command.get("params", {}))
        else:
            game.result(method, command.get("params", {}))
        game.wait_frames(3)
        if command.get("waitFor"):
            _wait_for_window(game, command["waitFor"], context)
    tree = _wait_for_window(game, root_id, context)
    game.capture_screenshot(str(out / f"{_slug(root_id)}-{viewport.name}.png"), wait_frames=0)
    pairs: dict[str, dict[str, Any]] = {}
    for target in entry.get("scroll", []) or [root_id]:
        before = _tree(game, context)
        params: dict[str, Any] = {"id": target, "dy": 2}
        if context:
            params["ctx"] = context
        with game.player_gated():
            game.result("ui.scroll", params)
        game.wait_frames(12)
        pairs[target] = {"before": before, "after": _tree(game, context)}
    return {"tree": tree, "after_scroll": pairs}


def capture_contract(
    contract: dict[str, Any], game_root: Path, plan: Any, out: Path,
    viewports: tuple[ViewportCase, ViewportCase], startup_timeout: float = 8.0,
) -> tuple[dict[str, Any], dict[str, Any]]:
    out.mkdir(parents=True, exist_ok=True)
    scene_list: dict[str, Any] | None = None
    proof: dict[str, Any] = {"schema": "ai_studio.ui_window_proof.v1", "windows": []}
    for entry in contract["windows"]:
        record: dict[str, Any] = {"id": entry["id"], "after_scroll": {}}
        for viewport in viewports:
            with running_game(
                exe=str(plan.exe), cwd=str(plan.cwd), fresh_state=plan.fresh_state,
                autosave_enabled=plan.autosave_enabled, window_size=viewport.window_size,
                extra_args=list(plan.extra_args), startup_timeout=startup_timeout,
            ) as game:
                if scene_list is None:
                    try:
                        scene_list = game.result("game.scene.list")
                    except DevApiError:
                        scene_list = {"scenes": []}
                capture = capture_window(game, entry, viewport, out)
                record[viewport.name] = capture["tree"]
                record["after_scroll"][viewport.name] = capture["after_scroll"]
        proof["windows"].append(record)
    if scene_list is None:
        raise DevApiError("contract contains no windows")
    (out / "proof.json").write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
    (out / "scene-list.json").write_text(json.dumps(scene_list, indent=2) + "\n", encoding="utf-8")
    return proof, scene_list


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", required=True, help="game id in Studio catalog")
    parser.add_argument("--contract", type=Path, help="default: <game>/design/ui_window_contracts.json")
    parser.add_argument("--out", type=Path, help="default: <game>/build/quality/ui_windows")
    parser.add_argument("--exe", type=Path, help="override catalog native executable")
    parser.add_argument("--cwd", type=Path, help="override game working directory")
    parser.add_argument("--launch-arg", action="append", default=[])
    parser.add_argument("--fresh-state", action=argparse.BooleanOptionalAction, default=None)
    parser.add_argument("--autosave", action=argparse.BooleanOptionalAction, default=None)
    parser.add_argument("--startup-timeout", type=float, default=8.0)
    args = parser.parse_args()
    try:
        plan = resolve_launch(args)
        if plan.game_root is None:
            raise ValueError("game root is unavailable")
        contract_path = args.contract or plan.game_root / "design" / "ui_window_contracts.json"
        contract = json.loads(contract_path.read_text(encoding="utf-8"))
        static_errors = check_contract(contract, plan.game_root)
        if static_errors:
            raise ValueError("; ".join(static_errors))
        if not plan.exe.is_file():
            raise ValueError(f"build native DevAPI executable first: {plan.exe}")
        out = args.out or plan.game_root / "build" / "quality" / "ui_windows"
        viewports = (ViewportCase("portrait", 390, 844), ViewportCase("landscape", 844, 390))
        proof, scenes = capture_contract(contract, plan.game_root, plan, out, viewports, args.startup_timeout)
        errors = check_contract(contract, plan.game_root, proof, scenes)
        if errors:
            raise ValueError("; ".join(errors))
    except (OSError, ValueError, json.JSONDecodeError, DevApiError) as exc:
        print(f"window contract capture failed: {exc}", file=sys.stderr)
        return 1
    print(f"PASS: {len(contract['windows'])} windows at portrait and landscape; proof: {out / 'proof.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
