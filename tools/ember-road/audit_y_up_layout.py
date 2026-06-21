#!/usr/bin/env python3
"""Audit Ember Road town forge Y-up layout proof through code and DevAPI."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "devapi"))

from devapi_client import DevApiError, running_game  # noqa: E402

REPORT_MD = "gamedesign/projects/ember-road/reviews/T0024_town_forge_y_up_layout_audit.md"
REPORT_JSON = "gamedesign/projects/ember-road/reviews/T0024_town_forge_y_up_layout_audit.json"
SOURCE_PATH = "src/clean_seed_main.c"


def as_dict(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def field(state: dict[str, Any], group: str, name: str, fallback: Any = None) -> Any:
    nested = state.get(group)
    if isinstance(nested, dict) and name in nested:
        return nested.get(name)
    return state.get(f"{group}_{name}", fallback)


def node_map(tree: Any) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    nodes = tree.get("nodes") if isinstance(tree, dict) else None
    if not isinstance(nodes, list):
        return result
    for node in nodes:
        if isinstance(node, dict) and isinstance(node.get("id"), str):
            result[node["id"]] = node
    return result


def logical_rect(node: dict[str, Any], viewport_h: float) -> dict[str, float]:
    x = float(node.get("x", 0.0))
    y_down = float(node.get("y", 0.0))
    w = float(node.get("w", 0.0))
    h = float(node.get("h", 0.0))
    y_up = viewport_h - y_down - h
    return {
        "x": x,
        "y": y_up,
        "w": w,
        "h": h,
        "center_x": x + w * 0.5,
        "center_y": y_up + h * 0.5,
    }


def add_check(checks: list[dict[str, Any]], name: str, ok: bool, detail: str = "") -> None:
    checks.append({"name": name, "status": "pass" if ok else "fail", "detail": detail})


def source_line_number(text: str, needle: str) -> int | None:
    for index, line in enumerate(text.splitlines(), start=1):
        if needle in line:
            return index
    return None


def drive_to_town_forge(game: Any) -> dict[str, Any]:
    for method in (
        "game.reset_playtest",
        "game.action.accept_quest",
        "game.action.travel_north_road",
        "game.action.auto_battle",
        "game.action.equip_ring",
        "game.action.claim_reward",
        "game.action.enter_old_mine",
        "game.action.scout_old_mine",
        "game.action.resolve_old_mine_depth",
        "game.action.delve_old_mine",
        "game.action.return_old_gate",
    ):
        state = as_dict(game.result(method))
        game.wait_frames(1)
    return state


def render_markdown(record: dict[str, Any]) -> str:
    lines = [
        "---",
        "type: LayoutAudit",
        "project: ember-road",
        "task: T0024",
        "surface: town-forge-native-v2",
        f"verdict: {record['verdict']}",
        "---",
        "",
        "# T0024 Town Forge Y-Up Layout Audit",
        "",
        f"Verdict: **{record['verdict'].upper()}**",
        "",
        "## Source Boundary Evidence",
        "",
    ]
    for entry in record["source_evidence"]:
        lines.append(f"- {entry['label']}: `{entry['path']}:{entry['line']}`")
    lines.extend([
        "",
        "## Runtime Layout",
        "",
        "Coordinates below are converted back from DevAPI screen rectangles to logical Y-up rectangles.",
        "",
    ])
    for node_id, rect in record["logical_rects"].items():
        lines.append(
            f"- `{node_id}`: x={rect['x']:.1f}, y={rect['y']:.1f}, "
            f"w={rect['w']:.1f}, h={rect['h']:.1f}"
        )
    lines.extend(["", "## Checks", ""])
    for check in record["checks"]:
        detail = f" - {check['detail']}" if check.get("detail") else ""
        lines.append(f"- {check['status'].upper()} `{check['name']}`{detail}")
    lines.extend([
        "",
        "## Notes",
        "",
        "- Game/world/UI boxes remain authored in logical Y-up coordinates.",
        "- Renderer/input/DevAPI conversion is boundary-only and named in code.",
        "- This audit is proof for the current town forge native screen, not acceptance of broader Depth 2 content.",
        "",
    ])
    return "\n".join(lines)


def run(port: int, window_size: str, markdown_out: str, json_out: str) -> int:
    checks: list[dict[str, Any]] = []
    source_path = ROOT / SOURCE_PATH
    source = source_path.read_text(encoding="utf-8")
    required_source = [
        ("renderer boundary conversion", "static float sy(float y, float h)"),
        ("Y-up hit testing", "static bool contains_y_up"),
        ("input boundary conversion", "const float y_up = s_view_h - pointer.y;"),
        ("DevAPI boundary conversion", "const float y_down = s_view_h - box.y - box.h;"),
        ("town forge logical box", "s_forge_workbench_box = (UiBox)"),
        ("source-derived action plaque", "FORGE_ACTION_PANEL_V2"),
        ("source-derived result strip", "FORGE_RESULT_STRIP_SLICE9_V2"),
    ]
    source_evidence: list[dict[str, Any]] = []
    for label, needle in required_source:
        line = source_line_number(source, needle)
        add_check(checks, f"source:{label}", line is not None, needle)
        source_evidence.append({"label": label, "path": SOURCE_PATH, "line": line or 0})

    logical_rects: dict[str, dict[str, float]] = {}
    click_state: dict[str, Any] = {}
    launch_log_path = ""
    with running_game(port=port, window_size=window_size, fresh_state=True) as game:
        launch_log_path = str(game.launch_log_path or "")
        state = drive_to_town_forge(game)
        add_check(checks, "state:town lantern upgrade open", state.get("town_lantern_upgrade_open") is True, json.dumps(state)[:220])
        tree = game.result("ui.tree")
        nodes = node_map(tree)
        root = nodes.get("root", {})
        viewport_h = float(root.get("h", 0.0))
        add_check(checks, "runtime:viewport height", viewport_h > 0.0, str(viewport_h))
        required_nodes = [
            "ember.map.old_gate",
            "ember.map.north_road",
            "ember.map.old_mine",
            "ember.scene.forge_workbench",
            "ember.town.lantern_upgrade",
            "ember.forge_mine_lantern",
            "ember.primary",
        ]
        for node_id in required_nodes:
            add_check(checks, f"runtime:node:{node_id}", node_id in nodes)
            if node_id in nodes and viewport_h > 0.0:
                logical_rects[node_id] = logical_rect(nodes[node_id], viewport_h)

        if all(node_id in logical_rects for node_id in ("ember.map.old_gate", "ember.map.north_road", "ember.map.old_mine")):
            gate = logical_rects["ember.map.old_gate"]
            road = logical_rects["ember.map.north_road"]
            mine = logical_rects["ember.map.old_mine"]
            add_check(checks, "layout:route nodes ordered left-to-right", gate["x"] < road["x"] < mine["x"])

        if all(node_id in logical_rects for node_id in ("ember.scene.forge_workbench", "ember.map.old_mine", "ember.town.lantern_upgrade")):
            forge = logical_rects["ember.scene.forge_workbench"]
            mine = logical_rects["ember.map.old_mine"]
            action = logical_rects["ember.town.lantern_upgrade"]
            add_check(checks, "layout:forge is above route strip in Y-up", forge["center_y"] > mine["center_y"], f"forge={forge['center_y']:.1f}, mine={mine['center_y']:.1f}")
            add_check(checks, "layout:forge is left of right rail action", forge["center_x"] < action["x"], f"forge={forge['center_x']:.1f}, action_x={action['x']:.1f}")
            add_check(checks, "layout:right rail action stays below forge event", action["center_y"] < forge["center_y"], f"action={action['center_y']:.1f}, forge={forge['center_y']:.1f}")

        game.result("ui.click", {"id": "ember.scene.forge_workbench"})
        game.wait_frames(3)
        click_state = as_dict(game.result("game.state"))
        add_check(checks, "input:ui.click forge workbench triggers lantern", field(click_state, "gear", "mine_lantern") is True, json.dumps(click_state)[:220])
        add_check(checks, "input:ui.click unlocks depth 2", field(click_state, "old_mine", "depth2_unlocked") is True or click_state.get("old_mine_depth2_unlocked") is True, json.dumps(click_state)[:220])

    verdict = "pass" if all(check["status"] == "pass" for check in checks) else "fail"
    record = {
        "schema": "game.y_up_layout_audit",
        "version": 1,
        "project": "ember-road",
        "task": "T0024",
        "surface": "town-forge-native-v2",
        "verdict": verdict,
        "window_size": window_size,
        "source_evidence": source_evidence,
        "logical_rects": logical_rects,
        "click_state": click_state,
        "checks": checks,
        "launch_log": launch_log_path,
    }
    md_path = ROOT / markdown_out
    json_path = ROOT / json_out
    md_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.parent.mkdir(parents=True, exist_ok=True)
    md_path.write_text(render_markdown(record), encoding="utf-8")
    json_path.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(f"verdict: {verdict}")
    print(f"report: {markdown_out}")
    print(f"json: {json_out}")
    return 0 if verdict == "pass" else 1


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=9123)
    parser.add_argument("--window-size", default="1280x720")
    parser.add_argument("--report", default=REPORT_MD)
    parser.add_argument("--json-output", default=REPORT_JSON)
    args = parser.parse_args(argv)
    try:
        return run(args.port, args.window_size, args.report, args.json_output)
    except DevApiError as exc:
        print(f"devapi error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
