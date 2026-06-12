#!/usr/bin/env python3
"""Broader probe for the seed DevAPI contract."""

from __future__ import annotations

import sys

from devapi_client import running_game


def check(name: str, condition: bool, detail: object = None) -> bool:
    if condition:
        print(f"PASS {name}")
        return True
    print(f"FAIL {name}: {detail!r}", file=sys.stderr)
    return False


def node_by_id(tree: list[dict], node_id: str) -> dict | None:
    return next((node for node in tree if node.get("id") == node_id), None)


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    ok = True
    with running_game(port=port, fresh_state=True) as game:
        ok &= check("ping", game.result("ping").get("pong") is True)
        view = game.result("view")
        ok &= check("view has dimensions", view.get("fb_w", 0) > 0 and view.get("fb_h", 0) > 0, view)

        tree = game.result("ui.tree")
        button = node_by_id(tree, "seed.cycle")
        ok &= check("seed button visible", bool(button and button.get("visible") and button.get("enabled")), button)

        state = game.result("game.state")
        ok &= check("game.state readable", isinstance(state.get("test_ui_clicks"), int) and "settings" in state, state)

        before = state.get("test_ui_clicks", 0)
        game.result("input.key", {"key": "SPACE", "mode": "tap", "hold_frames": 1})
        after = game.batch_results([("frame.wait", {"frames": 2}), ("game.state", {})])[-1]
        ok &= check("input.key SPACE changes state", after.get("test_ui_clicks", 0) > before, after)

        schema = game.result("game.state.schema", {"doc": "game"})
        ok &= check("schema id", schema.get("schema") == "game_seed.state", schema)

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
