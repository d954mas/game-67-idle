import copy
import json
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest import mock


SCRIPT_DIR = Path(__file__).resolve().parent
FIXTURES = SCRIPT_DIR.parent / "tests" / "fixtures"
sys.path.insert(0, str(SCRIPT_DIR))

from generate_items_api_proof import (  # noqa: E402
    DiagnosticError,
    generate,
    load_and_validate,
    xxh64,
)
import generate_items_api_proof as proof  # noqa: E402


class GenerateItemsApiProofTests(unittest.TestCase):
    def fixture(self, name):
        path = FIXTURES / name
        return path, json.loads(path.read_text(encoding="utf-8"))

    def item(self, snapshot, def_id):
        return next(item for item in snapshot["items"] if item["def_id"] == def_id)

    def assert_code(self, snapshot, code, hasher=xxh64):
        with self.assertRaises(DiagnosticError) as raised:
            load_and_validate(snapshot, hasher=hasher)
        diagnostic = raised.exception.diagnostic
        self.assertEqual(code, diagnostic["code"])
        self.assertEqual(
            {"code", "file", "line", "column", "path"},
            set(diagnostic),
        )

    def test_known_engine_xxh64_ids(self):
        self.assertEqual(0xE662E696028B01C4, xxh64(b"game.gold", seed=0))
        self.assertEqual(0xB36736FA950BF10D, xxh64(b"game.iron_sword", seed=0))

    def test_core_only_emits_no_weapon_or_attack_api(self):
        path, _ = self.fixture("items_api_core_proof.json")
        with tempfile.TemporaryDirectory() as temp:
            generate(path, Path(temp))
            header = (Path(temp) / "items_game.gen.h").read_text(encoding="utf-8")
            internal = (Path(temp) / "items_game.internal.gen.h").read_text(encoding="utf-8")
            source = (Path(temp) / "items_game.gen.c").read_text(encoding="utf-8")
            self.assertIn("ITEM_GAME_GOLD", header)
            self.assertNotIn("weapon", header.lower())
            self.assertNotIn("attack", header.lower())
            self.assertNotIn("weapon", source.lower())
            self.assertNotIn("attack", source.lower())
            self.assertIn("items_game_internal_item_count", internal)

    def test_weapon_emits_typed_field_and_ids(self):
        path, _ = self.fixture("items_api_weapon_proof.json")
        with tempfile.TemporaryDirectory() as temp:
            generate(path, Path(temp))
            header = (Path(temp) / "items_game.gen.h").read_text(encoding="utf-8")
            internal = (Path(temp) / "items_game.internal.gen.h").read_text(encoding="utf-8")
            source = (Path(temp) / "items_game.gen.c").read_text(encoding="utf-8")
            self.assertIn("int64_t attack;", header)
            self.assertIn("ITEM_FIELD_GAME_WEAPON_LEVEL_ATTACK", header)
            self.assertIn("ITEMS_GAME_HAS_WEAPON", header)
            self.assertIn("items_weapon_level", header)
            self.assertIn("UINT64_C(0xE662E696028B01C4)", header)
            self.assertIn("UINT64_C(0xB36736FA950BF10D)", header)
            self.assertIn("ITEM_GAME_EXTRAORDINARILY_LONG_BALANCE_RESOURCE_IDENTIFIER", header)
            self.assertIn('"game.extraordinarily_long_balance_resource_identifier"', source)
            self.assertIn('#include "features/items/items.h"', source)
            self.assertIn("items_game_internal_def_id", internal)
            self.assertNotIn("items_try_get_string", source)
            self.assertNotIn("item_def_ref_t items_get", source)

    def test_field_metadata_evolution_and_reserved_identity_contract(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        required = {
            "field_id", "member", "scope", "c_type", "required_for", "min", "max",
            "unit", "rounding", "label_key", "ui", "evolution", "source",
        }
        self.assertEqual(required, set(base["fields"][0]))
        for key in sorted(required):
            bad = copy.deepcopy(base)
            del bad["fields"][0][key]
            with self.subTest(missing=key):
                self.assert_code(bad, "field-metadata-required")

        bad = copy.deepcopy(base); bad["fields"][0]["required_for"] = ["weapon", "armor"]
        self.assert_code(bad, "field-capability-cardinality")
        bad = copy.deepcopy(base); bad["fields"][0]["min"] = float("nan")
        self.assert_code(bad, "invalid-field-range")
        bad = copy.deepcopy(base); bad["fields"][0]["min"] = 5; bad["fields"][0]["max"] = 4
        self.assert_code(bad, "invalid-field-range")
        bad = copy.deepcopy(base); bad["fields"][0]["unit"] = 7
        self.assert_code(bad, "invalid-field-metadata")
        bad = copy.deepcopy(base); bad["fields"][0]["ui"]["layout"] = "grid"
        self.assert_code(bad, "field-ui-key-forbidden")
        bad = copy.deepcopy(base); bad["fields"][0]["evolution"]["since"] = 0
        self.assert_code(bad, "invalid-field-evolution")
        bad = copy.deepcopy(base); bad["reserved_field_ids"].append("game.weapon.level.attack")
        self.assert_code(bad, "reserved-field-id-reused")

    def test_views_reference_field_ids_and_cannot_redefine_schema(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        load_and_validate(base)
        for forbidden in ("label_key", "unit", "c_type"):
            bad = copy.deepcopy(base); bad["views"][0][forbidden] = "bad"
            with self.subTest(forbidden=forbidden):
                self.assert_code(bad, "view-schema-metadata-forbidden")
        bad = copy.deepcopy(base); bad["views"][0]["order"] = ["game.weapon.level.missing"]
        self.assert_code(bad, "unknown-view-field")
        bad = copy.deepcopy(base); bad["views"][0]["chart"]["field_ids"] = ["game.weapon.level.missing"]
        self.assert_code(bad, "unknown-view-field")

    def test_luau_schema_stub_and_all_outputs_are_atomic_write_if_different(self):
        path, _ = self.fixture("items_api_weapon_proof.json")
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            with mock.patch.object(proof.os, "replace", wraps=os.replace) as replace:
                generate(path, out)
                self.assertEqual(4, replace.call_count)
            luau = out / "items_game.luau"
            content = luau.read_text(encoding="utf-8")
            self.assertIn("---@meta", content)
            self.assertIn("---@class ItemsGameWeaponLevelRow", content)
            self.assertIn(
                "---@field attack integer # field_id=game.weapon.level.attack unit=damage label_key=item.attack",
                content,
            )
            self.assertIn("---@class ItemsGameItemDefinition", content)
            self.assertIn("---@param definition ItemsGameItemDefinition", content)
            self.assertIn("function items.define(definition) end", content)
            self.assertIn("function items.extend_schema(extension) end", content)
            self.assertIn("function items.view(view) end", content)
            mtimes = {p.name: p.stat().st_mtime_ns for p in out.iterdir()}
            time.sleep(0.01)
            with mock.patch.object(proof.os, "replace", wraps=os.replace) as replace:
                generate(path, out)
                self.assertEqual(0, replace.call_count)
            self.assertEqual(mtimes, {p.name: p.stat().st_mtime_ns for p in out.iterdir()})

    def test_renderer_is_field_driven_and_float_literals_are_canonical(self):
        path, base = self.fixture("items_api_weapon_proof.json")
        defense = copy.deepcopy(base["fields"][0])
        defense.update({
            "field_id": "game.weapon.level.defense",
            "member": "defense",
            "c_type": "double",
            "min": 0.0,
            "max": 1.0,
            "unit": "ratio",
            "rounding": "none",
            "label_key": "item.defense",
            "ui": {"format": "decimal", "description_key": "item.defense.description"},
            "source": {"file": "items_api_weapon_proof.lua", "line": 7, "column": 3},
        })
        accuracy = copy.deepcopy(defense)
        accuracy.update({
            "field_id": "game.weapon.level.accuracy",
            "member": "accuracy",
            "c_type": "float",
            "label_key": "item.accuracy",
            "ui": {"format": "decimal", "description_key": "item.accuracy.description"},
        })
        speed = copy.deepcopy(accuracy)
        speed.update({
            "field_id": "game.weapon.level.speed",
            "member": "speed",
            "label_key": "item.speed",
            "ui": {"format": "decimal", "description_key": "item.speed.description"},
        })
        base["fields"].extend([defense, accuracy, speed])
        for field in (defense, accuracy, speed):
            base["views"][0]["order"].append(field["field_id"])
            base["views"][0]["chart"]["field_ids"].append(field["field_id"])
        for item in base["items"]:
            if item["kind"] == "weapon":
                for row in item["levels"]:
                    row.update({"defense": 1, "accuracy": 1, "speed": 0.25})
                    for member in ("defense", "accuracy", "speed"):
                        row["provenance"][member] = row["provenance"]["attack"]
        with tempfile.TemporaryDirectory() as temp:
            temp_path = Path(temp)
            snapshot = temp_path / "snapshot.json"
            snapshot.write_text(json.dumps(base), encoding="utf-8")
            generate(snapshot, temp_path)
            header = (temp_path / "items_game.gen.h").read_text(encoding="utf-8")
            source = (temp_path / "items_game.gen.c").read_text(encoding="utf-8")
            self.assertIn("double defense;", header)
            self.assertIn("ITEM_FIELD_GAME_WEAPON_LEVEL_DEFENSE", header)
            self.assertIn(".defense = 1.0", source)
            self.assertIn(".accuracy = 1.0f", source)
            self.assertIn(".speed = 0.25f", source)

        for value in (float("nan"), float("inf"), float("-inf")):
            bad = copy.deepcopy(base)
            self.item(bad, "game.fixed_sword")["levels"][0]["speed"] = value
            with self.subTest(value=value):
                self.assert_code(bad, "non-finite-field-value")

    def test_exact_diagnostic_uses_typed_source_span(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        fixed = self.item(base, "game.fixed_sword")
        del fixed["levels"][0]["attack"]
        with self.assertRaises(DiagnosticError) as raised:
            load_and_validate(base)
        self.assertEqual({
            "code": "required-field-missing",
            "file": "items_api_weapon_proof.lua",
            "line": 34,
            "column": 3,
            "path": "$.items[3].levels[0].attack",
        }, raised.exception.diagnostic)

    def test_write_if_different_keeps_mtime(self):
        path, _ = self.fixture("items_api_weapon_proof.json")
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            generate(path, out)
            header = out / "items_game.gen.h"
            first = header.stat().st_mtime_ns
            time.sleep(0.01)
            generate(path, out)
            self.assertEqual(first, header.stat().st_mtime_ns)

    def test_rejects_identity_and_shape_errors(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        cases = []

        bad = copy.deepcopy(base); bad["items"][0]["def_id"] = "Bad ID"
        cases.append((bad, "invalid-def-id"))
        bad = copy.deepcopy(base); bad["items"].append(copy.deepcopy(bad["items"][0]))
        cases.append((bad, "duplicate-def-id"))
        bad = copy.deepcopy(base); bad["fields"].append(copy.deepcopy(bad["fields"][0]))
        cases.append((bad, "duplicate-field-id"))
        bad = copy.deepcopy(base); bad["fields"][0]["field_id"] = "items.level.cost_to_reach"
        cases.append((bad, "sealed-field-redefinition"))
        bad = copy.deepcopy(base); bad["items"][0]["surprise"] = 1
        cases.append((bad, "unknown-key"))
        bad = copy.deepcopy(base); bad["fields"][0]["c_type"] = "size_t"
        cases.append((bad, "unsupported-c-type"))

        for snapshot, code in cases:
            with self.subTest(code=code):
                self.assert_code(snapshot, code)

    def test_rejects_capability_level_and_numeric_errors(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        cases = []

        bad = copy.deepcopy(base); del self.item(bad, "game.fixed_sword")["levels"][0]["attack"]
        cases.append((bad, "required-field-missing"))
        bad = copy.deepcopy(base); self.item(bad, "game.gold")["authoring_mode"] = "table"; self.item(bad, "game.gold")["levels"] = [{
            "level": 1, "attack": 1, "provenance": {"attack": "table"},
            "source": {"file": "x.lua", "line": 1, "column": 1},
        }]
        cases.append((bad, "field-wrong-kind"))
        bad = copy.deepcopy(base); self.item(bad, "game.iron_sword")["levels"][1]["level"] = 3
        cases.append((bad, "level-not-contiguous"))
        bad = copy.deepcopy(base); self.item(bad, "game.iron_sword")["levels"][0]["cost_to_reach"] = {"kind": "free"}
        cases.append((bad, "level-one-transition"))
        bad = copy.deepcopy(base); self.item(bad, "game.fixed_sword")["levels"][0]["attack"] = 1.5
        cases.append((bad, "integer-required"))
        bad = copy.deepcopy(base); self.item(bad, "game.fixed_sword")["levels"][0]["attack"] = 1000001
        cases.append((bad, "value-out-of-range"))
        bad = copy.deepcopy(base); self.item(bad, "game.fixed_sword")["levels"][0]["attack"] = 2**63
        cases.append((bad, "integer-overflow"))

        for snapshot, code in cases:
            with self.subTest(code=code):
                self.assert_code(snapshot, code)

    def test_rejects_hash_and_sanitized_name_collisions(self):
        _, base = self.fixture("items_api_core_proof.json")
        self.assert_code(base, "item-id-hash-collision", hasher=lambda _data, seed=0: 7)

        bad = copy.deepcopy(base)
        bad["items"] = [
            {"def_id": "game.a_b", "kind": "material", "tags": [], "stack": 1, "authoring_mode": "none", "levels": [], "source": {"file": "x.lua", "line": 1, "column": 1}},
            {"def_id": "game_a.b", "kind": "material", "tags": [], "stack": 1, "authoring_mode": "none", "levels": [], "source": {"file": "x.lua", "line": 2, "column": 1}},
        ]
        self.assert_code(bad, "c-name-collision")

    def test_tags_are_required_unique_valid_and_orthogonal_to_kind(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        self.assertTrue(all(isinstance(item["tags"], list) for item in base["items"]))
        bad = copy.deepcopy(base); del bad["items"][0]["tags"]
        self.assert_code(bad, "item-metadata-required")
        bad = copy.deepcopy(base); bad["items"][0]["tags"] = ["economy", "economy"]
        self.assert_code(bad, "duplicate-item-tag")
        bad = copy.deepcopy(base); bad["items"][0]["tags"] = ["Bad Tag"]
        self.assert_code(bad, "invalid-item-tag")
        bad = copy.deepcopy(base); bad["items"][0]["tags"] = ["currency"]
        self.assert_code(bad, "tag-equals-kind")

    def test_authoring_modes_and_bounded_provenance(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        modes = {item["authoring_mode"] for item in base["items"]}
        self.assertEqual({"none", "single", "table", "generate", "columns"}, modes)
        lua = (FIXTURES / "items_api_weapon_proof.lua").read_text(encoding="utf-8")
        for vocabulary in ("levels.single", "levels.table", "levels.generate", "levels.columns", "overrides"):
            self.assertIn(vocabulary, lua)
        bad = copy.deepcopy(base); self.item(bad, "game.fixed_sword")["authoring_mode"] = "magic"
        self.assert_code(bad, "invalid-authoring-mode")
        bad = copy.deepcopy(base); self.item(bad, "game.fixed_sword")["levels"][0]["provenance"]["other"] = "single"
        self.assert_code(bad, "unknown-provenance-field")
        bad = copy.deepcopy(base); self.item(bad, "game.fixed_sword")["levels"][0]["provenance"]["attack"] = "magic"
        self.assert_code(bad, "invalid-provenance-value")
        bad = copy.deepcopy(base); del self.item(bad, "game.fixed_sword")["levels"][0]["provenance"]["attack"]
        self.assert_code(bad, "missing-provenance-field")
        bad = copy.deepcopy(base); del self.item(bad, "game.table_sword")["levels"][1]["provenance"]["cost_to_reach"]
        self.assert_code(bad, "missing-provenance-field")

    def test_transition_shape_and_stackable_unique_cost_resources(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        bad = copy.deepcopy(base); self.item(bad, "game.table_sword")["levels"][1]["cost_to_reach"] = {"kind": "free", "cost": []}
        self.assert_code(bad, "invalid-transition")
        bad = copy.deepcopy(base); del self.item(bad, "game.table_sword")["levels"][1]["cost_to_reach"]
        self.assert_code(bad, "level-transition-required")
        bad = copy.deepcopy(base); self.item(bad, "game.fixed_sword")["acquire"] = {"kind": "cost", "cost": []}
        self.assert_code(bad, "invalid-transition")
        bad = copy.deepcopy(base); transition = self.item(bad, "game.iron_sword")["levels"][1]["cost_to_reach"]
        transition["cost"].append(copy.deepcopy(transition["cost"][0]))
        self.assert_code(bad, "duplicate-cost-resource")
        bad = copy.deepcopy(base); self.item(bad, "game.iron_sword")["levels"][1]["cost_to_reach"]["cost"][0]["item_ref"] = "game.fixed_sword"
        self.assert_code(bad, "cost-resource-not-stackable")

    def test_malformed_source_and_range_always_raise_typed_diagnostic(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        bad = copy.deepcopy(base); bad["fields"][0]["source"] = "bad"
        self.assert_code(bad, "invalid-source-span")
        bad = copy.deepcopy(base); bad["fields"][0]["source"]["line"] = "bad"
        self.assert_code(bad, "invalid-source-span")
        bad = copy.deepcopy(base); bad["fields"][0]["min"] = None
        self.assert_code(bad, "invalid-field-range")

    def test_capability_ids_are_c_safe_and_do_not_collide(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        for invalid in ("weapon-type", "Weapon", "1weapon"):
            bad = copy.deepcopy(base); bad["fields"][0]["required_for"] = [invalid]
            with self.subTest(invalid=invalid):
                self.assert_code(bad, "invalid-capability-id")

        bad = copy.deepcopy(base)
        duplicate = copy.deepcopy(bad["fields"][0])
        duplicate.update({"field_id": "game.other.level.power", "member": "power", "required_for": ["Weapon"]})
        bad["fields"].append(duplicate)
        self.assert_code(bad, "invalid-capability-id")

    def test_distinct_field_ids_cannot_generate_the_same_macro(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        first = copy.deepcopy(base["fields"][0])
        first.update({"field_id": "game.a_b.c", "member": "first"})
        second = copy.deepcopy(base["fields"][0])
        second.update({"field_id": "game.a.b_c", "member": "second"})
        base["fields"] = [first, second]
        self.assert_code(base, "field-c-name-collision")

    def test_capability_with_zero_items_uses_portable_sentinel_storage(self):
        path, base = self.fixture("items_api_core_proof.json")
        armor = {
            "field_id": "game.armor.level.defense", "member": "defense", "scope": "level_row",
            "c_type": "int64_t", "required_for": ["armor"], "min": 0, "max": 1000,
            "unit": "defense", "rounding": "exact", "label_key": "item.defense",
            "ui": {"format": "integer", "description_key": "item.defense.description"},
            "evolution": {"since": 1, "deprecated": False},
            "source": {"file": "items_api_core_proof.lua", "line": 1, "column": 1},
        }
        base["fields"].append(armor)
        with tempfile.TemporaryDirectory() as temp:
            temp_path = Path(temp)
            snapshot = temp_path / "snapshot.json"
            snapshot.write_text(json.dumps(base), encoding="utf-8")
            generate(snapshot, temp_path)
            source = (temp_path / "items_game.gen.c").read_text(encoding="utf-8")
            self.assertIn("static const item_armor_level_t s_armor_levels[]", source)
            self.assertIn("    { 0 },", source)
            self.assertNotIn("s_armor_levels[] = {\n};", source)
            self.assertIn("s_armor_spans[] = {\n    { UINT32_C(0), UINT32_C(0) },", source)


if __name__ == "__main__":
    unittest.main()
