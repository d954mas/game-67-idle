#!/usr/bin/env python3
"""Dragon Grove first-slice smoke.

Build first, then launch native DevAPI, run one merge, and capture a screenshot.
"""

from __future__ import annotations

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "devapi"))

from devapi_client import DevApiError, running_game  # noqa: E402


SHOT = "build/captures/dragon-grove-smoke.png"


def unwrap_state(result: dict) -> dict:
    state = result.get("state") if isinstance(result, dict) else None
    if not isinstance(state, dict):
        raise AssertionError(f"expected state object, got {result!r}")
    return state


def assert_board_has_level2_reward(state: dict) -> None:
    board = state.get("board")
    if not isinstance(board, list):
        raise AssertionError("state.board missing")
    rewards = [cell for cell in board if cell.get("level") == 2 and cell.get("item") in {"hatchling", "bloom"}]
    if not rewards:
        raise AssertionError("expected at least one level-2 hatchling/bloom after merge")


def main() -> int:
    try:
        with running_game(port=9123, fresh_state=True, window_size="960x540") as game:
            methods = game.endpoint_methods()
            required = {"game.state", "game.reset_playtest", "game.action.merge_ready", "ui.tree"}
            missing = sorted(required - methods)
            if missing:
                raise AssertionError(f"missing endpoints: {missing}")

            initial = unwrap_state(game.result("game.reset_playtest"))
            if initial.get("runtime") != "dragon_grove":
                raise AssertionError(f"wrong runtime: {initial.get('runtime')!r}")
            if initial.get("has_merge") is not True:
                raise AssertionError("initial state should have a merge-ready group")

            after = unwrap_state(game.step("game.action.merge_ready", wait_frames=2))
            if after.get("merge_count") != 1 or after.get("restored_tiles") != 1:
                raise AssertionError(f"first merge failed: {after!r}")
            if "restored" not in str(after.get("last_feedback_text", "")).lower():
                raise AssertionError(f"feedback did not name restore: {after.get('last_feedback_text')!r}")
            assert_board_has_level2_reward(after)

            after = unwrap_state(game.step("game.action.merge_ready", wait_frames=2))
            if after.get("merge_count") != 2 or after.get("restored_tiles") != 2:
                raise AssertionError(f"second merge failed: {after!r}")

            after = unwrap_state(game.step("game.action.merge_ready", wait_frames=2))
            if after.get("merge_count") != 3 or after.get("restored_tiles") != 3:
                raise AssertionError(f"third merge failed: {after!r}")

            blocked = unwrap_state(game.step("game.action.merge_ready", wait_frames=2))
            if blocked.get("has_merge") is not False:
                raise AssertionError("expected no merge-ready group after three merges")
            if "no group" not in str(blocked.get("blocked_reason_text", "")).lower():
                raise AssertionError(f"blocked reason missing: {blocked.get('blocked_reason_text')!r}")

            ui = game.result("ui.tree")
            nodes = ui.get("nodes", []) if isinstance(ui, dict) else []
            node_ids = {node.get("id") for node in nodes if isinstance(node, dict)}
            for node_id in ("grove.board", "grove.merge_ready", "grove.progress"):
                if node_id not in node_ids:
                    raise AssertionError(f"missing UI node {node_id}")

            shot = game.capture_screenshot(SHOT, wait_frames=2, audit=True)
            print(f"PASS dragon-grove smoke: {shot}")
            print(
                "checks: endpoints, runtime=dragon_grove, merge_count=3, "
                "restored_tiles=3, level-2 reward, blocked state, ui.tree nodes, screenshot"
            )
            return 0
    except (AssertionError, DevApiError) as exc:
        print(f"FAIL dragon-grove smoke: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
