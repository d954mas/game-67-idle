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
    "game.state.schema",
    "game.state.get",
    "game.events.tail",
    "game.iteration.proof",
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


def validate_game_state_schema(schema: Any) -> dict[str, Any]:
    # A5: game.state.schema returns a per-fragment aggregate { "game": <schema>, ... }.
    # A6: the aggregate now also carries the `settings` fragment.
    if not isinstance(schema, dict):
        raise DevApiError(f"game.state.schema returned {type(schema).__name__}, expected object")
    frag = schema.get("game")
    if not isinstance(frag, dict):
        raise DevApiError("game.state.schema missing 'game' fragment")
    if frag.get("schema") != "game_seed.state":
        raise DevApiError(f"unexpected game.state.schema id: {frag.get('schema')!r}")
    # The normalized schema carries both `fragment` and `document` — accept either.
    if frag.get("fragment") != "game" and frag.get("document") != "game":
        raise DevApiError(f"unexpected game.state.schema fragment: {frag.get('fragment')!r}")
    if not isinstance(frag.get("fields"), list):
        raise DevApiError("game.state.schema 'game' missing fields array")
    settings = schema.get("settings")
    if not isinstance(settings, dict) or not isinstance(settings.get("fields"), list):
        raise DevApiError("game.state.schema missing 'settings' fragment fields")
    return schema


def validate_game_state(state: Any) -> dict[str, Any]:
    # A6: get {path:""} returns the multi-fragment aggregate
    # { path:"", value:{ settings:{...}, game:{...} } } — settings is now its own
    # top-level fragment beside game (no longer nested under game.settings).
    if not isinstance(state, dict):
        raise DevApiError(f"game.state.get returned {type(state).__name__}, expected object")
    if state.get("path") != "":
        raise DevApiError(f"unexpected game.state.get path: {state.get('path')!r}")
    value = state.get("value")
    if not isinstance(value, dict):
        raise DevApiError("game.state.get missing value object")
    game = value.get("game")
    if not isinstance(game, dict):
        raise DevApiError("game.state.get missing value.game fragment")
    for key in ("tutorial",):  # settings removed from the game set; T0327 hygiene
        # gutted the dead prototype inventory.item_ids field (owned by items now)
        if not isinstance(game.get(key), dict):
            raise DevApiError(f"game.state.get missing value.game.{key} object")
    settings = value.get("settings")  # new fragment beside game
    if not isinstance(settings, dict):
        raise DevApiError("game.state.get missing value.settings fragment")
    if not isinstance(settings.get("master_volume"), (int, float)):
        raise DevApiError("value.settings.master_volume is not a number")
    return state


def validate_events_tail(tail: Any) -> dict[str, Any]:
    # E3: game.events.tail returns the render-at-copy ring window. The template emits no
    # events by default, so `events` is typically []; validate SHAPE, tolerate empty.
    if not isinstance(tail, dict):
        raise DevApiError(f"game.events.tail returned {type(tail).__name__}, expected object")
    if not isinstance(tail.get("events"), list):
        raise DevApiError("game.events.tail missing 'events' array")
    for key in ("next_seq", "dropped", "evicted"):
        if not isinstance(tail.get(key), (int, float)):
            raise DevApiError(f"game.events.tail missing numeric '{key}'")
    for ev in tail["events"]:  # each rendered event is a self-contained object
        if not isinstance(ev, dict) or not isinstance(ev.get("seq"), (int, float)) \
                or not isinstance(ev.get("type"), str):
            raise DevApiError("game.events.tail event missing seq/type")
    return tail


def validate_iteration_proof(proof: Any) -> dict[str, Any]:
    if not isinstance(proof, dict):
        raise DevApiError("game.iteration.proof returned a non-object")
    for key in ("cFixture", "schemaFixture"):
        if not isinstance(proof.get(key), str) or not proof[key]:
            raise DevApiError(f"game.iteration.proof missing non-empty {key}")
    return proof


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
        for method in ("render.set_enabled", "ui.click", "capture.frame", "game.state.schema", "game.state.get", "game.events.tail", "game.iteration.proof")
    }

    closed_tree = wait_for_ui_id(game, "settings/gear")
    state_schema = validate_game_state_schema(game.result("game.state.schema"))
    state_before = validate_game_state(game.result("game.state.get", {"path": ""}))
    events_tail = validate_events_tail(game.result("game.events.tail", {}))
    iteration_proof = validate_iteration_proof(game.result("game.iteration.proof", {}))
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
        "schema": "template.devapi_smoke.v4",
        "method_count": len(methods),
        "required_methods": sorted(REQUIRED_METHODS),
        "described_methods": sorted(described.keys()),
        "ui_tree_nodes": len(ui_nodes(closed_tree)),
        "stable_ui_id": "settings/gear",
        "game_state_schema": state_schema,
        "game_state": state_before,
        "events_tail": events_tail,
        "iteration_proof": iteration_proof,
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
