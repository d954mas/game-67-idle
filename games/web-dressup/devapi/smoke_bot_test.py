import contextlib
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import smoke_bot  # noqa: E402


class FakeGame:
    """Small semantic model of the Runway Awakening UI, not a transport mock."""

    def __init__(self, *, unreliable_frame_wait=False):
        self.category = 1
        self.main = "none"
        self.accent = "none"
        self.phase = "dress"
        self.recipe_mask = 0
        self.lookbook_mask = 0
        self.rounds_completed = 0
        self.card_copy = "NEW MAGIC"
        self.lookbook_open = False
        self.lookbook_detail = None
        self.clicks = []
        self.waits = []
        self.time_steps = []
        self.player_gate_entries = 0
        self.unreliable_frame_wait = unreliable_frame_wait
        self.awakening_frames = 0
        self.events = []
        self.time_mode = "run"
        self.current_recipe = 3
        self.hair = "hair_bob"
        self.bottom = "bot_jeans"
        self.shoes = "shoe_sneak"
        self.saved_looks = {}

    def _emit(self, event_type):
        self.events.append({"seq": len(self.events), "type": event_type})

    def _advance(self, frames):
        if self.phase == "awakening":
            self.awakening_frames += frames
            if self.awakening_frames >= 12:
                self.phase = "card"
                recipe_bit = 1 << self.current_recipe
                signature = {"hair_bob": 0, "hair_long": 1, "hair_pink": 2}[self.hair]
                look_bit = 1 << (self.current_recipe * 3 + signature)
                new_recipe = (self.recipe_mask & recipe_bit) == 0
                exact = (self.hair, self.main, self.bottom, self.shoes, self.accent)
                prefix = smoke_bot.RECIPE_IDS[self.current_recipe]
                existing = next((key for key, look in self.saved_looks.items()
                                 if key.startswith(prefix + "/") and tuple(look.values()) == exact), None)
                new_look = existing is None
                self.recipe_mask |= recipe_bit
                self.lookbook_mask |= look_bit
                if new_look:
                    slot = sum(key.startswith(prefix + "/") for key in self.saved_looks)
                    self.saved_looks[f"{prefix}/{slot}"] = dict(zip(
                        ("hair_id", "main_id", "bottom_id", "shoes_id", "accent_id"), exact))
                    self.rounds_completed += 1
                self.card_copy = "NEW MAGIC" if new_recipe else "NEW REMIX" if new_look else "KNOWN LOOK"
                self._emit(f"awakening/recipe-{self.current_recipe}/{'discovered' if new_recipe else 'remixed' if new_look else 'replayed'}")
                self._emit(f"round/{self.rounds_completed}/complete")

    def result(self, method, params=None):
        params = params or {}
        if method == "endpoints":
            methods = set(smoke_bot.REQUIRED_METHODS) | {
                "time.step",
                "time.set_mode",
                "input.set_player_enabled",
            }
            return {"commands": [{"method": name} for name in sorted(methods)]}
        if method == "command.describe":
            return {"method": params["method"], "params_shape": "{}", "result_shape": "{}"}
        if method == "ui.tree":
            if self.lookbook_open:
                if self.lookbook_detail is not None:
                    prefix = smoke_bot.RECIPE_IDS[self.lookbook_detail]
                    count = sum(key.startswith(prefix + "/") for key in self.saved_looks)
                    ids = ["lookbook/back"]
                    ids.append(smoke_bot.LOOKBOOK_CREATE_ANOTHER_ID if count else smoke_bot.LOOKBOOK_CREATE_ID)
                    if count:
                        ids.append(smoke_bot.LOOKBOOK_WEAR_ID)
                    return {"nodes": [{"id_string": node_id, "label": (
                        "HAIR: Bob MAIN: Crescent BOTTOM: Celestial Tide SHOES: Lunar High-Tops ACCENT: Bloom Crown"
                        if node_id == smoke_bot.LOOKBOOK_WEAR_ID else "")}
                        for node_id in ids]}
                return {"nodes": [
                    *({"id_string": f"lookbook/recipe/{i}"} for i in range(6)),
                    {"id_string": smoke_bot.LOOKBOOK_CLOSE_ID},
                ]}
            if self.phase == "card":
                return {
                    "nodes": [
                        {"id_string": "awakening/card", "label": self.card_copy},
                        {"id_string": "awakening/restyle"},
                    ]
                }
            if self.phase == "awakening":
                ids = [smoke_bot.SKIP_REPLAY_ID] if self.recipe_mask else []
            else:
                ids = ["dress/item/top_tee", "dress/category/4", smoke_bot.HAIR_CATEGORY_ID,
                       smoke_bot.BOTTOM_CATEGORY_ID, smoke_bot.SHOES_CATEGORY_ID,
                       smoke_bot.AWAKEN_ID,
                       smoke_bot.LOOKBOOK_OPEN_ID]
                if self.category == 4:
                    ids.append("dress/item/acc_hat")
                if self.category == 0:
                    ids.extend(["dress/item/hair_bob", smoke_bot.HAIR_LONG_ID, smoke_bot.HAIR_PINK_ID])
                if self.category == 2:
                    ids.append("dress/item/bot_jeans")
                if self.category == 3:
                    ids.append("dress/item/shoe_sneak")
                if self.main == "top_tee" and self.accent == "acc_hat" and self.recipe_mask:
                    ids.append(smoke_bot.TRY_NEXT_ID)
            return {"nodes": [{"id_string": node_id} for node_id in ids]}
        if method == "game.state.schema":
            return {
                "game": {
                    "schema": "runway_awakening.state",
                    "fragment": "game",
                    "fields": ["recipe_mask", "lookbook_mask", "rounds_completed"],
                },
                "settings": {"schema": "game_seed.settings", "fragment": "settings", "fields": []},
            }
        if method == "game.state.get":
            return {
                "path": params.get("path", ""),
                "value": {
                    "settings": {"master_volume": 0.8},
                    "game": {
                        "outfit_main_id": self.main,
                        "outfit_accent_id": self.accent,
                        "outfit_hair_id": self.hair,
                        "outfit_bottom_id": self.bottom,
                        "outfit_shoes_id": self.shoes,
                        "recipe_mask": self.recipe_mask,
                        "lookbook_mask": self.lookbook_mask,
                        "rounds_completed": self.rounds_completed,
                        "saved_looks": self.saved_looks,
                        "first_equip_done": self.main != "none",
                    },
                },
            }
        if method == "game.events.tail":
            return {
                "events": list(self.events),
                "next_seq": len(self.events),
                "dropped": 0,
                "evicted": 0,
            }
        if method == "frame.current":
            return {"frame": sum(self.waits) + sum(self.time_steps)}
        if method == "time.step":
            if self.time_mode != "manual":
                raise AssertionError("time.step requires manual mode")
            frames = int(params.get("count", 1))
            self.time_steps.append(frames)
            self._advance(frames)
            return {"frames": frames}
        if method == "time.set_mode":
            self.time_mode = params["mode"]
            return {"mode": self.time_mode}
        if method == "input.set_player_enabled":
            return {"enabled": bool(params["enabled"])}
        raise AssertionError(f"unexpected method: {method}")

    def wait_frames(self, frames=1):
        if self.unreliable_frame_wait and self.phase == "awakening":
            raise TimeoutError("synthetic frame.wait timeout")
        self.waits.append(frames)
        self._advance(frames)
        return {"frame": sum(self.waits)}

    def click_ui(self, element_id, button="left", wait_frames=1, observe="frame.current"):
        semantic_id = element_id.removesuffix("/control")
        self.clicks.append((semantic_id, button, wait_frames, observe))
        if semantic_id == "dress/item/top_tee":
            self.main = "top_tee"
            self._emit("ftue/first-equip/complete")
        elif semantic_id == "dress/category/4":
            self.category = 4
        elif semantic_id == "dress/item/acc_hat":
            self.accent = "acc_hat"
            self.current_recipe = 3
            self.category = 0
        elif semantic_id == smoke_bot.HAIR_CATEGORY_ID:
            self.category = 0
        elif semantic_id == smoke_bot.HAIR_LONG_ID:
            self.hair = "hair_long"
            self.category = 2
        elif semantic_id == smoke_bot.HAIR_PINK_ID:
            self.hair = "hair_pink"
            self.category = 2
        elif semantic_id == "dress/item/hair_bob":
            self.hair = "hair_bob"
            self.category = 2
        elif semantic_id == "dress/item/bot_jeans":
            self.bottom = "bot_jeans"
            self.category = 3
        elif semantic_id == "dress/item/shoe_sneak":
            self.shoes = "shoe_sneak"
        elif semantic_id == smoke_bot.AWAKEN_ID:
            self.phase = "awakening"
            self.awakening_frames = 0
            self._emit(f"round/{self.rounds_completed + 1}/start")
        elif semantic_id == "awakening/restyle":
            self.phase = "dress"
            self.main = "none"
            self.accent = "none"
            self.category = 1
            self._emit(f"round/{self.rounds_completed + 1}/ready")
        elif semantic_id == smoke_bot.LOOKBOOK_OPEN_ID:
            self.lookbook_open = True
            self.lookbook_detail = None
        elif semantic_id == smoke_bot.LOOKBOOK_CLOSE_ID:
            self.lookbook_open = False
        elif semantic_id.startswith("lookbook/recipe/"):
            self.current_recipe = int(semantic_id.rsplit("/", 1)[1])
            self.lookbook_detail = self.current_recipe
        elif semantic_id in (smoke_bot.LOOKBOOK_CREATE_ID, smoke_bot.LOOKBOOK_CREATE_ANOTHER_ID):
            self.lookbook_open = False
            self.phase = "dress"
            self.main = "top_tee"
            self.accent = "acc_hat"
            self.category = 0
        elif semantic_id == smoke_bot.LOOKBOOK_WEAR_ID:
            prefix = smoke_bot.RECIPE_IDS[self.current_recipe]
            look = self.saved_looks[f"{prefix}/0"]
            self.hair, self.main, self.bottom, self.shoes, self.accent = look.values()
            self.lookbook_open = False
            self.phase = "dress"
        elif semantic_id == smoke_bot.SKIP_REPLAY_ID:
            self.phase = "card"
            self.recipe_mask |= smoke_bot.MOON_BLOOM_RECIPE_BIT
            self.lookbook_mask |= smoke_bot.MOON_BLOOM_CROWN_LOOK_BIT
            self.card_copy = "KNOWN LOOK"
            self._emit("awakening/moon-bloom/replayed")
        else:
            raise AssertionError(f"unexpected click id: {element_id}")
        return {"frame": len(self.clicks)}

    @contextlib.contextmanager
    def player_gated(self):
        self.player_gate_entries += 1
        yield self

    def capture_screenshot(self, output, wait_frames=1, audit=False):
        Path(output).parent.mkdir(parents=True, exist_ok=True)
        Path(output).write_bytes(b"\x89PNG\r\n\x1a\nfake")
        return output


class SmokeBotTest(unittest.TestCase):
    def test_extract_endpoint_methods_accepts_engine_shape(self):
        listing = {"commands": [{"method": "ui.tree"}, {"method": 7}, {"group": "bad"}, "legacy.ping"]}
        self.assertEqual(smoke_bot.extract_endpoint_methods(listing), {"ui.tree", "legacy.ping"})

    def test_missing_required_methods_reports_sorted_names(self):
        missing = smoke_bot.missing_required_methods({"endpoints", "ui.tree"})
        self.assertEqual(missing, sorted(set(smoke_bot.REQUIRED_METHODS) - {"endpoints", "ui.tree"}))

    def test_find_ui_node_uses_stable_id_string(self):
        tree = {"nodes": [{"id_string": "awakening/cta", "label": "AWAKEN"}]}
        self.assertEqual(smoke_bot.find_ui_node(tree, "awakening/cta"), tree["nodes"][0])

    def test_validate_game_state_requires_runway_progress_fields(self):
        state = {
            "path": "",
            "value": {
                "settings": {"master_volume": 0.8},
                "game": {"recipe_mask": 8, "lookbook_mask": 512, "rounds_completed": 1,
                         "saved_looks": {}},
            },
        }
        self.assertIs(smoke_bot.validate_game_state(state), state)
        with self.assertRaises(smoke_bot.DevApiError):
            smoke_bot.validate_game_state({"path": "", "value": {"game": {"recipe_mask": 8}}})

    def test_validate_game_state_schema_requires_runway_schema(self):
        schema = {
            "game": {
                "schema": "runway_awakening.state",
                "fragment": "game",
                "fields": ["recipe_mask", "lookbook_mask", "rounds_completed"],
            },
            "settings": {"fields": []},
        }
        self.assertIs(smoke_bot.validate_game_state_schema(schema), schema)
        with self.assertRaises(smoke_bot.DevApiError):
            smoke_bot.validate_game_state_schema({"game": {"schema": "game_seed.state", "fields": []}})

    def test_validate_events_tail_rejects_loss_and_bad_events(self):
        ok = {"events": [{"seq": 0, "type": "round/1/start"}], "next_seq": 1, "dropped": 0, "evicted": 0}
        self.assertIs(smoke_bot.validate_events_tail(ok), ok)
        with self.assertRaises(smoke_bot.DevApiError):
            smoke_bot.validate_events_tail({**ok, "dropped": 1})
        with self.assertRaises(smoke_bot.DevApiError):
            smoke_bot.validate_events_tail({**ok, "events": [{"seq": 0}]})

    def test_default_executable_uses_ai_studio_game_exe_override(self):
        old = os.environ.get("AI_STUDIO_GAME_EXE")
        os.environ["AI_STUDIO_GAME_EXE"] = "C:/tmp/game.exe"
        try:
            self.assertEqual(smoke_bot.default_executable(), Path("C:/tmp/game.exe"))
        finally:
            if old is None:
                os.environ.pop("AI_STUDIO_GAME_EXE", None)
            else:
                os.environ["AI_STUDIO_GAME_EXE"] = old

    def test_run_smoke_completes_moon_bloom_round_and_starts_restyle(self):
        with tempfile.TemporaryDirectory() as tmp:
            game = FakeGame(unreliable_frame_wait=True)
            summary = smoke_bot.run_smoke(game, Path(tmp), audit=False)

            clicked_ids = [click[0] for click in game.clicks]
            self.assertEqual(clicked_ids[:7], [
                "dress/item/top_tee", "dress/category/4", "dress/item/acc_hat",
                "dress/item/hair_bob", "dress/item/bot_jeans", "dress/item/shoe_sneak",
                smoke_bot.AWAKEN_ID,
            ])
            self.assertIn(smoke_bot.SKIP_REPLAY_ID, clicked_ids)
            for recipe_index in (0, 1, 2, 4, 5):
                self.assertIn(f"lookbook/recipe/{recipe_index}", clicked_ids)
            self.assertIn(smoke_bot.HAIR_LONG_ID, clicked_ids)
            self.assertIn(smoke_bot.HAIR_PINK_ID, clicked_ids)
            self.assertGreater(len(game.time_steps), 0, "time.step must avoid unreliable frame.wait")
            self.assertEqual(game.time_mode, "run")
            self.assertEqual(game.player_gate_entries, len(game.clicks))
            self.assertEqual(summary["schema"], "runway_awakening.devapi_smoke.v2")
            self.assertEqual(summary["recipe"], "moon-bloom")
            self.assertEqual(summary["rounds_completed"], 1)
            self.assertTrue(Path(summary["recipe_card_screenshot"]).exists())
            self.assertEqual(summary["second_round_ui"], smoke_bot.AWAKEN_ID)
            self.assertIn("awakening/moon-bloom/replayed", summary["event_types"])
            self.assertEqual(summary["state_after_repeat"]["value"]["game"]["rounds_completed"], 1)
            self.assertEqual(summary["repeat_copy"], "KNOWN LOOK")
            self.assertEqual(summary["known_preflight_action"], smoke_bot.TRY_NEXT_ID)
            self.assertGreaterEqual(summary["lookbook_nodes"], 7)
            self.assertTrue(Path(summary["lookbook_screenshot"]).exists())
            self.assertTrue(Path(summary["repeat_card_screenshot"]).exists())
            self.assertEqual(summary["collected_recipe_order"], [3, 0, 1, 2, 4, 5])
            self.assertEqual(summary["completion_recipe_count"], 6)
            self.assertEqual(summary["completion_saved_look_count"], 8)
            self.assertGreater(summary["lookbook_detail_nodes"], 0)
            self.assertEqual(summary["completion_rounds"], 8)
            self.assertTrue(Path(summary["mastery_screenshot"]).exists())
            self.assertTrue(Path(summary["summary"]).exists())
            self.assertTrue(Path(summary["screenshot"]).exists())


if __name__ == "__main__":
    unittest.main()
