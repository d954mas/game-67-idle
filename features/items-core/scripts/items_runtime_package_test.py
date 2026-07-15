#!/usr/bin/env python3
"""Tests for the Snapshot-only compact Items runtime package."""

from __future__ import annotations

import copy
from pathlib import Path
import struct
import sys
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
            {"id": "game.gold", "kind": "currency", "stack": 0, "authoring_mode": "none"},
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
        self.assertEqual(inspected["schema"], "items.runtime.package.inspect.v1")
        self.assertEqual(inspected["snapshot_content_hash"], snapshot["content_hash"])
        self.assertEqual(inspected["sections"], {
            "strings": {"count": 77, "stride": 1},
            "items": {"count": 2, "stride": 48},
            "fields": {"count": 1, "stride": 48},
            "levels": {"count": 3, "stride": 32},
            "values": {"count": 3, "stride": 24},
            "costs": {"count": 2, "stride": 16},
        })
        self.assertEqual(inspected["items"], [
            {
                "id": "game.gold", "kind": "currency", "storage": "stack",
                "stack": 0, "level_count": 0, "acquire_costs": [],
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
                    PACKAGE.build_package(invalid_cost)

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

    def test_row_and_byte_budgets_fail_before_materialization(self):
        snapshot = self.snapshot()
        del snapshot["items"][1]["acquire"]
        with mock.patch.object(PACKAGE, "_cost_values", side_effect=AssertionError("traversed rows")):
            with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.level_budget"):
                PACKAGE.build_package(snapshot, max_levels=2)

        with mock.patch.object(PACKAGE, "_materialize_sections", side_effect=AssertionError("materialized")):
            with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.byte_budget"):
                PACKAGE.build_package(self.snapshot(), max_bytes=64)

    def test_export_requires_snapshot_and_never_accepts_evaluator_output(self):
        with self.assertRaisesRegex(PACKAGE.PackageFailure, "package.snapshot_schema"):
            PACKAGE.build_package(evaluation())


if __name__ == "__main__":
    unittest.main()
