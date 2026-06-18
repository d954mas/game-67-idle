#!/usr/bin/env python3
"""Game-agnostic DevAPI smoke test.

Asserts only the UNIVERSAL DevAPI contract every game registers (ping, endpoints,
frame.wait, ui.tree, state get/set, reset_playtest, in-process framebuffer), plus
a tiny per-game data block. The NEXT game edits ``GAME`` below — its primary UI
ids and one expected state key — not the assertions, so this harness keeps
working across games instead of being hardwired to one game's actions.

Usage: py -3.12 tools/devapi/smoke.py [port]
"""
from __future__ import annotations

import sys

from devapi_client import running_game

# --- per-game data: edit THIS for a new game, not the checks below -----------
GAME = {
    # ui.tree node ids the game's first screen must expose (root is universal).
    "primary_ui_ids": {"root", "backrooms.objective", "backrooms.fear", "backrooms.battery", "backrooms.threat"},
    # one key game.reset_playtest must return (set to None to skip).
    "expected_state_key": "stalker_pressure",
}

UNIVERSAL_ENDPOINTS = {
    "ping", "endpoints", "frame.wait", "ui.tree", "ui.click",
    "game.reset_playtest", "game.state", "game.state.schema",
    "game.state.get", "game.state.set", "game.state.save", "game.state.load",
}

GAME_ENDPOINTS = {
    "game.audio.status",
}


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
        ok &= check("game endpoints present", GAME_ENDPOINTS.issubset(endpoints),
                    sorted(GAME_ENDPOINTS - endpoints))
        ok &= check("ping", game.result("ping") is not None)

        state = game.result("game.reset_playtest") or {}
        key = GAME["expected_state_key"]
        if key is not None:
            ok &= check(f"reset_playtest returns {key}", key in state, state)

        tree = game.result("ui.tree")
        ids = {node.get("id") for node in tree}
        ok &= check("ui.tree exposes primary ids", GAME["primary_ui_ids"].issubset(ids),
                    sorted(GAME["primary_ui_ids"] - ids))

        patched = game.result("game.state.set", {"doc": "game", "path": "settings.master_volume", "value": 0.5})
        ok &= check("state.set round-trips",
                    abs(patched.get("settings", {}).get("master_volume", 0.0) - 0.5) < 0.01, patched)

        before_audio = game.result("game.audio.status")
        before_flashlight = before_audio.get("cue_play_count", {}).get("flashlight", 0)
        game.result("game.action.toggle_flashlight")
        after_audio = game.result("game.audio.status")
        after_flashlight = after_audio.get("cue_play_count", {}).get("flashlight", 0)
        ok &= check("audio cue count increments",
                    after_flashlight > before_flashlight,
                    {"before": before_audio, "after": after_audio})

        shot = game.capture_screenshot("build/captures/smoke.png", audit=False)
        ok &= check("screenshot captured", bool(shot), shot)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
