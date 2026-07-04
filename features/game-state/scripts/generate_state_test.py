import json
import tempfile
import unittest
from pathlib import Path

import generate_state


ROOT = Path(__file__).resolve().parents[3]
CLOSED_TOKENS = ("rune_", "fishing_", "Rune", "Fishing", "rune.", "fishing.")


def generate_schema(schema_path: Path, out_dir: Path) -> str:
    rc = generate_state.main(
        [
            "--schema",
            str(schema_path),
            "--out-dir",
            str(out_dir),
        ]
    )
    if rc != 0:
        raise AssertionError(f"generator returned {rc}")
    generated = []
    for name in (
        "game_state.h",
        "game_state.c",
        "game_state_devapi.c",
        "game_state_schema.gen.h",
    ):
        path = out_dir / name
        if not path.exists():
            raise AssertionError(f"missing generated file: {path}")
        generated.append(path.read_text(encoding="utf-8"))
    return "\n".join(generated)


def generate_variant(schema_name: str, out_dir: Path) -> str:
    return generate_schema(ROOT / "templates" / "template" / "state" / schema_name, out_dir)


class StateCodegenTests(unittest.TestCase):
    def test_default_template_out_dir_uses_build_generated_folder(self):
        schema = ROOT / "templates" / "template" / "state" / "game_state.schema.json"
        expected = ROOT / "templates" / "template" / "build" / "generated" / "game-state"
        self.assertEqual(generate_state.default_out_dir(schema), expected)

    def test_clean_schema_excludes_closed_prototype_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            text = generate_variant("game_state.schema.json", Path(tmp) / "clean")

        for token in CLOSED_TOKENS:
            self.assertNotIn(token, text)

    def test_clean_schema_generates_all_outputs(self):
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp) / "generated"
            # generate_variant already asserts every expected file exists.
            generate_variant("game_state.schema.json", out_dir)
            self.assertTrue((out_dir / "game_state.h").exists())
            self.assertTrue((out_dir / "game_state.c").exists())
            self.assertTrue((out_dir / "game_state_devapi.c").exists())
            self.assertTrue((out_dir / "game_state_schema.gen.h").exists())

    def test_player_state_schema_supports_owned_maps_and_lists(self):
        schema = {
            "schema": "rb_dark_rpg.player_state",
            "document": "player",
            "lifetime": "persistent_profile",
            "version": 1,
            "string_max": 96,
            "reserved": [],
            "collections": {},
            "enums": {
                "BindState": ["none", "bound"],
                "QuestStatus": ["hidden", "available", "active", "completed", "failed", "content_missing"],
            },
            "types": {
                "StackInstance": {
                    "kind": "object",
                    "fields": [
                        {"path": "def_id", "id": 1, "type": "string", "max_length": 63},
                        {"path": "count", "id": 2, "type": "int", "default": 1, "min": 1, "max": 999999},
                    ],
                },
                "GearInstance": {
                    "kind": "object",
                    "fields": [
                        {"path": "def_id", "id": 1, "type": "string", "max_length": 63},
                        {"path": "durability", "id": 2, "type": "float", "default": 1, "min": 0, "max": 1},
                        {"path": "level", "id": 3, "type": "int", "default": 1, "min": 1, "max": 9999},
                        {"path": "bind_state", "id": 4, "type": "enum", "enum": "BindState", "default": "none"},
                    ],
                },
                "QuestState": {
                    "kind": "object",
                    "fields": [
                        {"path": "status", "id": 1, "type": "enum", "enum": "QuestStatus", "default": "hidden"},
                        {"path": "current_step_id", "id": 2, "type": "string?", "default": None, "max_length": 63},
                        {"path": "objective_progress", "id": 3, "type": "int", "default": 0, "min": 0, "max": 999999},
                    ],
                },
            },
            "fields": [
                {"path": "hero.level", "id": 1, "type": "int", "default": 1, "min": 1, "max": 99},
                {"path": "hero.xp", "id": 2, "type": "int", "default": 0, "min": 0, "max": 999999},
                {"path": "hero.hp", "id": 3, "type": "int", "default": 30, "min": 0, "max": 9999},
                {"path": "wallet.gold", "id": 4, "type": "int", "default": 0, "min": 0, "max": 2147483647},
                {"path": "inventory.stack_instances", "id": 5, "type": "map<string,StackInstance>", "max_count": 64},
                {"path": "inventory.gear_instances", "id": 6, "type": "map<string,GearInstance>", "max_count": 64},
                {"path": "inventory.bag_order", "id": 7, "type": "list<string>", "max_count": 128},
                {"path": "equipment.weapon_instance_id", "id": 8, "type": "string?", "default": None, "max_length": 63},
                {"path": "quests.quest_states", "id": 9, "type": "map<string,QuestState>", "max_count": 32},
                {"path": "quests.completed_step_ids", "id": 10, "type": "list<string>", "max_count": 128},
            ],
        }
        with tempfile.TemporaryDirectory() as tmp:
            schema_path = Path(tmp) / "player_state.schema.json"
            schema_path.write_text(json.dumps(schema), encoding="utf-8")
            text = generate_schema(schema_path, Path(tmp) / "generated")

        self.assertIn("GAME_STATE_SCHEMA_ID \"rb_dark_rpg.player_state\"", text)
        self.assertIn("typedef struct GameStackInstance", text)
        self.assertIn("typedef struct GameGearInstance", text)
        self.assertIn("typedef struct GameQuestState", text)
        self.assertIn("inventory_stack_instances", text)
        self.assertIn("quests_completed_step_ids", text)
        self.assertNotIn("hero_hp_max", text)


if __name__ == "__main__":
    unittest.main()
