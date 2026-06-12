#!/usr/bin/env python3
"""One-pass agent playtest for the clean seed runtime."""

from __future__ import annotations

import json
import os
import sys
import time

from devapi_client import running_game


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "build/captures/agent_playtest"
    os.makedirs(out_dir, exist_ok=True)
    stamp = time.strftime("%Y%m%d_%H%M%S")

    report = {"ok": True, "checks": [], "screenshots": []}

    def check(name: str, condition: bool, detail: object = None) -> None:
        report["checks"].append({"name": name, "ok": bool(condition), "detail": detail})
        if not condition:
            report["ok"] = False

    with running_game(port=port, fresh_state=True) as game:
        endpoints = set(game.result("endpoints"))
        check("automation endpoints", {"ui.tree", "ui.click", "game.state", "frame.wait"}.issubset(endpoints), sorted(endpoints))

        initial = game.result("game.reset_playtest")
        check("fresh state", initial.get("test_ui_clicks") == 0, initial)

        initial_path = game.capture_screenshot(os.path.join(out_dir, f"initial_{stamp}.png"), wait_frames=2, audit=True)
        report["screenshots"].append(initial_path)

        for _ in range(3):
            state = game.click_ui("seed.cycle", wait_frames=2)
        check("button loop changes state", state.get("test_ui_clicks", 0) >= 3 and state.get("wallet", {}).get("soft", 0) >= 3, state)

        final_path = game.capture_screenshot(os.path.join(out_dir, f"after_clicks_{stamp}.png"), wait_frames=2, audit=True)
        report["screenshots"].append(final_path)
        report["final_state"] = game.result("game.state")

    print(json.dumps(report, indent=2))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
