#!/usr/bin/env python3
"""Ember Road DevAPI smoke test.

The smoke proves the first RPG slice: accept a town quest, travel on the map,
resolve an auto battle, equip loot, and claim the quest reward.
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


def as_dict(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def field(state: dict[str, Any], group: str, name: str, fallback: Any = None) -> Any:
    nested = state.get(group)
    if isinstance(nested, dict) and name in nested:
        return nested.get(name)
    return state.get(f"{group}_{name}", fallback)


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
                "game.action.primary",
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
                "game.action.forge_mine_lantern",
                "ui.tree",
                "ui.click",
            }
            missing = sorted(expected_methods - methods)
            report.check("devapi", "expected endpoints", not missing, f"missing={missing}" if missing else "")

            state = as_dict(game.result("game.reset_playtest"))
            report.check("state", "runtime is Ember Road", state.get("runtime") == "ember_road", str(state.get("runtime")))
            report.check("state", "starts in Old Gate", state.get("location") == "old_gate", str(state.get("location")))
            report.check("state", "starts with quest available", state.get("primary_action_id") == "ember.accept_quest", str(state.get("primary_action_id")))

            ui_tree = game.result("ui.tree")
            ui_ids = collect_ui_ids(ui_tree)
            report.check("ui", "primary action exists", "ember.primary" in ui_ids, f"ids={sorted(ui_ids)}")
            report.check("ui", "map node exists", "ember.map.north_road" in ui_ids, f"ids={sorted(ui_ids)}")

            accepted = as_dict(game.result("game.action.accept_quest"))
            report.check("quest", "quest accepted", accepted.get("quest_stage") == "accepted", str(accepted.get("quest_stage")))
            report.check("quest", "next action is travel", accepted.get("primary_action_id") == "ember.travel_north_road", str(accepted.get("primary_action_id")))

            travelled = as_dict(game.result("game.action.travel_north_road"))
            report.check("map", "travel reaches North Road", travelled.get("location") == "north_road", str(travelled.get("location")))
            report.check("battle", "encounter is ready", travelled.get("battle_state") == "ready", str(travelled.get("battle_state")))

            battle = as_dict(game.result("game.action.auto_battle"))
            report.check("battle", "auto battle wins", battle.get("battle_state") in {"victory", "low_health"}, str(battle.get("battle_state")))
            report.check("battle", "wolf objective complete", battle.get("quest_stage") == "wolf_defeated", str(battle.get("quest_stage")))
            report.check("rewards", "loot item is ready", field(battle, "reward", "item_ready") is True, str(field(battle, "reward", "item_ready")))

            equipped = as_dict(game.result("game.action.equip_ring"))
            report.check("items", "ring equipped", field(equipped, "gear", "ring_equipped") is True, str(field(equipped, "gear", "ring_equipped")))
            report.check("items", "attack increased", int(field(equipped, "hero", "attack", 0)) >= 6, str(field(equipped, "hero", "attack")))

            completed = as_dict(game.result("game.action.claim_reward"))
            report.check("quest", "quest completed", completed.get("quest_stage") == "completed", str(completed.get("quest_stage")))
            report.check("progression", "level two reached", int(field(completed, "hero", "level", 0)) >= 2, str(field(completed, "hero", "level")))
            report.check("economy", "gold awarded", int(field(completed, "hero", "gold", 0)) >= 24, str(field(completed, "hero", "gold")))
            report.check("progression", "equip-before-claim attack stable", int(field(completed, "hero", "attack", 0)) == 7, str(field(completed, "hero", "attack")))
            report.check("map", "Old Mine primary opens after completion", completed.get("primary_action_id") == "ember.enter_old_mine", str(completed.get("primary_action_id")))

            mine = as_dict(game.result("game.action.enter_old_mine"))
            report.check("map", "Old Mine route reached", mine.get("location") == "old_mine", str(mine.get("location")))
            report.check("choice", "Old Mine choice state open", mine.get("modal_or_choice_open") is True, str(mine.get("modal_or_choice_open")))
            report.check("choice", "Old Mine primary scouts", mine.get("primary_action_id") == "ember.scout_old_mine", str(mine.get("primary_action_id")))
            mine_tree = game.result("ui.tree")
            mine_ids = collect_ui_ids(mine_tree)
            report.check("ui", "Old Mine modal exists", "ember.mine.choice" in mine_ids, f"ids={sorted(mine_ids)}")
            report.check("ui", "Old Mine scout choice exists", "ember.mine.scout" in mine_ids, f"ids={sorted(mine_ids)}")
            report.check("ui", "Old Mine back choice exists", "ember.mine.back" in mine_ids, f"ids={sorted(mine_ids)}")
            scout = as_dict(game.result("game.action.scout_old_mine"))
            report.check("choice", "Old Mine scout result open", scout.get("old_mine_scout_result_open") is True, str(scout.get("old_mine_scout_result_open")))
            report.check("choice", "Old Mine scouted", field(scout, "old_mine", "scouted") is True, str(field(scout, "old_mine", "scouted")))
            report.check("choice", "Old Mine depth mapped", int(field(scout, "old_mine", "depth", 0)) == 1, str(field(scout, "old_mine", "depth")))
            report.check("choice", "Old Mine shards found", int(field(scout, "old_mine", "ember_shards", 0)) >= 3, str(field(scout, "old_mine", "ember_shards")))
            report.check("choice", "Old Mine primary clears depth after scout", scout.get("primary_action_id") == "ember.resolve_old_mine_depth", str(scout.get("primary_action_id")))
            scout_tree = game.result("ui.tree")
            scout_ids = collect_ui_ids(scout_tree)
            report.check("ui", "Old Mine scout result node exists", "ember.mine.scout_result" in scout_ids, f"ids={sorted(scout_ids)}")
            depth = as_dict(game.result("game.action.resolve_old_mine_depth"))
            report.check("choice", "Old Mine depth encounter open", depth.get("old_mine_depth_encounter_open") is True, str(depth.get("old_mine_depth_encounter_open")))
            report.check("choice", "Old Mine next delve choice open", depth.get("old_mine_next_delve_choice_open") is True, str(depth.get("old_mine_next_delve_choice_open")))
            report.check("choice", "Old Mine depth resolved", field(depth, "old_mine", "depth_resolved") is True, str(field(depth, "old_mine", "depth_resolved")))
            report.check("choice", "Cave Bat defeated", field(depth, "old_mine", "bat_defeated") is True, str(field(depth, "old_mine", "bat_defeated")))
            report.check("choice", "Cave Bat damage recorded", int(field(depth, "old_mine", "bat_damage", 0)) == 3, str(field(depth, "old_mine", "bat_damage")))
            report.check("choice", "Depth gold recorded", int(field(depth, "old_mine", "depth_gold", 0)) == 4, str(field(depth, "old_mine", "depth_gold")))
            report.check("choice", "Depth shards added", int(field(depth, "old_mine", "ember_shards", 0)) >= 5, str(field(depth, "old_mine", "ember_shards")))
            report.check("choice", "Old Mine primary delves cache", depth.get("primary_action_id") == "ember.delve_old_mine", str(depth.get("primary_action_id")))
            depth_tree = game.result("ui.tree")
            depth_ids = collect_ui_ids(depth_tree)
            report.check("ui", "Old Mine depth encounter node exists", "ember.mine.depth_encounter" in depth_ids, f"ids={sorted(depth_ids)}")
            report.check("ui", "Old Mine next delve node exists", "ember.mine.next_delve" in depth_ids, f"ids={sorted(depth_ids)}")
            delve = as_dict(game.result("game.action.delve_old_mine"))
            report.check("choice", "Old Mine delve reward open", delve.get("old_mine_delve_reward_open") is True, str(delve.get("old_mine_delve_reward_open")))
            report.check("choice", "Old Mine cache claimed", field(delve, "old_mine", "cache_claimed") is True, str(field(delve, "old_mine", "cache_claimed")))
            report.check("choice", "Old Mine delve count recorded", int(field(delve, "old_mine", "delve_count", 0)) == 1, str(field(delve, "old_mine", "delve_count")))
            report.check("choice", "Old Mine last delve shards", int(field(delve, "old_mine", "last_delve_shards", 0)) == 1, str(field(delve, "old_mine", "last_delve_shards")))
            report.check("choice", "Old Mine primary returns after cache", delve.get("primary_action_id") == "ember.return_old_gate", str(delve.get("primary_action_id")))
            returned = as_dict(game.result("game.action.return_old_gate"))
            report.check("map", "return reaches Old Gate", returned.get("location") == "old_gate", str(returned.get("location")))
            report.check("progression", "town lantern upgrade open", returned.get("town_lantern_upgrade_open") is True, str(returned.get("town_lantern_upgrade_open")))
            report.check("progression", "town primary forges lantern", returned.get("primary_action_id") == "ember.forge_mine_lantern", str(returned.get("primary_action_id")))
            town_tree = game.result("ui.tree")
            town_ids = collect_ui_ids(town_tree)
            report.check("ui", "town lantern upgrade node exists", "ember.town.lantern_upgrade" in town_ids, f"ids={sorted(town_ids)}")
            report.check("ui", "town forge workbench scene node exists", "ember.scene.forge_workbench" in town_ids, f"ids={sorted(town_ids)}")
            lantern = as_dict(game.result("game.action.forge_mine_lantern"))
            report.check("progression", "mine lantern forged", field(lantern, "gear", "mine_lantern") is True, str(field(lantern, "gear", "mine_lantern")))
            report.check("progression", "depth 2 unlocked", lantern.get("old_mine_depth2_unlocked") is True or field(lantern, "old_mine", "depth2_unlocked") is True, str(lantern.get("old_mine_depth2_unlocked")))
            report.check("progression", "shards spent on lantern", int(field(lantern, "old_mine", "ember_shards", 99)) == 0, str(field(lantern, "old_mine", "ember_shards")))
            report.check("progression", "town lantern forged state opens", lantern.get("town_lantern_forged_open") is True, str(lantern.get("town_lantern_forged_open")))
            report.check("progression", "primary returns to mine after lantern", lantern.get("primary_action_id") == "ember.enter_old_mine", str(lantern.get("primary_action_id")))

            game.result("game.reset_playtest")
            game.result("game.action.accept_quest")
            game.result("game.action.travel_north_road")
            game.result("game.action.auto_battle")
            claim_first = as_dict(game.result("game.action.claim_reward"))
            equip_after_claim = as_dict(game.result("game.action.equip_ring"))
            report.check("progression", "claim-before-equip attack stable", int(field(equip_after_claim, "hero", "attack", 0)) == 7, f"claim={field(claim_first, 'hero', 'attack')} equip={field(equip_after_claim, 'hero', 'attack')}")
    except DevApiError as exc:
        report.check("devapi", "smoke run", False, str(exc))

    return report.exit_code()


def main(argv: list[str]) -> int:
    port = int(argv[1]) if len(argv) > 1 else 9123
    return run(port)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
