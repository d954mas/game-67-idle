#!/usr/bin/env python3
"""Tests for the Snapshot-only compact Items runtime package."""

from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import items_runtime_package as PACKAGE  # noqa: E402
import items_snapshot as SNAPSHOT  # noqa: E402


def source(line: int, kind: str = "definition") -> dict:
    snippet = "items.define({" if kind == "definition" else "field.i64({"
    return {
        "file": "game/items.lua", "line": line, "column": 1,
        "end_line": line, "end_column": len(snippet) + 1,
        "snippet": snippet, "kind": kind,
    }


def evaluation() -> dict:
    attack_field = {
        "id": "game.weapon.level.attack", "member": "attack",
        "section": "level_row", "type": "i64", "required_for": ["weapon"],
        "min": 0, "max": 1_000_000, "rounding": "exact",
        "unit": "damage", "label_key": "item.attack",
    }
    gold_ref = {"__studio_kind": "item_ref", "id": "game.gold"}
    cost = {"__studio_kind": "cost", "item": gold_ref, "count": 100}
    return {
        "schema": "items.lua.evaluation.v1",
        "fields": [attack_field],
        "field_sources": {attack_field["id"]: source(1, "field")},
        "items": [
            {
                "id": "game.gold", "kind": "currency", "stack": 0,
                "currency": {"hud": "counter", "cap": 100},
                "authoring_mode": "none", "acquire": {"cost": {"__studio_kind": "free"}},
            },
            {
                "id": "game.sword", "kind": "weapon", "stack": 1,
                "authoring_mode": "table", "acquire": {"cost": cost},
                "levels": {
                    "mode": "table",
                    "rows": [
                        {"attack": 10},
                        {"attack": 15, "cost_to_reach": cost},
                        {"attack": 21, "cost_to_reach": {"__studio_kind": "free"}},
                    ],
                    "provenance": [
                        {"attack": "table"},
                        {"attack": "table", "cost_to_reach": "table"},
                        {"attack": "table", "cost_to_reach": "table"},
                    ],
                },
            },
        ],
        "sources": {"game.gold": source(10), "game.sword": source(20)},
        "requirements": [], "requirement_sources": {}, "waiver_sources": {},
    }


class ItemsRuntimePackageTests(unittest.TestCase):
    def snapshot(self) -> dict:
        return SNAPSHOT.build_snapshot(evaluation())

    @staticmethod
    def rehash(snapshot: dict) -> dict:
        snapshot["content_hash"] = SNAPSHOT.snapshot_content_hash(snapshot)
        return snapshot

    @staticmethod
    def resign(package: bytearray) -> bytes:
        struct.pack_into("<Q", package, PACKAGE.CONTENT_FINGERPRINT_OFFSET, 0)
        struct.pack_into(
            "<Q", package, PACKAGE.CONTENT_FINGERPRINT_OFFSET,
            PACKAGE.xxh64(bytes(package)),
        )
        return bytes(package)

    def test_builds_deterministic_flat_little_endian_package(self):
        snapshot = self.snapshot()
        first = PACKAGE.build_package(snapshot)
        second = PACKAGE.build_package(copy.deepcopy(snapshot))
        self.assertEqual(first, second)

        inspected = PACKAGE.inspect_package(first)
        self.assertEqual(inspected["schema"], "items.runtime.package.inspect.v2")
        self.assertEqual(inspected["snapshot_content_hash"], snapshot["content_hash"])
        self.assertEqual(inspected["sections"], {
            "strings": {"count": 77, "stride": 1},
            "items": {"count": 2, "stride": 56},
            "fields": {"count": 1, "stride": 48},
            "levels": {"count": 3, "stride": 32},
            "values": {"count": 3, "stride": 24},
            "costs": {"count": 2, "stride": 16},
        })
        self.assertEqual(inspected["items"], [
            {
                "id": "game.gold", "kind": "currency", "storage": "stack",
                "stack": 0, "level_count": 0, "acquire_costs": [], "acquire_free": True,
                "currency": {"cap": 100},
            },
            {
                "id": "game.sword", "kind": "weapon", "storage": "unique",
                "stack": 1, "level_count": 3,
                "acquire_costs": [{"item": "game.gold", "count": 100}],
            },
        ])
        self.assertEqual(inspected["levels"], [
            {"item": "game.sword", "level": 1, "values": {"game.weapon.level.attack": 10}},
            {
                "item": "game.sword", "level": 2,
                "values": {"game.weapon.level.attack": 15},
                "costs": [{"item": "game.gold", "count": 100}],
            },
            {
                "item": "game.sword", "level": 3,
                "values": {"game.weapon.level.attack": 21}, "free": True,
            },
        ])

    def test_native_fixture_content_hash_matches_snapshot_payload(self):
        fixture_path = SCRIPT_DIR.parent / "tests" / "fixtures" / "items_runtime_snapshot_v1.json"
        fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
        payload = {
            key: fixture[key]
            for key in ("schema", "fields", "items", "requirements")
        }
        digest = hashlib.sha256(json.dumps(
            payload, ensure_ascii=False, sort_keys=True, separators=(",", ":"),
        ).encode("utf-8")).hexdigest()
        self.assertEqual(fixture["content_hash"], f"sha256:{digest}")

    def test_rejects_stale_snapshot_content_hash_at_public_boundaries(self):
        snapshot = self.snapshot()
        package = PACKAGE.build_package(snapshot)
        header = PACKAGE.render_abi_header(snapshot).encode("utf-8")
        stale = copy.deepcopy(snapshot)
        stale["items"][1]["levels"]["rows"][0]["attack"] = 999

        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.content_hash"):
            PACKAGE.build_package(stale)
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.content_hash"):
            PACKAGE.render_abi_header(stale)
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.content_hash"):
            PACKAGE.verify_publication(stale, package, header)

    def test_rejects_corruption_collisions_and_default_budget_overflow(self):
        snapshot = self.snapshot()
        package = bytearray(PACKAGE.build_package(snapshot))
        package[-1] ^= 1
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.content_fingerprint"):
            PACKAGE.inspect_package(bytes(package))

        with mock.patch.object(PACKAGE, "xxh64", return_value=1):
            with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.item_hash_collision"):
                PACKAGE.build_package(snapshot)

        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.level_budget"):
            PACKAGE.build_package(snapshot, max_levels=2)
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.byte_budget"):
            PACKAGE.build_package(snapshot, max_bytes=64)

        for count in (0, -1):
            with self.subTest(cost_count=count):
                invalid_cost = self.snapshot()
                invalid_cost["items"][1]["acquire"]["cost"]["count"] = count
                with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.cost_count"):
                    PACKAGE.build_package(self.rehash(invalid_cost))

        invalid_currency = self.snapshot()
        invalid_currency["items"][0]["kind"] = "material"
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.currency"):
            PACKAGE.build_package(self.rehash(invalid_currency))

    def test_inspector_rejects_noncanonical_ownership_and_field_metadata(self):
        original = PACKAGE.build_package(self.snapshot())
        header = PACKAGE.HEADER.unpack_from(original)

        wrong_owner = bytearray(original)
        item_offset = header[10]
        gold = list(PACKAGE.ITEM.unpack_from(wrong_owner, item_offset))
        gold[6] = 3
        PACKAGE.ITEM.pack_into(wrong_owner, item_offset, *gold)
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.span"):
            PACKAGE.inspect_package(self.resign(wrong_owner))

        overlapping_values = bytearray(original)
        level_offset = header[16]
        second = list(PACKAGE.LEVEL.unpack_from(overlapping_values, level_offset + PACKAGE.LEVEL.size))
        second[2] = 0
        PACKAGE.LEVEL.pack_into(overlapping_values, level_offset + PACKAGE.LEVEL.size, *second)
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.span"):
            PACKAGE.inspect_package(self.resign(overlapping_values))

        field_offset = header[13]
        for index, replacement in ((2, 0), (7, 0)):
            with self.subTest(field_word=index):
                malformed = bytearray(original)
                field = list(PACKAGE.FIELD.unpack_from(malformed, field_offset))
                field[index] = replacement
                PACKAGE.FIELD.pack_into(malformed, field_offset, *field)
                with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.field|package.string"):
                    PACKAGE.inspect_package(self.resign(malformed))

        invalid_item_flag = bytearray(original)
        gold = list(PACKAGE.ITEM.unpack_from(invalid_item_flag, item_offset))
        gold[9] = 4
        PACKAGE.ITEM.pack_into(invalid_item_flag, item_offset, *gold)
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.item"):
            PACKAGE.inspect_package(self.resign(invalid_item_flag))

        free_with_cost = bytearray(original)
        sword_offset = item_offset + PACKAGE.ITEM.size
        sword = list(PACKAGE.ITEM.unpack_from(free_with_cost, sword_offset))
        sword[9] = PACKAGE.ITEM_ACQUIRE_FREE
        PACKAGE.ITEM.pack_into(free_with_cost, sword_offset, *sword)
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.item"):
            PACKAGE.inspect_package(self.resign(free_with_cost))

    def test_row_and_byte_budgets_fail_before_materialization(self):
        snapshot = self.snapshot()
        del snapshot["items"][0]["acquire"]
        del snapshot["items"][1]["acquire"]
        with mock.patch.object(PACKAGE, "_cost_values", side_effect=AssertionError("traversed rows")):
            with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.level_budget"):
                PACKAGE.build_package(self.rehash(snapshot), max_levels=2)

        with mock.patch.object(PACKAGE, "_materialize_sections", side_effect=AssertionError("materialized")):
            with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.byte_budget"):
                PACKAGE.build_package(self.snapshot(), max_bytes=64)

    def test_export_requires_snapshot_and_never_accepts_evaluator_output(self):
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.snapshot_schema"):
            PACKAGE.build_package(evaluation())

    def test_generated_abi_header_is_value_stable_and_write_if_different(self):
        snapshot = self.snapshot()
        header = PACKAGE.render_abi_header(snapshot)
        changed = copy.deepcopy(snapshot)
        changed["items"][1]["levels"]["rows"][0]["attack"] = 999
        self.assertEqual(PACKAGE.render_abi_header(self.rehash(changed)), header)
        added = copy.deepcopy(snapshot)
        added["items"].append({
            "id": "game.wood", "kind": "material", "stack": 99,
            "authoring_mode": "none",
        })
        self.assertNotEqual(PACKAGE.render_abi_header(self.rehash(added)), header)
        self.assertIn("ITEMS_CATALOG_SCHEMA_ABI", header)
        self.assertIn("ITEMS_CATALOG_CAPABILITY_COUNT UINT32_C(1)", header)
        self.assertIn("ITEM_GAME_GOLD", header)
        self.assertIn("ITEM_FIELD_GAME_WEAPON_LEVEL_ATTACK", header)
        self.assertIn("ITEMS_GAME_HAS_WEAPON", header)
        self.assertIn("item_weapon_level_t", header)
        self.assertIn("items_weapon_level", header)

        core_only = copy.deepcopy(snapshot)
        core_only["fields"] = []
        core_header = PACKAGE.render_abi_header(self.rehash(core_only))
        self.assertIn("ITEMS_CATALOG_CAPABILITY_COUNT UINT32_C(0)", core_header)
        self.assertIn(
            "#if ITEMS_CATALOG_CAPABILITY_COUNT > 0U\n"
            "static bool items_catalog_internal_is_kind",
            core_header,
        )

        keyword = copy.deepcopy(snapshot)
        keyword["fields"][0]["member"] = "int"
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.field_id"):
            PACKAGE.render_abi_header(self.rehash(keyword))

        full_i64 = copy.deepcopy(snapshot)
        full_i64["fields"][0]["min"] = PACKAGE.I64_MIN
        full_i64["fields"][0]["max"] = PACKAGE.I64_MAX
        boundary_header = PACKAGE.render_abi_header(self.rehash(full_i64))
        self.assertIn("INT64_MIN", boundary_header)
        self.assertIn("INT64_MAX", boundary_header)
        self.assertNotIn("INT64_C(9223372036854775808)", boundary_header)

        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "items_catalog_abi.gen.h"
            self.assertTrue(PACKAGE.write_if_different(target, header.encode("utf-8")))
            first_mtime = target.stat().st_mtime_ns
            self.assertFalse(PACKAGE.write_if_different(target, header.encode("utf-8")))
            self.assertEqual(target.stat().st_mtime_ns, first_mtime)

    def test_cli_builds_blob_and_reports_noop_rebuild(self):
        snapshot = self.snapshot()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "snapshot.json"
            blob = root / "items.catalog"
            header = root / "items_catalog_abi.gen.h"
            source.write_text(json.dumps(snapshot), encoding="utf-8")
            command = [
                sys.executable, str(SCRIPT_DIR / "items_runtime_package.py"), "build",
                "--snapshot", str(source), "--out", str(blob), "--header-out", str(header),
            ]
            first = subprocess.run(command, text=True, capture_output=True, encoding="utf-8", timeout=10)
            self.assertEqual(first.returncode, 0, first.stderr)
            first_payload = json.loads(first.stdout)
            self.assertEqual(first_payload["schema"], "items.runtime.package.build.v1")
            self.assertEqual(first_payload["changed"], {"blob": True, "header": True})
            self.assertEqual(PACKAGE.inspect_package(blob.read_bytes())["snapshot_content_hash"], snapshot["content_hash"])

            second = subprocess.run(command, text=True, capture_output=True, encoding="utf-8", timeout=10)
            self.assertEqual(second.returncode, 0, second.stderr)
            self.assertEqual(json.loads(second.stdout)["changed"], {"blob": False, "header": False})

    def test_publication_verification_rejects_stale_blob_and_header(self):
        snapshot = self.snapshot()
        package = PACKAGE.build_package(snapshot)
        header = PACKAGE.render_abi_header(snapshot).encode("utf-8")
        verified = PACKAGE.verify_publication(snapshot, package, header)
        self.assertEqual(verified["snapshot_content_hash"], snapshot["content_hash"])

        changed_evaluation = evaluation()
        changed_evaluation["items"][1]["levels"]["rows"][0]["attack"] = 999
        changed_snapshot = SNAPSHOT.build_snapshot(changed_evaluation)
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.stale_blob"):
            PACKAGE.verify_publication(changed_snapshot, package, header)
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.stale_header"):
            PACKAGE.verify_publication(snapshot, package, header + b"\n")

    def test_cli_verifies_selected_outputs_against_current_snapshot(self):
        snapshot = self.snapshot()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "snapshot.json"
            blob = root / "items.catalog"
            header = root / "items_catalog_abi.gen.h"
            source.write_text(json.dumps(snapshot), encoding="utf-8")
            blob.write_bytes(PACKAGE.build_package(snapshot))
            header.write_bytes(PACKAGE.render_abi_header(snapshot).encode("utf-8"))
            command = [
                sys.executable, str(SCRIPT_DIR / "items_runtime_package.py"), "verify",
                "--snapshot", str(source), "--blob", str(blob), "--header", str(header),
            ]
            verified = subprocess.run(command, text=True, capture_output=True, encoding="utf-8", timeout=10)
            self.assertEqual(verified.returncode, 0, verified.stderr)
            self.assertEqual(json.loads(verified.stdout)["schema"], "items.runtime.package.verify.v1")

            stale_evaluation = evaluation()
            stale_evaluation["items"][1]["levels"]["rows"][0]["attack"] = 999
            source.write_text(json.dumps(SNAPSHOT.build_snapshot(stale_evaluation)), encoding="utf-8")
            stale = subprocess.run(command, text=True, capture_output=True, encoding="utf-8", timeout=10)
            self.assertEqual(stale.returncode, 1)
            self.assertIn("package.stale_blob", json.loads(stale.stderr)["error"])

    def test_cli_rejects_path_overlap_bad_arguments_and_non_utf8_as_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "snapshot.json"
            source.write_text(json.dumps(self.snapshot()), encoding="utf-8")
            script = str(SCRIPT_DIR / "items_runtime_package.py")

            overlap = subprocess.run([
                sys.executable, script, "build", "--snapshot", str(source),
                "--out", str(root / "same"), "--header-out", str(root / "same"),
            ], text=True, capture_output=True, encoding="utf-8", timeout=10)
            self.assertEqual(overlap.returncode, 1)
            self.assertEqual(json.loads(overlap.stderr)["schema"], "items.runtime.package.error.v1")
            self.assertFalse((root / "same").exists())

            original_source = source.read_bytes()
            input_overlap = subprocess.run([
                sys.executable, script, "build", "--snapshot", str(source),
                "--out", str(source), "--header-out", str(root / "header"),
            ], text=True, capture_output=True, encoding="utf-8", timeout=10)
            self.assertEqual(input_overlap.returncode, 1)
            self.assertEqual(source.read_bytes(), original_source)

            missing = subprocess.run(
                [sys.executable, script, "build"],
                text=True, capture_output=True, encoding="utf-8", timeout=10,
            )
            self.assertEqual(missing.returncode, 1)
            self.assertEqual(json.loads(missing.stderr)["schema"], "items.runtime.package.error.v1")

            invalid = root / "invalid.json"
            invalid.write_bytes(b"\xff")
            bad_encoding = subprocess.run([
                sys.executable, script, "build", "--snapshot", str(invalid),
                "--out", str(root / "blob"), "--header-out", str(root / "header"),
            ], text=True, capture_output=True, encoding="utf-8", timeout=10)
            self.assertEqual(bad_encoding.returncode, 1)
            self.assertEqual(json.loads(bad_encoding.stderr)["schema"], "items.runtime.package.error.v1")


if __name__ == "__main__":
    unittest.main()
