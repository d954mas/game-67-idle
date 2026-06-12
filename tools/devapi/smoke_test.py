#!/usr/bin/env python3
"""Smoke test for the clean seed runtime and universal DevAPI surface."""

from __future__ import annotations

import sys

from devapi_client import running_game


def check(name: str, condition: bool, detail: object = None) -> bool:
    if condition:
        print(f"PASS {name}")
        return True
    print(f"FAIL {name}: {detail!r}", file=sys.stderr)
    return False


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    ok = True
    with running_game(port=port, fresh_state=True) as game:
        endpoints = set(game.result("endpoints"))
        required = {
            "ping",
            "endpoints",
            "frame.wait",
            "ui.tree",
            "ui.click",
            "game.state",
            "game.reset_playtest",
            "game.state.schema",
            "game.state.get",
            "game.state.set",
            "game.state.save",
            "game.state.load",
        }
        ok &= check("required endpoints", required.issubset(endpoints), sorted(required - endpoints))

        state0 = game.result("game.reset_playtest")
        ok &= check("reset returns seed state", state0.get("test_ui_clicks") == 0 and state0.get("wallet", {}).get("soft") == 0, state0)

        tree = game.result("ui.tree")
        ids = {node.get("id") for node in tree}
        ok &= check("ui tree exposes seed button", {"root", "seed.panel", "seed.cycle"}.issubset(ids), tree)

        clicked = game.click_ui("seed.cycle", wait_frames=2)
        ok &= check("ui.click changes state", clicked.get("test_ui_clicks", 0) >= 1, clicked)

        patched = game.result("game.state.set", {"doc": "game", "path": "settings.master_volume", "value": 0.5})
        ok &= check("state set works", abs(patched.get("settings", {}).get("master_volume", 0.0) - 0.5) < 0.01, patched)

        shot = game.capture_screenshot("build/captures/devapi_smoke.png", wait_frames=2, audit=True)
        ok &= check("screenshot captured", bool(shot), shot)

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
