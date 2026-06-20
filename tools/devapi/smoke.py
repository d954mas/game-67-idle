#!/usr/bin/env python3
"""Blockfell Runes DevAPI smoke test.

Usage: py -3.12 tools/devapi/smoke.py [port]
"""
from __future__ import annotations

import sys

from devapi_client import running_game

SLICE = {
    "primary_ui_ids": {
        "root",
        "rune.progress",
        "quest.chain",
        "combat.status",
        "loot.status",
        "ability.belt",
        "action.claim",
        "action.attack",
    },
    "runtime": "blockfell_runes",
}

UNIVERSAL_ENDPOINTS = {
    "ping", "endpoints", "command.describe", "frame.wait",
    "ui.tree", "ui.element", "ui.click",
    "game.state", "game.state.schema", "game.state.get", "game.state.set",
    "game.state.save", "game.state.load", "game.state.reset",
}

SLICE_ENDPOINTS = {
    "game.action.cycle",
    "game.action.claim",
    "game.action.attack",
    "game.playtest.move_to_rune",
    "game.playtest.move_to_camp",
    "game.playtest.move_to_chest",
    "game.playtest.complete_slice",
    "game.capture.framebuffer",
}


class AcceptanceReport:
    def __init__(self) -> None:
        self.passed: list[str] = []
        self.failed: list[str] = []

    def check(self, category: str, name: str, condition: object, detail: object = None) -> bool:
        check_id = f"{category}.{name}"
        if condition:
            self.passed.append(check_id)
            print(f"PASS {check_id}")
            return True
        self.failed.append(check_id)
        print(f"FAIL {check_id}: {detail!r}", file=sys.stderr)
        return False

    def summary(self) -> bool:
        print(f"SUMMARY acceptance_checks passed={len(self.passed)} failed={len(self.failed)}")
        if self.failed:
            print("FAILED acceptance_checks " + ", ".join(self.failed), file=sys.stderr)
            return False
        return True


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    ok = True
    report = AcceptanceReport()
    with running_game(port=port, fresh_state=True) as game:
        endpoints = game.endpoint_methods()
        ok &= report.check("infra", "universal_endpoints_present", UNIVERSAL_ENDPOINTS.issubset(endpoints),
                           sorted(UNIVERSAL_ENDPOINTS - endpoints))
        ok &= report.check("infra", "blockfell_endpoints_present", SLICE_ENDPOINTS.issubset(endpoints),
                           sorted(SLICE_ENDPOINTS - endpoints))
        ok &= report.check("infra", "ping", game.result("ping") is not None)

        reset = game.result("game.state.reset") or {}
        ok &= report.check("infra", "state_reset_serializes_schema_state", "shape_index" in reset, reset)

        state = game.result("game.reset_playtest") or {}
        ok &= report.check("accept", "runtime_is_blockfell_runes", state.get("runtime") == SLICE["runtime"], state)
        ok &= report.check("accept", "gate_starts_locked", state.get("gate_open") is False, state)
        ok &= report.check("accept", "combat_starts_with_four_enemies", state.get("enemies_alive") == 4, state)
        ok &= report.check("accept", "quest_starts_at_first_rune", state.get("objective_stage") == 0, state)

        tree = game.result("ui.tree") or {}
        ids = {node.get("id") for node in tree.get("nodes", [])}
        ok &= report.check("ui", "tree_exposes_blockfell_ids", SLICE["primary_ui_ids"].issubset(ids),
                           sorted(SLICE["primary_ui_ids"] - ids))

        patched = game.result("game.state.set", {"doc": "game", "path": "settings.master_volume", "value": 0.5})
        ok &= report.check("infra", "state_set_round_trips",
                           abs(patched.get("settings", {}).get("master_volume", 0.0) - 0.5) < 0.01, patched)

        first_rune = game.result("game.playtest.move_to_rune", {"index": 0}) or {}
        ok &= report.check("accept", "playtest_reaches_first_rune", first_rune.get("action_ready") is True, first_rune)
        first_rune = game.result("game.action.claim") or {}
        ok &= report.check("accept", "first_rune_claims_without_opening_gate",
                           first_rune.get("wallet", {}).get("soft") == 1
                           and first_rune.get("gate_open") is False
                           and first_rune.get("objective_stage") == 1,
                           first_rune)

        camp = game.result("game.playtest.move_to_camp") or {}
        ok &= report.check("accept", "playtest_reaches_combat_camp", camp.get("enemies_alive") == 4, camp)
        resolved = {}
        for _ in range(9):
            resolved = game.result("game.action.attack") or {}
            game.result("frame.wait", {"frames": 18})
        ok &= report.check("accept", "combat_clears_through_attacks",
                           resolved.get("combat_cleared") is True and resolved.get("wallet", {}).get("hard", 0) >= 20,
                           resolved)

        chest = game.result("game.playtest.move_to_chest") or {}
        ok &= report.check("accept", "playtest_reaches_chest", chest.get("near_chest") is True, chest)
        chest = game.result("game.action.claim") or {}
        ok &= report.check("accept", "chest_opens_after_combat",
                           chest.get("chest_open") is True and chest.get("wallet", {}).get("hard", 0) >= 45,
                           chest)

        game.result("game.playtest.move_to_rune", {"index": 1})
        second = game.result("game.action.claim") or {}
        ok &= report.check("accept", "combat_rune_claims_after_combat",
                           second.get("wallet", {}).get("soft") == 2, second)
        game.result("game.playtest.move_to_rune", {"index": 2})
        third = game.result("game.action.claim") or {}
        ok &= report.check("accept", "loot_rune_opens_gate",
                           third.get("gate_open") is True and third.get("wallet", {}).get("soft") == 3,
                           third)

        completed = game.result("game.playtest.complete_slice") or {}
        ok &= report.check("accept", "complete_slice_leaves_full_route_solved",
                           completed.get("gate_open") is True
                           and completed.get("combat_cleared") is True
                           and completed.get("chest_open") is True
                           and completed.get("objective_stage") in {5, 6},
                           completed)

        shot = game.capture_screenshot("build/captures/smoke.png", audit=False)
        ok &= report.check("visual", "screenshot_captured", bool(shot), shot)

        clicked = game.click_ui("action.claim", wait_frames=4)
        ok &= report.check("ui", "click_action_node_resolves",
                           clicked is not None and clicked.get("runtime") == SLICE["runtime"],
                           clicked)

    ok &= report.summary()
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
