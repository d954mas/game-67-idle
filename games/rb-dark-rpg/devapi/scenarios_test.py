import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parent))
import scenarios  # noqa: E402


class FakeGame:
    def __init__(self):
        self.clicks = []
        self.gestures = []
        self.patches = []
        self.waits = []
        self.current_location = "hub_last_post"
        self.place_open = False
        self.map_open = False
        self.place_tab = "environment"
        self.completed_step_ids = []
        self.quest_states = {}
        self.hero_xp = 0
        self.flags_ids = []
        self.claimed_reward_ids = []
        self.combat_prefight_open = False
        self.combat_running_open = False
        self.combat_result_open = False
        self.auto_open_dev_place = False

    def _current_nodes(self):
        nav_names = ["equipment", "journal", "map", "place", "more"]
        nodes = [
            {"id": 1, "id_string": "bottom_nav/root", "bounds": {"x": 0, "y": 470, "w": 960, "h": 70}},
        ]
        for slot, name in enumerate(nav_names):
            nodes.append(
                {
                    "id": 10 + slot,
                    "parent": 1,
                    "id_string": f"bottom_nav/slot/{name}",
                    "role": "bottom_nav",
                    "bounds": {"x": 220 + slot * 100, "y": 480, "w": 80, "h": 48},
                }
            )
            nodes.append(
                {
                    "id": 20 + slot,
                    "parent": 10 + slot,
                    "id_string": f"ui/nav_v11_{name}",
                    "bounds": {"x": 225 + slot * 100, "y": 485, "w": 70, "h": 38},
                }
            )
        if self.map_open:
            nodes.append(
                {
                    "id": 29,
                    "id_string": "world_map/atlas_canvas",
                    "bounds": {"x": 180, "y": 80, "w": 600, "h": 320},
                }
            )
            nodes.append(
                {
                    "id": 28,
                    "parent": 29,
                    "id_string": "world_map/art",
                    "bounds": {"x": 180, "y": 80, "w": 600, "h": 320},
                }
            )
            nodes.append(
                {
                    "id": 30,
                    "id_string": "world_map/location/hub_last_post",
                    "bounds": {"x": 320, "y": 250, "w": 38, "h": 38},
                }
            )
            nodes.append(
                {
                    "id": 35,
                    "id_string": "world_map/location/hub_gate_outskirts",
                    "bounds": {"x": 500, "y": 240, "w": 44, "h": 44},
                }
            )
            nodes.append(
                {
                    "id": 36,
                    "id_string": "world_map/hero/ring",
                    "bounds": {"x": 314, "y": 286, "w": 48, "h": 15},
                }
            )
        if self.place_open:
            nodes.append(
                {
                    "id": 37,
                    "id_string": "world_place/tab/enemies",
                    "bounds": {"x": 300, "y": 120, "w": 130, "h": 40},
                }
            )
            nodes.append(
                {
                    "id": 38,
                    "id_string": "world_place/tab/environment",
                    "bounds": {"x": 440, "y": 120, "w": 180, "h": 40},
                }
            )
            nodes.append(
                {
                    "id": 39,
                    "id_string": "world_place/tabs",
                    "bounds": {"x": 300, "y": 120, "w": 320, "h": 40},
                }
            )
            if self.current_location == "old_mill" and self.place_tab == "environment":
                nodes.append(
                    {
                        "id": 32,
                        "id_string": "world_place/object/old_mill.black_sun_mark",
                        "bounds": {"x": 300, "y": 170, "w": 280, "h": 52},
                    }
                )
                q002 = self.quest_states.get("q002_bread_for_post")
                if isinstance(q002, dict) and q002.get("current_step_id") == "report_to_elder":
                    nodes.append(
                        {
                            "id": 47,
                            "id_string": "world_place/object/old_mill.main_yard",
                            "bounds": {"x": 300, "y": 230, "w": 280, "h": 52},
                        }
                    )
            elif self.current_location == "hub_last_post" and self.place_tab == "enemies":
                q001 = self.quest_states.get("q001_gate_pass")
                if isinstance(q001, dict) and q001.get("current_step_id") == "clear_gate_scavenger":
                    nodes.append(
                        {
                            "id": 31,
                            "id_string": "world_place/object/hub_last_post.caged_scavenger",
                            "bounds": {"x": 300, "y": 170, "w": 280, "h": 52},
                        }
                    )
            elif self.current_location == "hub_last_post":
                nodes.append(
                    {
                        "id": 33,
                        "id_string": "world_place/object/hub_last_post.gate_guard",
                        "bounds": {"x": 300, "y": 170, "w": 280, "h": 52},
                    }
                )
                nodes.append(
                    {
                        "id": 41,
                        "id_string": "world_place/object/hub_last_post.healer",
                        "bounds": {"x": 300, "y": 230, "w": 280, "h": 52},
                    }
                )
        nodes.append(
            {
                "id": 34,
                "id_string": "dialogue/primary_choice_inline",
                "role": "nt_button",
                "bounds": {"x": 340, "y": 390, "w": 220, "h": 52},
            }
        )
        if self.combat_prefight_open:
            nodes.append(
                {
                    "id": 40,
                    "id_string": "combat/prefight",
                    "bounds": {"x": 250, "y": 100, "w": 460, "h": 320},
                }
            )
            nodes.append(
                {
                    "id": 42,
                    "id_string": "combat/prefight_start",
                    "role": "nt_button",
                    "bounds": {"x": 305, "y": 104, "w": 180, "h": 42},
                }
            )
        if self.combat_running_open:
            nodes.append(
                {
                    "id": 45,
                    "id_string": "combat/running",
                    "bounds": {"x": 240, "y": 70, "w": 480, "h": 416},
                }
            )
            nodes.append(
                {
                    "id": 46,
                    "id_string": "combat/stage",
                    "bounds": {"x": 260, "y": 170, "w": 440, "h": 154},
                }
            )
        if self.combat_result_open:
            nodes.append(
                {
                    "id": 43,
                    "id_string": "combat/result",
                    "bounds": {"x": 250, "y": 130, "w": 460, "h": 250},
                }
            )
            nodes.append(
                {
                    "id": 44,
                    "id_string": "combat/result_close",
                    "bounds": {"x": 370, "y": 150, "w": 220, "h": 42},
                }
            )
        return nodes

    def _node_at_point(self, x, y):
        hits = []
        for node in reversed(self._current_nodes()):
            bounds = node.get("bounds")
            if not isinstance(bounds, dict):
                continue
            left = float(bounds["x"])
            top = float(bounds["y"])
            right = left + float(bounds["w"])
            bottom = top + float(bounds["h"])
            if left <= x <= right and top <= y <= bottom:
                hits.append(node)
        for node in hits:
            node_id = node.get("id_string")
            if node.get("role") in ("bottom_nav", "nt_button") or (
                isinstance(node_id, str)
                and (node_id.startswith("world_map/") or node_id.startswith("world_place/"))
            ):
                return node_id
        return hits[0].get("id_string") if hits else None

    def _apply_click(self, click_id):
        if click_id == "bottom_nav/slot/place":
            self.place_open = True
            self.map_open = False
            q001 = self.quest_states.get("q001_gate_pass")
            if self.current_location == "hub_last_post" and isinstance(q001, dict) and q001.get("current_step_id") == "clear_gate_scavenger":
                self.place_tab = "enemies"
            else:
                self.place_tab = "environment"
        elif click_id == "bottom_nav/slot/map":
            self.map_open = True
            self.place_open = False
        elif click_id == "world_place/tab/enemies":
            self.place_tab = "enemies"
        elif click_id == "world_place/tab/environment":
            self.place_tab = "environment"
        elif click_id == "world_map/location/hub_gate_outskirts":
            self.current_location = "hub_gate_outskirts"
            self.map_open = False
            self.place_open = True
            self.place_tab = "environment"
        elif click_id == "world_map/location/old_mill":
            self.current_location = "old_mill"
            self.map_open = False
            self.place_open = True
            self.place_tab = "environment"
        elif click_id == "world_map/location/hub_last_post":
            self.current_location = "hub_last_post"
            self.map_open = False
            self.place_open = True
            self.place_tab = "environment"
        elif click_id == "world_place/object/old_mill.black_sun_mark":
            self.completed_step_ids = ["visit_old_mill", "inspect_old_mill"]
            self.quest_states = {
                "q002_bread_for_post": {
                    "status": "active",
                    "current_step_id": "report_to_elder",
                    "objective_progress": 0,
                    "last_update_reason": "inspect.old_mill.black_sun_mark",
                }
            }
        elif click_id == "world_place/object/hub_last_post.caged_scavenger":
            self.combat_prefight_open = True
            self.combat_running_open = False
            self.combat_result_open = False
        elif click_id == "world_place/object/old_mill.main_yard":
            self.combat_prefight_open = True
            self.combat_running_open = False
            self.combat_result_open = False
        elif click_id == "combat/prefight_start":
            self.combat_prefight_open = False
            self.combat_running_open = True
            self.combat_result_open = False
        elif click_id == "combat/result_close":
            self.combat_prefight_open = False
            self.combat_running_open = False
            self.combat_result_open = False
        elif click_id == "dialogue/primary_choice_inline":
            q001 = self.quest_states.get("q001_gate_pass")
            if isinstance(q001, dict) and q001.get("status") == "ready_to_turn_in":
                self.quest_states = {
                    "q001_gate_pass": {
                        "status": "completed",
                        "objective_progress": 0,
                        "last_update_reason": "dlg_gate_guard_turn_in.take_token",
                    }
                }
                self.hero_xp = 12
                self.claimed_reward_ids = ["dlg_gate_guard_turn_in.take_token.completion"]
                self.flags_ids = [
                    "gate_guard_intro_seen",
                    "starter_gear_received",
                    "old_sword_equipped",
                    "padded_jacket_equipped",
                    "leather_greaves_equipped",
                    "gate_scavenger_defeated",
                    "seeker_token_owned",
                    "map_gate_unlocked",
                    "old_mill_unlocked",
                ]

    def result(self, method, params=None):
        if method == "game.state.patch":
            self.patches.append(params)
            values = params.get("values", {}) if isinstance(params, dict) else {}
            self.current_location = values.get("world.current_location_id", self.current_location)
            self.completed_step_ids = values.get("quests.completed_step_ids", self.completed_step_ids)
            self.quest_states = values.get("quests.quest_states", self.quest_states)
            self.hero_xp = values.get("hero.xp", self.hero_xp)
            self.flags_ids = values.get("flags.ids", self.flags_ids)
            self.claimed_reward_ids = values.get("quests.claimed_reward_ids", self.claimed_reward_ids)
            if self.auto_open_dev_place and "dev_world_place_open" in self.flags_ids:
                self.place_open = True
                self.map_open = False
                q001 = self.quest_states.get("q001_gate_pass")
                if (
                    self.current_location == "hub_last_post"
                    and isinstance(q001, dict)
                    and q001.get("current_step_id") == "clear_gate_scavenger"
                ):
                    self.place_tab = "enemies"
            return {"ok": True}
        if method == "game.state.get":
            path = params.get("path", "") if params else ""
            if path == "hero.xp":
                return {"path": path, "value": self.hero_xp}
            if path == "flags.ids":
                return {"path": path, "value": self.flags_ids}
            if path == "quests.claimed_reward_ids":
                return {"path": path, "value": self.claimed_reward_ids}
            if path == "quests.completed_step_ids":
                return {"path": path, "value": self.completed_step_ids}
            if path == "quests.quest_states":
                return {"path": path, "value": self.quest_states}
            return {"path": path, "value": self.current_location}
        if method in ("input.gesture", "input.click"):
            self.gestures.append(params)
            if method == "input.click" and isinstance(params, dict):
                click_id = self._node_at_point(float(params["x"]), float(params["y"]))
                self._apply_click(click_id)
            else:
                points = params.get("points") if isinstance(params, dict) else None
                if isinstance(points, list) and points and isinstance(points[-1], list) and len(points[-1]) >= 2:
                    click_id = self._node_at_point(float(points[-1][0]), float(points[-1][1]))
                    self._apply_click(click_id)
            return {"ok": True}
        if method == "ui.click":
            self.gestures.append(params)
            click_id = params.get("id") if isinstance(params, dict) else None
            self._apply_click(click_id)
            return {"ok": True}
        if method == "ui.tree":
            return {
                "viewport": {"x": 0, "y": 0, "w": 960, "h": 540},
                "nodes": self._current_nodes(),
            }
        raise AssertionError(f"unexpected method: {method}")

    def wait_frames(self, frames=1):
        self.waits.append(frames)
        return {"frame": len(self.waits)}

    def click_ui(self, element_id, button="left", wait_frames=1, observe="frame.current"):
        self.clicks.append((element_id, button, wait_frames, observe))
        if element_id == "bottom_nav/slot/place":
            self.place_open = True
            self.map_open = False
            q001 = self.quest_states.get("q001_gate_pass")
            if self.current_location == "hub_last_post" and isinstance(q001, dict) and q001.get("current_step_id") == "clear_gate_scavenger":
                self.place_tab = "enemies"
            else:
                self.place_tab = "environment"
        elif element_id == "bottom_nav/slot/map":
            self.map_open = True
            self.place_open = False
        if element_id == "world_map/location/hub_gate_outskirts":
            self.current_location = "hub_gate_outskirts"
            self.map_open = False
            self.place_open = True
            self.place_tab = "environment"
        return {"ok": True}


class WorldMapScenarioTest(unittest.TestCase):
    def test_prepare_world_map_open_waits_for_atlas_and_hero_marker(self):
        game = FakeGame()
        viewport = SimpleNamespace(window_size="960x540")

        result = scenarios.prepare_world_map_open(game, viewport)

        self.assertEqual(result["state"], "world_map_open")
        self.assertEqual(result["location"], "hub_last_post")
        self.assertTrue(game.patches)
        self.assertTrue(any(click.get("id") == "bottom_nav/slot/map" for click in game.gestures))
        self.assertTrue(game.map_open)
        self.assertEqual(game.current_location, "hub_last_post")

    def test_prepare_location_screen_opens_standalone_place_screen(self):
        game = FakeGame()
        game.auto_open_dev_place = True
        viewport = SimpleNamespace(window_size="390x844")

        result = scenarios.prepare_location_screen(game, viewport)

        self.assertEqual(result["state"], "location_screen")
        self.assertEqual(result["location"], "hub_last_post")
        self.assertTrue(game.place_open)
        self.assertFalse(game.map_open)
        self.assertEqual(game.place_tab, "enemies")
        self.assertFalse(any(click.get("id") == "bottom_nav/slot/map" for click in game.gestures))

    def test_prepare_location_points_screen_switches_to_environment_tab(self):
        game = FakeGame()
        game.auto_open_dev_place = True
        viewport = SimpleNamespace(window_size="390x844")

        result = scenarios.prepare_location_points_screen(game, viewport)

        self.assertEqual(result["state"], "location_points_screen")
        self.assertTrue(game.place_open)
        self.assertEqual(game.place_tab, "environment")
        self.assertTrue(any(click.get("id") == "world_place/tab/environment" for click in game.gestures))

    def test_prepare_world_map_move_gate_clicks_semantic_location_and_verifies_state(self):
        game = FakeGame()
        viewport = SimpleNamespace(window_size="960x540")

        result = scenarios.prepare_world_map_move_gate(game, viewport)

        self.assertEqual(result["state"], "world_map_move_gate")
        self.assertEqual(result["location"], "hub_gate_outskirts")
        self.assertTrue(game.patches)
        self.assertTrue(any(click.get("id") == "bottom_nav/slot/map" for click in game.gestures))
        self.assertTrue(any(click.get("id") == "world_map/location/hub_gate_outskirts" for click in game.gestures))
        self.assertEqual(game.current_location, "hub_gate_outskirts")

    def test_prepare_old_mill_inspect_mark_taps_semantic_object_and_advances_quest(self):
        game = FakeGame()
        viewport = SimpleNamespace(window_size="960x540")

        result = scenarios.prepare_old_mill_inspect_mark(game, viewport)

        self.assertEqual(result["state"], "old_mill_inspect_mark")
        self.assertEqual(result["quest"], "q002_bread_for_post")
        self.assertEqual(game.current_location, "old_mill")
        self.assertTrue(any(click.get("id") == "bottom_nav/slot/place" for click in game.gestures))
        self.assertTrue(any(click.get("id") == "world_place/object/old_mill.black_sun_mark" for click in game.gestures))
        self.assertEqual(game.completed_step_ids, ["visit_old_mill", "inspect_old_mill"])
        self.assertEqual(game.quest_states["q002_bread_for_post"]["status"], "active")
        self.assertEqual(game.quest_states["q002_bread_for_post"]["current_step_id"], "report_to_elder")

    def test_prepare_gate_guard_turn_in_from_place_completes_q001(self):
        game = FakeGame()
        viewport = SimpleNamespace(window_size="960x540")

        result = scenarios.prepare_gate_guard_turn_in_from_place(game, viewport)

        self.assertEqual(result["state"], "gate_guard_turn_in_from_place")
        self.assertEqual(result["quest"], "q001_gate_pass")
        self.assertEqual(game.current_location, "hub_last_post")
        self.assertTrue(any(click.get("id") == "bottom_nav/slot/place" for click in game.gestures))
        self.assertTrue(any(click.get("id") == "world_place/object/hub_last_post.gate_guard" for click in game.gestures))
        self.assertTrue(any(click.get("id") == "dialogue/primary_choice_inline" for click in game.gestures))
        self.assertEqual(game.quest_states["q001_gate_pass"]["status"], "completed")
        self.assertEqual(game.hero_xp, 12)
        self.assertIn("seeker_token_owned", game.flags_ids)
        self.assertEqual(game.claimed_reward_ids, ["dlg_gate_guard_turn_in.take_token.completion"])

    def test_prepare_combat_prefight_prefers_clickable_bottom_nav_slot(self):
        game = FakeGame()
        viewport = SimpleNamespace(window_size="960x540")

        result = scenarios.prepare_combat_prefight(game, viewport)

        self.assertEqual(result["state"], "combat_prefight")
        self.assertTrue(game.place_open)
        self.assertEqual(game.clicks, [])

    def test_prepare_combat_running_waits_on_stage(self):
        game = FakeGame()
        viewport = SimpleNamespace(window_size="960x540")

        result = scenarios.prepare_combat_running(game, viewport)

        self.assertEqual(result["state"], "combat_running")
        self.assertTrue(game.combat_running_open)
        self.assertTrue(any(click.get("x") == 395.0 and click.get("y") == 125.0 for click in game.gestures))
        self.assertIn(120, game.waits)

    def test_prepare_mill_combat_running_waits_on_stage(self):
        game = FakeGame()
        viewport = SimpleNamespace(window_size="960x540")

        result = scenarios.prepare_mill_combat_running(game, viewport)

        self.assertEqual(result["state"], "mill_combat_running")
        self.assertEqual(game.current_location, "old_mill")
        self.assertTrue(game.combat_running_open)
        self.assertTrue(any(click.get("x") == 440.0 and click.get("y") == 256.0 for click in game.gestures))
        self.assertIn(120, game.waits)


if __name__ == "__main__":
    unittest.main()
