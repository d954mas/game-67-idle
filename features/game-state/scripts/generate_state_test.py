import json
import tempfile
import unittest
from pathlib import Path

import generate_state


ROOT = Path(__file__).resolve().parents[3]
TEMPLATE_SCHEMA = ROOT / "templates" / "template" / "state" / "game_state.schema.json"
SETTINGS_SCHEMA = ROOT / "templates" / "template" / "state" / "settings.schema.json"
MINI_SCHEMA = ROOT / "features" / "game-state" / "tests" / "mini_state.schema.json"
GOLDEN = ROOT / "features" / "game-state" / "tests" / "golden"
CLOSED_TOKENS = ("rune_", "fishing_", "Rune", "Fishing", "rune.", "fishing.")

OUTPUT_SUFFIXES = (".h", ".c", "_schema.gen.h", "_events.gen.h", "_events.gen.c")


def generate(schema_path: Path, out_dir: Path, fragment: str | None = None) -> None:
    argv = ["--schema", str(schema_path), "--out-dir", str(out_dir)]
    if fragment is not None:
        argv += ["--fragment", fragment]
    rc = generate_state.main(argv)
    if rc != 0:
        raise AssertionError(f"generator returned {rc}")


def read_outputs(out_dir: Path, prefix: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for suffix in OUTPUT_SUFFIXES:
        name = f"{prefix}{suffix}"
        path = out_dir / name
        if not path.exists():
            raise AssertionError(f"missing generated file: {path}")
        out[name] = path.read_text(encoding="utf-8")
    return out


def generate_and_join(schema_path: Path, out_dir: Path, prefix: str, fragment: str | None = None) -> str:
    generate(schema_path, out_dir, fragment)
    return "\n".join(read_outputs(out_dir, prefix).values())


class StateCodegenTests(unittest.TestCase):
    def test_default_template_out_dir_uses_build_generated_folder(self):
        expected = ROOT / "templates" / "template" / "build" / "generated" / "game-state"
        self.assertEqual(generate_state.default_out_dir(TEMPLATE_SCHEMA), expected)

    def test_clean_schema_excludes_closed_prototype_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            text = generate_and_join(TEMPLATE_SCHEMA, Path(tmp) / "clean", "game_state", "game")
        for token in CLOSED_TOKENS:
            self.assertNotIn(token, text)

    def test_clean_schema_generates_all_outputs(self):
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp) / "generated"
            generate(TEMPLATE_SCHEMA, out_dir, "game")
            for suffix in OUTPUT_SUFFIXES:
                self.assertTrue((out_dir / f"game_state{suffix}").exists())

    def test_settings_schema_generates(self):
        # A6: the committed settings fragment schema stays valid and generates all
        # five per-fragment files (state layer + empty-events stub), so a broken
        # schema is caught here without a full build.
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp) / "generated"
            generate(SETTINGS_SCHEMA, out_dir, "settings")
            outputs = read_outputs(out_dir, "settings_state")
        self.assertIn("extern SettingsState settings_state;", outputs["settings_state.h"])
        self.assertIn("const GameSaveFragment settings_state_fragment", outputs["settings_state.c"])
        # empty events section -> stub table with zero descriptors.
        self.assertIn("settings_ev_desc_count = 0", outputs["settings_state_events.gen.c"])

    def test_header_has_no_devapi_decl(self):
        # A5: the DevAPI dispatch is a hand-written shell TU (game_save_devapi.c),
        # so the generator no longer emits a devapi source or a header declaration.
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp)
            generate(TEMPLATE_SCHEMA, out_dir, "game")
            hdr = (out_dir / "game_state.h").read_text(encoding="utf-8")
        self.assertNotIn("register_devapi", hdr)
        self.assertNotIn("NT_DEVAPI_ENABLED", hdr)
        self.assertFalse((out_dir / "game_state_devapi.c").exists())

    def test_player_state_schema_supports_owned_maps_and_lists(self):
        # v2 form of the rb_dark player state: fragment "player" -> PlayerState
        # namespace, fields/type-fields as maps, no numeric ids/document.
        schema = {
            "schema": "rb_dark_rpg.player_state",
            "schema_version": 2,
            "fragment": "player",
            "version": 1,
            "string_max": 96,
            "enums": {
                "BindState": ["none", "bound"],
                "QuestStatus": ["hidden", "available", "active", "completed", "failed", "content_missing"],
            },
            "types": {
                "StackInstance": {
                    "kind": "object",
                    "fields": {
                        "def_id": {"type": "string", "max_length": 63},
                        "count": {"type": "int", "default": 1, "min": 1, "max": 999999},
                    },
                },
                "GearInstance": {
                    "kind": "object",
                    "fields": {
                        "def_id": {"type": "string", "max_length": 63},
                        "durability": {"type": "float", "default": 1, "min": 0, "max": 1},
                        "level": {"type": "int", "default": 1, "min": 1, "max": 9999},
                        "bind_state": {"type": "enum", "enum": "BindState", "default": "none"},
                    },
                },
                "QuestState": {
                    "kind": "object",
                    "fields": {
                        "status": {"type": "enum", "enum": "QuestStatus", "default": "hidden"},
                        "current_step_id": {"type": "string?", "default": None, "max_length": 63},
                        "objective_progress": {"type": "int", "default": 0, "min": 0, "max": 999999},
                    },
                },
            },
            "fields": {
                "hero.level": {"type": "int", "default": 1, "min": 1, "max": 99},
                "hero.xp": {"type": "int", "default": 0, "min": 0, "max": 999999},
                "hero.hp": {"type": "int", "default": 30, "min": 0, "max": 9999},
                "wallet.gold": {"type": "int", "default": 0, "min": 0, "max": 2147483647},
                "inventory.stack_instances": {"type": "map<string,StackInstance>", "max_count": 64},
                "inventory.gear_instances": {"type": "map<string,GearInstance>", "max_count": 64},
                "inventory.bag_order": {"type": "list<string>", "max_count": 128},
                "equipment.weapon_instance_id": {"type": "string?", "default": None, "max_length": 63},
                "quests.quest_states": {"type": "map<string,QuestState>", "max_count": 32},
                "quests.completed_step_ids": {"type": "list<string>", "max_count": 128},
                "world.visited_location_ids": {"type": "list<string>", "max_count": 64, "default": ["hub_last_post"]},
            },
        }
        with tempfile.TemporaryDirectory() as tmp:
            schema_path = Path(tmp) / "player_state.schema.json"
            schema_path.write_text(json.dumps(schema), encoding="utf-8")
            text = generate_and_join(schema_path, Path(tmp) / "generated", "player_state", "player")

        self.assertIn("PLAYER_STATE_SCHEMA_ID \"rb_dark_rpg.player_state\"", text)
        self.assertIn("typedef struct PlayerStackInstance", text)
        self.assertIn("typedef struct PlayerGearInstance", text)
        self.assertIn("typedef struct PlayerQuestState", text)
        self.assertIn("extern PlayerState player_state;", text)
        self.assertIn("inventory_stack_instances", text)
        self.assertIn("quests_completed_step_ids", text)
        self.assertIn("gsj_copy_text(state->world_visited_location_ids[0]", text)
        self.assertIn("state->world_visited_location_ids_count = 1;", text)
        self.assertNotIn("hero_hp_max", text)

    # ------------------------------------------------------------------ golden

    def _assert_golden(self, schema_path: Path, prefix: str, fragment: str, golden_sub: str):
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp)
            generate(schema_path, out_dir, fragment)
            produced = read_outputs(out_dir, prefix)
        for name, got in produced.items():
            expected_path = GOLDEN / golden_sub / name
            self.assertTrue(expected_path.exists(), f"missing golden: {expected_path}")
            expected = expected_path.read_text(encoding="utf-8")
            self.assertEqual(got, expected, f"golden mismatch for {golden_sub}/{name}")

    def test_v2_template_golden(self):
        self._assert_golden(TEMPLATE_SCHEMA, "game_state", "game", "game")

    def test_v2_namespace_golden(self):
        self._assert_golden(MINI_SCHEMA, "mini_state", "mini", "mini")

    # ---------------------------------------------------------------- property

    def test_property_game_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp)
            generate(TEMPLATE_SCHEMA, out_dir, "game")
            files = read_outputs(out_dir, "game_state")
        src = files["game_state.c"]
        hdr = files["game_state.h"]

        # monolith is dead
        self.assertNotIn("g_game_state", src)
        self.assertNotIn("g_game_state", hdr)
        self.assertNotIn("make_save_doc", src)
        self.assertNotIn("game_state_save(", src)
        self.assertNotIn("static bool read_int_range", src)  # no static helper copies
        self.assertNotIn("static void set_error", src)

        # v2 shape
        self.assertIn("extern GameState game_state;", hdr)
        self.assertIn("const GameSaveFragment game_state_fragment", src)
        self.assertIn('#include "game_state_json.h"', src)
        self.assertIn("gsj_read_int_range", src)
        # i64 wire coverage (gsj_add_i64/gsj_read_i64) is NOT asserted here: the
        # template's `game` fragment carries no i64 field post-T0327-hygiene (rb-dark
        # RPG model gutted to the honest demo shape). The property is still covered by
        # test_v2_namespace_golden (MINI_SCHEMA's `total`/`Cell.count` i64 fields,
        # golden/mini/mini_state.c) -- this test only needs to stay schema-agnostic
        # about which downstream game happens to have an i64 field.
        self.assertIn(".steps         = NULL", src)  # no migrations for the template

        # E2: typed events are a SEPARATE file family (read the event keys, not the
        # state .c). state output above is unaffected by the events section.
        evh = files["game_state_events.gen.h"]
        evc = files["game_state_events.gen.c"]
        self.assertIn("typedef struct GameEvShapeChanged", evh)
        self.assertIn('#include "game_event_desc.h"', evh)
        self.assertIn("game_emit_shape_changed", evh)
        self.assertIn("game_ev_shape_changed_type", evh)
        self.assertIn("const game_event_desc_t *const game_ev_descs[]", evh)
        self.assertIn("game_emit_shape_changed", evc)
        self.assertIn("game_ev_shape_changed_type", evc)
        self.assertIn("const game_event_desc_t *const game_ev_descs[]", evc)
        self.assertIn("_Static_assert", evc)

    def test_property_mini_events(self):
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp)
            generate(MINI_SCHEMA, out_dir, "mini")
            files = read_outputs(out_dir, "mini_state")
        evh = files["mini_state_events.gen.h"]
        evc = files["mini_state_events.gen.c"]
        # f64 proof: schema 'float' -> C double (NOT state float(C float)).
        self.assertIn("double rate;", evh)
        # string field -> uint32 byte offset, never an inline char array.
        self.assertIn("uint32_t label;", evh)
        self.assertNotIn("char label[", evh)
        # scalar-only path + rich descriptor + union staging.
        self.assertIn("typedef struct MiniEvTicked", evh)
        self.assertIn("GAME_EVENT_FT_BYTES", evc)
        self.assertIn("union {", evc)

    def test_migrations_and_hooks_descriptor(self):
        schema = {
            "schema": "mig.state",
            "schema_version": 2,
            "fragment": "mig",
            "version": 3,
            "string_max": 32,
            "hooks": {"on_new_game": True, "reconcile": True},
            "migrations": [
                {"to_version": 2, "fn": "mig_migrate_v1_to_v2"},
                {"to_version": 3, "fn": "mig_migrate_v2_to_v3"},
            ],
            "enums": {},
            "types": {},
            "fields": {
                "total": {"type": "i64", "default": 0, "min": 0, "max": 9000000000000000000},
            },
        }
        with tempfile.TemporaryDirectory() as tmp:
            schema_path = Path(tmp) / "mig.schema.json"
            schema_path.write_text(json.dumps(schema), encoding="utf-8")
            generate(schema_path, Path(tmp) / "gen", "mig")
            src = (Path(tmp) / "gen" / "mig_state.c").read_text(encoding="utf-8")

        self.assertIn("extern bool mig_migrate_v1_to_v2(cJSON *frag, char *err, int cap);", src)
        self.assertIn("extern bool mig_migrate_v2_to_v3(cJSON *frag, char *err, int cap);", src)
        self.assertIn("static const GameSaveMigrateFn mig_state_migration_steps[] = {", src)
        self.assertIn(".steps         = mig_state_migration_steps", src)
        self.assertIn("extern void mig_on_new_game(void);", src)
        self.assertIn(".on_new_game   = mig_on_new_game", src)
        self.assertIn("extern void mig_reconcile(void);", src)
        self.assertIn(".reconcile     = mig_reconcile", src)

    # -------------------------------------------------------------- validation

    def _write_and_load(self, schema: dict) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "s.schema.json"
            path.write_text(json.dumps(schema), encoding="utf-8")
            generate_state.load_schema(path)

    def _base(self, **overrides) -> dict:
        schema = {
            "schema": "mini.state",
            "schema_version": 2,
            "fragment": "mini",
            "version": 1,
            "string_max": 32,
            "enums": {},
            "types": {},
            "fields": {"total": {"type": "i64", "default": 0, "min": 0, "max": 9000000000000000000}},
        }
        schema.update(overrides)
        return schema

    def test_reject_legacy_missing_schema_version(self):
        with self.assertRaises(SystemExit) as ctx:
            self._write_and_load({"schema": "x", "document": "game", "version": 1, "string_max": 64, "fields": {}})
        self.assertIn("shipping tag", str(ctx.exception))

    def test_reject_legacy_document_key(self):
        with self.assertRaises(SystemExit) as ctx:
            self._write_and_load(self._base(document="game"))
        self.assertIn("shipping tag", str(ctx.exception))

    def test_pascal_multiword_fragment_id(self):
        self.assertEqual(generate_state._pascal("meta_progression"), "MetaProgression")
        self.assertEqual(generate_state._pascal("game"), "Game")

    def test_reject_bad_fragment_charset(self):
        with self.assertRaises(SystemExit):
            self._write_and_load(self._base(fragment="Mini"))

    def test_reject_version_migration_mismatch(self):
        with self.assertRaises(SystemExit):
            self._write_and_load(self._base(version=3))

    def test_reject_field_named_v(self):
        with self.assertRaises(SystemExit):
            self._write_and_load(self._base(fields={"v": {"type": "int", "default": 0, "min": 0, "max": 9}}))

    def test_reject_c_ident_collision(self):
        with self.assertRaises(SystemExit):
            self._write_and_load(self._base(fields={
                "wallet.soft": {"type": "int", "default": 0, "min": 0, "max": 9},
                "wallet_soft": {"type": "int", "default": 0, "min": 0, "max": 9},
            }))

    def test_reject_i64_max_beyond_int64(self):
        with self.assertRaises(SystemExit):
            self._write_and_load(self._base(fields={"big": {"type": "i64", "default": 0, "min": 0, "max": 2**63}}))

    def test_reject_reserved_name_reused(self):
        with self.assertRaises(SystemExit):
            self._write_and_load(self._base(reserved=["total"], fields={"total": {"type": "int", "default": 0, "min": 0, "max": 9}}))

    def test_validate_field_names_rejects_duplicate_path(self):
        # A map cannot express duplicate keys from a file, so exercise the guard
        # directly on the normalized list form.
        fields = [
            {"path": "a", "type": "int", "default": 0, "min": 0, "max": 9},
            {"path": "a", "type": "int", "default": 0, "min": 0, "max": 9},
        ]
        with self.assertRaises(SystemExit):
            generate_state.validate_field_names("fields", fields, set(), True)

    # ---------------------------------------------------------- events (E2)
    # Every negative anchors on a word/phrase unique to its error BRANCH (LOW-2):
    # a mutation that swaps which guard fires now fails the test instead of passing.

    def test_reject_event_name_bad_charset(self):
        for bad in ("Foo", "a.b", "1x"):
            with self.assertRaisesRegex(SystemExit, r"event name .* must match"):
                self._write_and_load(self._base(events={bad: {"fields": {}}}))

    def test_reject_event_name_c_ident_collision(self):
        with self.assertRaisesRegex(SystemExit, r"event names .* collide on C identifier"):
            self._write_and_load(self._base(events={
                "a_b": {"fields": {}},
                "a__b": {"fields": {}},  # both -> C ident a_b (and struct MiniEvAB)
            }))

    def test_reject_event_name_reserved_symbol(self):
        # descs/desc_count/register are reserved for the per-fragment table symbols.
        for bad in ("descs", "desc_count", "register"):
            with self.assertRaisesRegex(SystemExit, r"per-fragment event table"):
                self._write_and_load(self._base(events={bad: {"fields": {}}}))

    def test_reject_unknown_event_field_type(self):
        # str/f64/enum catch the dictionary-split failure mode (event 'string'/'float',
        # no event enum) with a clean SystemExit rather than broken C.
        for bad in ("str", "f64", "enum"):
            with self.assertRaisesRegex(SystemExit, r"unknown event field type"):
                self._write_and_load(self._base(events={"cell_spawned": {"fields": {"x": {"type": bad}}}}))

    def test_reject_reserved_event_field_envelope(self):
        for bad in ("type", "seq", "tick"):
            with self.assertRaisesRegex(SystemExit, r"reserved \(envelope/accessor\)"):
                self._write_and_load(self._base(events={"cell_spawned": {"fields": {bad: {"type": "int"}}}}))

    def test_reject_reserved_event_field_symbol(self):
        # desc/fields would redefine the generated per-event descriptor / fields-array
        # symbols (accessor name clash) -> clean SystemExit, not a dirty duplicate.
        for bad in ("desc", "fields"):
            with self.assertRaisesRegex(SystemExit, r"redefine the generated per-event"):
                self._write_and_load(self._base(events={"cell_spawned": {"fields": {bad: {"type": "string"}}}}))

    def test_reject_event_field_c_ident_collision(self):
        with self.assertRaisesRegex(SystemExit, r"fields .* collide on C identifier"):
            self._write_and_load(self._base(events={"cell_spawned": {"fields": {
                "x_y": {"type": "int"},
                "x__y": {"type": "int"},
            }}}))

    def test_reject_bytes_len_synthesized_collision(self):
        # bytes 'blob' synthesizes 'blob_len'; a declared 'blob_len' would double the
        # C member -> clean SystemExit, not a dirty duplicate-member compile error.
        with self.assertRaisesRegex(SystemExit, r"synthesizes .* which collides"):
            self._write_and_load(self._base(events={"cell_spawned": {"fields": {
                "blob": {"type": "bytes"},
                "blob_len": {"type": "int"},
            }}}))

    def test_reject_event_spec_extra_key(self):
        with self.assertRaisesRegex(SystemExit, r"cell_spawned has unsupported keys"):
            self._write_and_load(self._base(events={"cell_spawned": {"fields": {}, "lifetime": "frame"}}))

    def test_reject_event_field_spec_extra_key(self):
        with self.assertRaisesRegex(SystemExit, r"cell_spawned\.x has unsupported keys"):
            self._write_and_load(self._base(events={"cell_spawned": {"fields": {"x": {"type": "int", "min": 0}}}}))


if __name__ == "__main__":
    unittest.main()
