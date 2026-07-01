#!/usr/bin/env python3
"""Minimal game-local DevAPI bot for the template.

This is intentionally copied with the game. It demonstrates where agents should
put semantic runtime scenarios while reusing ai_studio/runtime_automation for
transport, launch, frame waits, and capture.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path
from typing import Any


SCRIPT_PATH = Path(__file__).resolve()
GAME_ROOT = SCRIPT_PATH.parents[1]


def find_repo_root(start: Path) -> Path:
    for candidate in (start, *start.parents):
        if (candidate / "ai_studio" / "runtime_automation" / "devapi_client.py").exists():
            return candidate
    raise RuntimeError("could not find repo root with ai_studio/runtime_automation/devapi_client.py")


REPO_ROOT = find_repo_root(GAME_ROOT)
RUNTIME_AUTOMATION = REPO_ROOT / "ai_studio" / "runtime_automation"
if str(RUNTIME_AUTOMATION) not in sys.path:
    sys.path.insert(0, str(RUNTIME_AUTOMATION))

from devapi_client import DEFAULT_DEVAPI_PORT, DevApiError, running_game  # noqa: E402


REQUIRED_METHODS = {
    "endpoints",
    "command.describe",
    "frame.wait",
    "frame.current",
    "render.info",
    "render.set_enabled",
    "ui.tree",
    "ui.click",
    "capture.frame",
    "game.state",
}


def exe_name() -> str:
    return "game.exe" if os.name == "nt" else "game"


def default_executable(game_root: Path = GAME_ROOT) -> Path:
    env_path = os.environ.get("AI_STUDIO_GAME_EXE")
    if env_path:
        return Path(env_path)
    candidates = [
        game_root / "build" / "devapi-debug" / "bin" / exe_name(),
        game_root / "build" / "bin" / exe_name(),
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def extract_endpoint_methods(listing: Any) -> set[str]:
    methods: set[str] = set()
    if isinstance(listing, dict) and isinstance(listing.get("commands"), list):
        items = listing["commands"]
    elif isinstance(listing, list):
        items = listing
    else:
        return methods

    for item in items:
        method = item.get("method") if isinstance(item, dict) else item
        if isinstance(method, str):
            methods.add(method)
    return methods


def missing_required_methods(methods: set[str]) -> list[str]:
    return sorted(REQUIRED_METHODS - methods)


def ui_nodes(tree: Any) -> list[dict[str, Any]]:
    if isinstance(tree, dict) and isinstance(tree.get("nodes"), list):
        return [node for node in tree["nodes"] if isinstance(node, dict)]
    return []


def find_ui_node(tree: Any, element_id: str) -> dict[str, Any] | None:
    for node in ui_nodes(tree):
        if node.get("id_string") == element_id or node.get("id") == element_id:
            return node
    return None


def validate_game_state(state: Any) -> dict[str, Any]:
    if not isinstance(state, dict):
        raise DevApiError(f"game.state returned {type(state).__name__}, expected object")
    if state.get("schema") != "template.game_state.snapshot.v1":
        raise DevApiError(f"unexpected game.state schema: {state.get('schema')!r}")
    live_state = state.get("state")
    if not isinstance(live_state, dict):
        raise DevApiError("game.state missing state object")
    for key in ("player", "settings"):
        if not isinstance(live_state.get(key), dict):
            raise DevApiError(f"game.state missing state.{key} object")
    return state


def wait_for_ui_id(game: Any, element_id: str, *, max_frames: int = 90, stride: int = 3) -> dict[str, Any]:
    last_error: Exception | None = None
    for _ in range(max(1, max_frames // max(1, stride))):
        try:
            tree = game.result("ui.tree")
        except DevApiError as exc:
            last_error = exc
        else:
            if find_ui_node(tree, element_id) is not None:
                return tree
        game.wait_frames(stride)
    detail = f"; last error: {last_error}" if last_error else ""
    raise DevApiError(f"ui id {element_id!r} did not appear after {max_frames} frames{detail}")


def run_smoke(game: Any, out_dir: Path, *, audit: bool = True) -> dict[str, Any]:
    out_dir.mkdir(parents=True, exist_ok=True)

    endpoint_listing = game.result("endpoints")
    methods = extract_endpoint_methods(endpoint_listing)
    missing = missing_required_methods(methods)
    if missing:
        raise DevApiError(f"missing required DevAPI methods: {', '.join(missing)}")

    described = {
        method: game.result("command.describe", {"method": method})
        for method in ("render.set_enabled", "ui.click", "capture.frame", "game.state")
    }

    closed_tree = wait_for_ui_id(game, "settings/gear")
    state_before = validate_game_state(game.result("game.state"))
    render_before = game.result("render.info")
    game.result("render.set_enabled", {"enabled": False})
    game.wait_frames(2)
    render_disabled = game.result("render.info")
    if render_disabled.get("enabled") is not False:
        raise DevApiError(f"render.set_enabled(false) did not take effect: {render_disabled}")
    game.result("render.set_enabled", {"enabled": True})
    game.wait_frames(2)
    render_enabled = game.result("render.info")
    if render_enabled.get("enabled") is not True:
        raise DevApiError(f"render.set_enabled(true) did not take effect: {render_enabled}")

    screenshot = game.capture_screenshot(str(out_dir / "first_screen.png"), wait_frames=2, audit=audit)
    summary = {
        "schema": "template.devapi_smoke.v1",
        "method_count": len(methods),
        "required_methods": sorted(REQUIRED_METHODS),
        "described_methods": sorted(described.keys()),
        "ui_tree_nodes": len(ui_nodes(closed_tree)),
        "stable_ui_id": "settings/gear",
        "game_state": state_before,
        "render_before": render_before,
        "render_disabled": render_disabled,
        "render_enabled": render_enabled,
        "action": "render.set_enabled false -> true",
        "screenshot": screenshot,
    }
    summary_path = out_dir / "summary.json"
    summary["summary"] = str(summary_path)
    with summary_path.open("w", encoding="utf-8") as handle:
        json.dump(summary, handle, indent=2)
        handle.write("\n")
    return summary


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Launch the template and run a minimal DevAPI bot.")
    parser.add_argument("--exe", type=Path, default=default_executable(), help="Native debug executable to launch.")
    parser.add_argument("--port", type=int, default=DEFAULT_DEVAPI_PORT, help="DevAPI TCP port.")
    parser.add_argument("--reuse", action="store_true", help="Attach to an existing DevAPI process when available.")
    parser.add_argument("--window-size", default="640x360", help="Window size passed to the game.")
    parser.add_argument("--out", type=Path, default=REPO_ROOT / "tmp" / "template_devapi_smoke", help="Evidence output directory.")
    parser.add_argument("--no-audit", action="store_true", help="Skip pixel-health audit for the captured screenshot.")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    exe = args.exe.resolve()
    if not args.reuse and not exe.exists():
        print(f"build native Debug first or pass --exe: {exe}", file=sys.stderr)
        return 2

    try:
        with running_game(
            port=args.port,
            exe=str(exe),
            cwd=str(REPO_ROOT),
            reuse_existing=args.reuse,
            fresh_state=True,
            autosave_enabled=False,
            window_size=args.window_size,
        ) as game:
            summary = run_smoke(game, args.out, audit=not args.no_audit)
    except DevApiError as exc:
        print(f"devapi smoke failed: {exc}", file=sys.stderr)
        return 1

    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
