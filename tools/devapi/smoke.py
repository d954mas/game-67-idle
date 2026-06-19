#!/usr/bin/env python3
"""Clean-seed DevAPI smoke test.

Asserts the universal DevAPI contract plus the tiny seed surface that every new
iteration starts from. A new game may replace the ``SEED`` block after it has a
real first screen, but this file must not point at a closed project.

Usage: py -3.12 tools/devapi/smoke.py [port]
"""
from __future__ import annotations

import sys

from devapi_client import running_game

SEED = {
    "primary_ui_ids": {"root", "seed.cycle", "seed.progress"},
    "expected_state_key": "shape_index",
}

UNIVERSAL_ENDPOINTS = {
    "ping", "endpoints", "frame.wait", "ui.tree", "ui.click",
    "game.reset_playtest", "game.state", "game.state.schema",
    "game.state.get", "game.state.set", "game.state.save", "game.state.load",
}

SEED_ENDPOINTS = {"game.action.cycle"}


def check(name: str, condition: object, detail: object = None) -> bool:
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
        ok &= check("universal endpoints present", UNIVERSAL_ENDPOINTS.issubset(endpoints),
                    sorted(UNIVERSAL_ENDPOINTS - endpoints))
        ok &= check("seed endpoints present", SEED_ENDPOINTS.issubset(endpoints),
                    sorted(SEED_ENDPOINTS - endpoints))
        ok &= check("ping", game.result("ping") is not None)

        state = game.result("game.reset_playtest") or {}
        key = SEED["expected_state_key"]
        ok &= check(f"reset_playtest returns {key}", key in state, state)

        tree = game.result("ui.tree")
        ids = {node.get("id") for node in tree}
        ok &= check("ui.tree exposes seed ids", SEED["primary_ui_ids"].issubset(ids),
                    sorted(SEED["primary_ui_ids"] - ids))

        patched = game.result("game.state.set", {"doc": "game", "path": "settings.master_volume", "value": 0.5})
        ok &= check("state.set round-trips",
                    abs(patched.get("settings", {}).get("master_volume", 0.0) - 0.5) < 0.01, patched)

        before_shape = state.get("shape_index")
        after_cycle = game.result("game.action.cycle") or {}
        ok &= check("game.action.cycle changes shape",
                    after_cycle.get("shape_index") != before_shape,
                    {"before": before_shape, "after": after_cycle})

        shot = game.capture_screenshot("build/captures/smoke.png", audit=False)
        ok &= check("screenshot captured", bool(shot), shot)

        game.click_ui("seed.cycle")
        clicked = game.result("game.state")
        ok &= check("ui.click seed cycle updates state",
                    clicked.get("shape_index") != after_cycle.get("shape_index"),
                    clicked)

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
