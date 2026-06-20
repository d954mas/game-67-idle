#!/usr/bin/env python3
"""Clean-seed DevAPI smoke test.

This script intentionally stays game-agnostic. It proves that the native seed
launches, exposes the reusable DevAPI surface, and accepts a basic UI click.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from typing import Any

from devapi_client import DevApiError, running_game


@dataclass
class SmokeReport:
    passed: int = 0
    failed: int = 0

    def check(self, section: str, name: str, ok: bool, detail: str = "") -> None:
        status = "PASS" if ok else "FAIL"
        suffix = f" - {detail}" if detail else ""
        print(f"{status} {section}: {name}{suffix}")
        if ok:
            self.passed += 1
        else:
            self.failed += 1

    def exit_code(self) -> int:
        print(f"summary: {self.passed} passed, {self.failed} failed")
        return 1 if self.failed else 0


def collect_ui_ids(node: Any) -> set[str]:
    ids: set[str] = set()
    if isinstance(node, dict):
        nodes = node.get("nodes")
        if isinstance(nodes, list):
            for child in nodes:
                ids.update(collect_ui_ids(child))
        node_id = node.get("id")
        if isinstance(node_id, str):
            ids.add(node_id)
        children = node.get("children")
        if isinstance(children, list):
            for child in children:
                ids.update(collect_ui_ids(child))
    elif isinstance(node, list):
        for child in node:
            ids.update(collect_ui_ids(child))
    return ids


def run(port: int) -> int:
    report = SmokeReport()
    try:
        with running_game(port=port, window_size="960x540") as game:
            methods = game.endpoint_methods()
            expected_methods = {
                "ping",
                "endpoints",
                "command.describe",
                "frame.wait",
                "game.state",
                "game.reset_playtest",
                "game.action.cycle",
                "ui.tree",
                "ui.click",
            }
            missing = sorted(expected_methods - methods)
            report.check("devapi", "expected endpoints", not missing, f"missing={missing}" if missing else "")

            state = game.observe()
            report.check("state", "runtime is clean seed", state.get("runtime") == "clean_seed", str(state.get("runtime")))
            report.check("state", "shape is exposed", isinstance(state.get("shape"), str) and bool(state.get("shape")), str(state.get("shape")))

            ui_tree = game.result("ui.tree")
            ui_ids = collect_ui_ids(ui_tree)
            report.check("ui", "cycle button exists", "seed.cycle" in ui_ids, f"ids={sorted(ui_ids)}")
            report.check("ui", "progress meter exists", "seed.progress" in ui_ids, f"ids={sorted(ui_ids)}")

            before_clicks = int(state.get("test_ui_clicks", 0))
            before_shape = state.get("shape")
            after = game.click_ui("seed.cycle", wait_frames=2)
            after_clicks = int(after.get("test_ui_clicks", 0))
            report.check(
                "ui",
                "cycle click mutates state",
                after_clicks > before_clicks and after.get("shape") != before_shape,
                f"{before_shape}/{before_clicks} -> {after.get('shape')}/{after_clicks}",
            )
    except DevApiError as exc:
        report.check("devapi", "smoke run", False, str(exc))

    return report.exit_code()


def main(argv: list[str]) -> int:
    port = int(argv[1]) if len(argv) > 1 else 9123
    return run(port)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
