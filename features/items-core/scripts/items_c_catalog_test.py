import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest


SCRIPT_DIR = Path(__file__).resolve().parent
FIXTURES = SCRIPT_DIR.parent / "tests" / "fixtures"
sys.path.insert(0, str(SCRIPT_DIR))

from items_c_catalog import (  # noqa: E402
    CatalogFailure,
    generate,
    normalize,
    render,
    write_if_different,
)
from items_snapshot import snapshot_content_hash  # noqa: E402
from items_xxh64 import xxh64  # noqa: E402


def reseal(snapshot):
    """Re-stamp the content hash so a mutated fixture reaches the projection layer."""
    snapshot["content_hash"] = snapshot_content_hash(snapshot)
    return snapshot


class ItemsCCatalogTests(unittest.TestCase):
    def fixture(self, name):
        path = FIXTURES / name
        return path, json.loads(path.read_text(encoding="utf-8"))

    def item(self, snapshot, item_id):
        return next(item for item in snapshot["items"] if item["id"] == item_id)

    def assert_code(self, snapshot, code):
        with self.assertRaises(CatalogFailure) as raised:
            normalize(snapshot)
        self.assertEqual(code, str(raised.exception).partition(":")[0])

    def test_known_engine_xxh64_ids(self):
        self.assertEqual(0xE662E696028B01C4, xxh64(b"game.gold", seed=0))
        self.assertEqual(0xB36736FA950BF10D, xxh64(b"game.iron_sword", seed=0))

    def test_core_only_emits_no_capability_api(self):
        _, snapshot = self.fixture("items_api_core_proof.json")
        rendered = render(normalize(snapshot))
        header = rendered["items_catalog.gen.h"]
        source = rendered["items_catalog.gen.c"]
        self.assertIn("ITEM_GAME_GOLD", header)
        self.assertIn("#define ITEMS_CATALOG_CAPABILITY_COUNT UINT32_C(0)", header)
        for text in (header, source):
            self.assertNotIn("weapon", text.lower())
            self.assertNotIn("attack", text.lower())
        self.assertIn(
            "items_catalog_internal_item_count", rendered["items_catalog.internal.gen.h"],
        )

    def test_weapon_catalog_emits_typed_field_ids_and_currency(self):
        _, snapshot = self.fixture("items_api_weapon_proof.json")
        rendered = render(normalize(snapshot))
        header = rendered["items_catalog.gen.h"]
        source = rendered["items_catalog.gen.c"]
        self.assertIn("#define ITEMS_GAME_HAS_WEAPON 1", header)
        self.assertIn("    int64_t attack;", header)
        self.assertIn('#define ITEM_FIELD_GAME_WEAPON_LEVEL_ATTACK "game.weapon.level.attack"', header)
        self.assertIn("#define ITEMS_CATALOG_ITEM_COUNT UINT32_C(7)", header)
        self.assertIn("item_weapon_level_t items_weapon_level(item_def_ref_t ref, uint32_t level);", header)
        # game.gold is the only currency and carries the authored cap.
        self.assertIn('"game.gold", INT64_C(5000), true', source)
        self.assertEqual(1, source.count(", true },"))
        # levels.columns with an override: 10, 15, then 21 instead of 20.
        self.assertIn(".attack = INT64_C(21),", source)
        self.assertIn("ITEM_TRANSITION_FREE", source)
        self.assertIn("ITEM_TRANSITION_COST", source)

    def test_a_fractional_field_bound_to_an_item_kind_says_so(self):
        """One registry serves item kinds and track kinds, so required_for can name the
        wrong space. The error has to name the field and the kind, or the author reads
        an ABI complaint about a column they meant for a track."""
        _, snapshot = self.fixture("items_api_weapon_proof.json")
        field = next(f for f in snapshot["fields"] if f["id"] == "game.weapon.level.attack")
        field["type"] = "f64"
        field.pop("rounding", None)
        with self.assertRaises(CatalogFailure) as raised:
            normalize(reseal(snapshot))
        message = str(raised.exception)
        self.assertIn("game.weapon.level.attack", message)
        self.assertIn("f64", message)
        self.assertIn("weapon", message)
        self.assertIn("exact integers", message)

    def test_generated_indices_follow_sorted_item_ids(self):
        _, snapshot = self.fixture("items_api_weapon_proof.json")
        model = normalize(snapshot)
        self.assertEqual(
            sorted(item["id"] for item in snapshot["items"]),
            [item["id"] for item in model["items"]],
        )
        # Span zero stays the null cost so an absent transition never indexes data.
        self.assertEqual((0, 0), model["cost_spans"][0])

    def test_content_fingerprint_follows_snapshot_metadata(self):
        _, snapshot = self.fixture("items_api_weapon_proof.json")
        before = normalize(snapshot)
        renamed = copy.deepcopy(snapshot)
        self.item(renamed, "game.gold")["name"] = "Bullion"
        after = normalize(reseal(renamed))
        self.assertEqual(before["schema_abi"], after["schema_abi"])
        self.assertNotEqual(before["content_fingerprint"], after["content_fingerprint"])

    def test_luau_stub_documents_capability_rows(self):
        _, snapshot = self.fixture("items_api_weapon_proof.json")
        luau = render(normalize(snapshot))["items_catalog.luau"]
        self.assertIn("---@class ItemsGameWeaponLevelRow", luau)
        self.assertIn("---@field attack integer # field_id=game.weapon.level.attack unit=damage", luau)
        self.assertIn("function items.define(definition) end", luau)

    def test_outputs_are_atomic_write_if_different(self):
        path, _ = self.fixture("items_api_weapon_proof.json")
        snapshot = json.loads(path.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as temp:
            out_dir = Path(temp)
            self.assertTrue(all(generate(snapshot, out_dir).values()))
            written = out_dir / "items_catalog.gen.c"
            stamp = written.stat().st_mtime_ns
            time.sleep(0.01)
            self.assertFalse(any(generate(snapshot, out_dir).values()))
            self.assertEqual(stamp, written.stat().st_mtime_ns)
            self.assertEqual([], [name for name in os.listdir(out_dir) if name.startswith(".")])

    def test_rejects_snapshot_shape_and_identity_errors(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        wrong_schema = copy.deepcopy(base)
        wrong_schema["schema"] = "items.snapshot.v2"
        self.assert_code(wrong_schema, "catalog.snapshot_schema")

        stale_hash = copy.deepcopy(base)
        stale_hash["content_hash"] = "sha256:" + "0" * 64
        self.assert_code(stale_hash, "catalog.content_hash")

        no_items = copy.deepcopy(base)
        del no_items["runtime_export"]
        self.assert_code(no_items, "catalog.snapshot_shape")

        stale_export = copy.deepcopy(base)
        stale_export["runtime_export"]["items"][0]["level_count"] = 9
        self.assert_code(reseal(stale_export), "catalog.runtime_metadata")

        duplicate = copy.deepcopy(base)
        duplicate["items"].append(copy.deepcopy(self.item(duplicate, "game.gold")))
        self.assert_code(reseal(duplicate), "catalog.item_id")

    def test_rejects_currency_acquire_and_level_errors(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        wrong_kind = copy.deepcopy(base)
        self.item(wrong_kind, "game.metal")["currency"] = {"hud": "counter", "cap": 1}
        self.assert_code(reseal(wrong_kind), "catalog.currency")

        negative_cap = copy.deepcopy(base)
        self.item(negative_cap, "game.gold")["currency"]["cap"] = -1
        self.assert_code(reseal(negative_cap), "catalog.currency")

        bare_acquire = copy.deepcopy(base)
        self.item(bare_acquire, "game.fixed_sword")["acquire"] = {"__studio_kind": "free"}
        self.assert_code(reseal(bare_acquire), "catalog.acquire")

        unknown_member = copy.deepcopy(base)
        self.item(unknown_member, "game.fixed_sword")["levels"]["rows"][0]["defence"] = 1
        self.assert_code(reseal(unknown_member), "catalog.level_field")

        missing_member = copy.deepcopy(base)
        del self.item(missing_member, "game.fixed_sword")["levels"]["rows"][0]["attack"]
        self.assert_code(reseal(missing_member), "catalog.level_field")

        unknown_cost = copy.deepcopy(base)
        acquire = self.item(unknown_cost, "game.fixed_sword")["acquire"]
        acquire["cost"]["item"]["id"] = "game.absent"
        self.assert_code(reseal(unknown_cost), "catalog.acquire")

    def test_rejects_stackable_levels_and_storage_contradictions(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        levelled_stack = copy.deepcopy(base)
        gold = self.item(levelled_stack, "game.gold")
        gold["levels"] = {"mode": "single", "provenance": [{}], "rows": [{}]}
        next(entry for entry in levelled_stack["runtime_export"]["items"]
             if entry["id"] == "game.gold")["level_count"] = 1
        self.assert_code(reseal(levelled_stack), "catalog.levels")

        contradiction = copy.deepcopy(base)
        self.item(contradiction, "game.metal")["stack"] = 1
        self.assert_code(reseal(contradiction), "catalog.item")

    def test_rejects_budget_overflow(self):
        _, base = self.fixture("items_api_weapon_proof.json")
        with self.assertRaises(CatalogFailure) as raised:
            normalize(base, max_items=2)
        self.assertEqual("catalog.item_budget", str(raised.exception).partition(":")[0])
        with self.assertRaises(CatalogFailure) as raised:
            normalize(base, max_levels=1)
        self.assertEqual("catalog.level_budget", str(raised.exception).partition(":")[0])

    def test_write_if_different_keeps_mtime(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "generated.h"
            self.assertTrue(write_if_different(path, b"first"))
            stamp = path.stat().st_mtime_ns
            time.sleep(0.01)
            self.assertFalse(write_if_different(path, b"first"))
            self.assertEqual(stamp, path.stat().st_mtime_ns)
            self.assertTrue(write_if_different(path, b"second"))
            self.assertEqual(b"second", path.read_bytes())

    def test_fixtures_match_their_lua_sources(self):
        """The committed Snapshot fixtures stay reproducible from their Lua."""
        for name in ("items_api_core_proof", "items_api_weapon_proof"):
            with self.subTest(fixture=name), tempfile.TemporaryDirectory() as temp:
                evaluation = Path(temp) / "evaluation.json"
                rebuilt = Path(temp) / "snapshot.json"
                evaluation.write_bytes(subprocess.run(
                    [sys.executable, str(SCRIPT_DIR / "items_lua_sandbox.py"), "evaluate",
                     "--root", str(FIXTURES), "--manifest", str(FIXTURES / f"{name}.lua.json")],
                    check=True, capture_output=True,
                ).stdout)
                subprocess.run(
                    [sys.executable, str(SCRIPT_DIR / "items_snapshot.py"), "build",
                     "--evaluation", str(evaluation), "--out", str(rebuilt)],
                    check=True, capture_output=True,
                )
                self.assertEqual(
                    json.loads((FIXTURES / f"{name}.json").read_text(encoding="utf-8")),
                    json.loads(rebuilt.read_text(encoding="utf-8")),
                )


if __name__ == "__main__":
    unittest.main()
