import copy
import importlib.util
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("items_snapshot.py")
SPEC = importlib.util.spec_from_file_location("items_snapshot", SCRIPT)
SNAPSHOT = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(SNAPSHOT)


def attack_field():
    return {
        "id": "game.weapon.level.attack",
        "member": "attack",
        "section": "level_row",
        "type": "i64",
        "required_for": ["weapon"],
        "min": 0,
        "max": 1_000_000,
        "unit": "damage",
        "rounding": "exact",
        "label_key": "item.attack",
    }


def evaluation(items, fields=None):
    fields = [attack_field()] if fields is None else fields
    source_lines = {
        item_id: index + 3
        for index, item_id in enumerate(sorted(item["id"] for item in items))
    }
    return {
        "schema": "items.lua.evaluation.v1",
        "backend": {"module": "lupa.lua54", "version": "5.4"},
        "fields": fields,
        "field_sources": {
            field["id"]: {
                "file": "game/schema.lua",
                "line": index + 5,
                "column": 1,
                "kind": "field",
            }
            for index, field in enumerate(fields)
        },
        "items": items,
        "sources": {
            item["id"]: {
                "file": "game/items.lua",
                "line": source_lines[item["id"]],
                "column": 1,
                "kind": "definition",
            }
            for item in items
        },
    }


class ItemsSnapshotTests(unittest.TestCase):
    def base_items(self):
        gold = {
            "id": "game.gold", "kind": "currency", "stack": 0,
            "authoring_mode": "none",
        }
        sword = {
            "id": "game.sword",
            "kind": "weapon",
            "stack": 1,
            "authoring_mode": "table",
            "acquire": {
                "cost": {
                    "__studio_kind": "cost",
                    "count": 100,
                    "item": {"__studio_kind": "item_ref", "id": "game.gold"},
                }
            },
            "levels": {
                "mode": "table",
                "provenance": [
                    {"attack": "table"},
                    {"attack": "table", "cost_to_reach": "table"},
                    {"attack": "table", "cost_to_reach": "table"},
                ],
                "rows": [
                    {"attack": 10},
                    {
                        "attack": 15,
                        "cost_to_reach": {
                            "__studio_kind": "cost",
                            "count": 50,
                            "item": {"__studio_kind": "item_ref", "id": "game.gold"},
                        },
                    },
                    {"attack": 20, "cost_to_reach": {"__studio_kind": "free"}},
                ],
            },
        }
        return [gold, sword]

    def test_build_is_canonical_and_derives_actual_dependencies(self):
        items = self.base_items()
        first = SNAPSHOT.build_snapshot(evaluation(items))
        second = SNAPSHOT.build_snapshot(evaluation(list(reversed(copy.deepcopy(items)))))

        self.assertEqual(first, second)
        self.assertEqual(first["schema"], "items.snapshot.v1")
        self.assertRegex(first["content_hash"], r"^sha256:[0-9a-f]{64}$")
        self.assertEqual(first["evaluator"], {"module": "lupa.lua54", "version": "5.4"})
        self.assertEqual([item["id"] for item in first["items"]], ["game.gold", "game.sword"])
        self.assertEqual(first["dependencies"], {"game.gold": [], "game.sword": ["game.gold"]})
        self.assertEqual(first["dependents"], {"game.gold": ["game.sword"], "game.sword": []})
        self.assertEqual(first["sources"]["game.gold"]["file"], "game/items.lua")
        self.assertEqual(first["fields"][0]["id"], "game.weapon.level.attack")
        self.assertEqual(first["field_sources"]["game.weapon.level.attack"]["file"], "game/schema.lua")

        moved = evaluation(copy.deepcopy(items))
        moved["sources"]["game.gold"]["line"] = 300
        moved["field_sources"]["game.weapon.level.attack"]["line"] = 400
        self.assertEqual(
            SNAPSHOT.build_snapshot(moved)["content_hash"],
            first["content_hash"],
        )

    def test_typed_level_fields_enforce_kind_presence_type_and_range(self):
        no_levels = evaluation(self.base_items())
        del no_levels["items"][1]["levels"]
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.required_field"):
            SNAPSHOT.build_snapshot(no_levels)

        empty_levels = evaluation(self.base_items())
        empty_levels["items"][1]["levels"]["rows"] = []
        empty_levels["items"][1]["levels"]["provenance"] = []
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.required_field"):
            SNAPSHOT.build_snapshot(empty_levels)

        missing = evaluation(self.base_items())
        del missing["items"][1]["levels"]["rows"][0]["attack"]
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.required_field"):
            SNAPSHOT.build_snapshot(missing)

        wrong_type = evaluation(self.base_items())
        wrong_type["items"][1]["levels"]["rows"][0]["attack"] = 1.5
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.field_type"):
            SNAPSHOT.build_snapshot(wrong_type)

        out_of_range = evaluation(self.base_items())
        out_of_range["items"][1]["levels"]["rows"][0]["attack"] = 1_000_001
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.field_range"):
            SNAPSHOT.build_snapshot(out_of_range)

        wrong_kind = evaluation(self.base_items())
        wrong_kind["items"][0]["authoring_mode"] = "single"
        wrong_kind["items"][0]["levels"] = {
            "mode": "single", "rows": [{"attack": 1}],
            "provenance": [{"attack": "single"}],
        }
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.field_kind"):
            SNAPSHOT.build_snapshot(wrong_kind)

        malformed_source = evaluation(self.base_items())
        malformed_source["field_sources"]["game.weapon.level.attack"]["kind"] = "definition"
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.field_source"):
            SNAPSHOT.build_snapshot(malformed_source)

        unknown = evaluation(self.base_items())
        unknown["items"][1]["levels"]["rows"][0]["damage"] = 2
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.unknown_field"):
            SNAPSHOT.build_snapshot(unknown)

        sealed = evaluation(self.base_items())
        sealed["fields"][0]["id"] = "items.weapon.level.attack"
        sealed["field_sources"] = {
            "items.weapon.level.attack": sealed["field_sources"].pop("game.weapon.level.attack"),
        }
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.sealed_field_id"):
            SNAPSHOT.build_snapshot(sealed)

    def test_query_selects_one_item_field_and_level_range(self):
        snapshot = SNAPSHOT.build_snapshot(evaluation(self.base_items()))
        result = SNAPSHOT.query_snapshot(
            snapshot,
            item_id="game.sword",
            field="attack",
            level_from=2,
            level_to=3,
            include_inputs=True,
            include_dependents=True,
        )

        self.assertEqual(result, {
            "schema": "items.snapshot.query.v1",
            "content_hash": snapshot["content_hash"],
            "item": {
                "id": "game.sword",
                "levels": [
                    {
                        "level": 2, "values": {"attack": 15},
                        "provenance": {"attack": "table"},
                    },
                    {
                        "level": 3, "values": {"attack": 20},
                        "provenance": {"attack": "table"},
                    },
                ],
            },
            "inputs": ["game.gold"],
            "dependents": [],
            "source": {
                "file": "game/items.lua",
                "line": 4,
                "column": 1,
                "kind": "definition",
            },
            "field": {
                "schema": attack_field(),
                "source": {
                    "file": "game/schema.lua",
                    "line": 5,
                    "column": 1,
                    "kind": "field",
                },
            },
        })

        item_only = SNAPSHOT.query_snapshot(snapshot, item_id="game.gold")
        self.assertEqual(item_only["item"], {
            "id": "game.gold",
            "values": {"authoring_mode": "none", "kind": "currency", "stack": 0},
        })

        top_level_items = self.base_items()
        top_level_items[0]["attack"] = 7
        top_level = SNAPSHOT.query_snapshot(
            SNAPSHOT.build_snapshot(evaluation(top_level_items)),
            item_id="game.gold", field="attack",
        )
        self.assertEqual(top_level["item"]["values"], {"attack": 7})
        self.assertNotIn("field", top_level)

        transition = SNAPSHOT.query_snapshot(
            snapshot, item_id="game.sword", field="cost_to_reach",
            level_from=2, level_to=2,
        )
        self.assertEqual(
            transition["item"]["levels"][0]["provenance"],
            {"cost_to_reach": "table"},
        )
        self.assertNotIn("field", transition)

    def test_level_provenance_is_complete_and_mode_consistent(self):
        missing = evaluation(self.base_items())
        del missing["items"][1]["levels"]["provenance"][0]["attack"]
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.provenance"):
            SNAPSHOT.build_snapshot(missing)

        extra = evaluation(self.base_items())
        extra["items"][1]["levels"]["provenance"][0]["damage"] = "table"
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.provenance"):
            SNAPSHOT.build_snapshot(extra)

        inconsistent = evaluation(self.base_items())
        inconsistent["items"][1]["levels"]["provenance"][0]["attack"] = "override"
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.provenance"):
            SNAPSHOT.build_snapshot(inconsistent)

        malformed = evaluation(self.base_items())
        malformed["items"][1]["levels"]["provenance"][0]["attack"] = ["table"]
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.provenance"):
            SNAPSHOT.build_snapshot(malformed)

        wrong_mode = evaluation(self.base_items())
        wrong_mode["items"][1]["authoring_mode"] = "generate"
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.authoring_mode"):
            SNAPSHOT.build_snapshot(wrong_mode)

        multiple_single = evaluation(self.base_items())
        multiple_single["items"][1]["authoring_mode"] = "single"
        multiple_single["items"][1]["levels"]["mode"] = "single"
        multiple_single["items"][1]["levels"]["provenance"] = [
            {key: "single" for key in row}
            for row in multiple_single["items"][1]["levels"]["rows"]
        ]
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.authoring_mode"):
            SNAPSHOT.build_snapshot(multiple_single)

    def test_non_finite_values_and_unknown_references_are_rejected(self):
        bad_number = self.base_items()
        bad_number[1]["levels"]["rows"][0]["attack"] = math.inf
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.non_finite"):
            SNAPSHOT.build_snapshot(evaluation(bad_number))

        bad_reference = self.base_items()
        bad_reference[1]["acquire"]["cost"]["item"]["id"] = "game.missing"
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.unknown_reference"):
            SNAPSHOT.build_snapshot(evaluation(bad_reference))

        bad_source = evaluation(self.base_items())
        bad_source["sources"]["game.gold"]["line"] = True
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.source"):
            SNAPSHOT.build_snapshot(bad_source)

        malformed_snapshot = SNAPSHOT.build_snapshot(evaluation(self.base_items()))
        malformed_snapshot["sources"]["game.gold"] = "not-a-source"
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "query.source"):
            SNAPSHOT.query_snapshot(malformed_snapshot, item_id="game.gold")

        missing_field_source = SNAPSHOT.build_snapshot(evaluation(self.base_items()))
        missing_field_source["field_sources"] = {}
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "query.field_source"):
            SNAPSHOT.query_snapshot(missing_field_source, item_id="game.sword", field="attack")

        missing_field_schema = SNAPSHOT.build_snapshot(evaluation(self.base_items()))
        missing_field_schema["fields"] = []
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "query.field_schema"):
            SNAPSHOT.query_snapshot(missing_field_schema, item_id="game.sword", field="attack")

        for key, value in (("id", "bad"), ("type", "f64"), ("min", 2_000_000)):
            malformed_field_schema = SNAPSHOT.build_snapshot(evaluation(self.base_items()))
            schema = malformed_field_schema["fields"][0]
            old_id = schema["id"]
            schema[key] = value
            if key == "id":
                malformed_field_schema["field_sources"] = {
                    value: malformed_field_schema["field_sources"].pop(old_id),
                }
            with self.subTest(key=key), self.assertRaisesRegex(
                SNAPSHOT.SnapshotFailure, "query.field_schema",
            ):
                SNAPSHOT.query_snapshot(
                    malformed_field_schema, item_id="game.sword", field="attack",
                )

        spoofed_provenance = SNAPSHOT.build_snapshot(evaluation(self.base_items()))
        spoofed_provenance["items"][1]["levels"]["provenance"][0]["attack"] = "generate"
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "query.provenance"):
            SNAPSHOT.query_snapshot(
                spoofed_provenance, item_id="game.sword", field="attack",
                level_from=1, level_to=1,
            )

    def test_query_requires_a_range_for_more_than_1000_levels(self):
        items = self.base_items()
        items[1]["levels"]["rows"] = [{"attack": level} for level in range(1, 1002)]
        items[1]["levels"]["provenance"] = [
            {"attack": "table"} for _ in range(1, 1002)
        ]
        snapshot = SNAPSHOT.build_snapshot(evaluation(items))

        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "query.row_limit"):
            SNAPSHOT.query_snapshot(snapshot, item_id="game.sword")

        bounded = SNAPSHOT.query_snapshot(
            snapshot, item_id="game.sword", level_from=1000, level_to=1001,
        )
        self.assertEqual([row["level"] for row in bounded["item"]["levels"]], [1000, 1001])

        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "query.level_range"):
            SNAPSHOT.query_snapshot(snapshot, item_id="game.sword", level_from=1001, level_to=1002)

    def test_chart_query_is_bounded_and_discloses_downsampling(self):
        items = self.base_items()
        items[1]["levels"]["rows"] = [{"attack": level} for level in range(1, 1002)]
        items[1]["levels"]["provenance"] = [
            {"attack": "table"} for _ in range(1, 1002)
        ]
        snapshot = SNAPSHOT.build_snapshot(evaluation(items))

        chart = SNAPSHOT.chart_snapshot(
            snapshot, item_id="game.sword", field="attack", max_points=3,
        )

        self.assertEqual(chart["schema"], "items.snapshot.chart.v1")
        self.assertEqual([point["level"] for point in chart["points"]], [1, 501, 1001])
        self.assertEqual(chart["bounds"], {
            "level_from": 1, "level_to": 1001,
            "value_min": 1, "value_max": 1001,
        })
        self.assertEqual(chart["downsampling"], {
            "applied": True,
            "method": "even-index",
            "source_points": 1001,
            "returned_points": 3,
            "max_points": 3,
        })
        self.assertEqual(chart["field"]["schema"]["unit"], "damage")
        self.assertEqual(chart["points"][0]["provenance"], "table")

        ranged = SNAPSHOT.chart_snapshot(
            snapshot, item_id="game.sword", field="attack",
            level_from=100, level_to=200, max_points=3,
        )
        self.assertEqual([point["level"] for point in ranged["points"]], [100, 150, 200])
        self.assertEqual(ranged["bounds"]["value_min"], 100)
        self.assertEqual(ranged["bounds"]["value_max"], 200)

        full = SNAPSHOT.chart_snapshot(
            SNAPSHOT.build_snapshot(evaluation(self.base_items())),
            item_id="game.sword", field="attack", max_points=10,
        )
        self.assertFalse(full["downsampling"]["applied"])
        self.assertEqual(full["downsampling"]["method"], "none")

        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "chart.max_points"):
            SNAPSHOT.chart_snapshot(snapshot, item_id="game.sword", field="attack", max_points=1)
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "chart.field"):
            SNAPSHOT.chart_snapshot(
                SNAPSHOT.build_snapshot(evaluation(self.base_items())),
                item_id="game.sword", field="cost_to_reach", level_from=2,
            )

        stale = SNAPSHOT.build_snapshot(evaluation(self.base_items()))
        stale["items"][1]["levels"]["rows"][0]["attack"] = 1_000_001
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "chart.field"):
            SNAPSHOT.chart_snapshot(
                stale, item_id="game.sword", field="attack",
                level_from=1, level_to=1,
            )

    def test_diff_reports_only_semantic_changes_in_stable_order(self):
        before = SNAPSHOT.build_snapshot(evaluation(self.base_items()))
        changed_items = self.base_items()
        changed_items[1]["levels"]["rows"][1]["attack"] = 16
        after_evaluation = evaluation(changed_items)
        after_evaluation["sources"]["game.sword"]["line"] = 400
        after = SNAPSHOT.build_snapshot(after_evaluation)

        result = SNAPSHOT.diff_snapshots(before, after)

        self.assertEqual(result, {
            "schema": "items.snapshot.diff.v1",
            "before_hash": before["content_hash"],
            "after_hash": after["content_hash"],
            "changes": [{
                "op": "replace",
                "item": "game.sword",
                "path": "/levels/rows/1/attack",
                "before": 15,
                "after": 16,
            }],
        })

        source_only = copy.deepcopy(before)
        source_only["sources"]["game.sword"]["line"] = 999
        self.assertEqual(SNAPSHOT.diff_snapshots(before, source_only)["changes"], [])

        typed_items = self.base_items()
        typed_items[1]["stack"] = True
        typed = SNAPSHOT.build_snapshot(evaluation(typed_items))
        self.assertEqual(SNAPSHOT.diff_snapshots(before, typed)["changes"], [{
            "op": "replace", "item": "game.sword", "path": "/stack",
            "before": 1, "after": True,
        }])

        added_items = self.base_items() + [{
            "id": "game.z_potion", "kind": "consumable", "stack": 9,
            "authoring_mode": "none",
        }]
        added = SNAPSHOT.build_snapshot(evaluation(added_items))
        self.assertEqual(SNAPSHOT.diff_snapshots(before, added)["changes"][0]["path"], "")

    def test_diff_is_bounded(self):
        before_items = self.base_items()
        before_items[1]["levels"]["rows"] = [{"attack": level} for level in range(1001)]
        before_items[1]["levels"]["provenance"] = [
            {"attack": "table"} for _ in range(1001)
        ]
        after_items = copy.deepcopy(before_items)
        for row in after_items[1]["levels"]["rows"]:
            row["attack"] += 1

        before = SNAPSHOT.build_snapshot(evaluation(before_items))
        after = SNAPSHOT.build_snapshot(evaluation(after_items))
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "diff.change_limit"):
            SNAPSHOT.diff_snapshots(before, after)
        self.assertEqual(len(SNAPSHOT.diff_snapshots(before, after, max_changes=1001)["changes"]), 1001)

    def test_cli_build_query_and_diff_emit_bounded_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "evaluation.json"
            target = root / "snapshot.json"
            changed_source = root / "evaluation-changed.json"
            changed_target = root / "snapshot-changed.json"
            source.write_text(json.dumps(evaluation(self.base_items())), encoding="utf-8")

            built = subprocess.run(
                [sys.executable, str(SCRIPT), "build", "--evaluation", str(source), "--out", str(target)],
                text=True, capture_output=True, encoding="utf-8", timeout=10,
            )
            self.assertEqual(built.returncode, 0, built.stderr)
            queried = subprocess.run(
                [
                    sys.executable, str(SCRIPT), "query", "--snapshot", str(target),
                    "--item", "game.sword", "--field", "attack", "--level-from", "2",
                    "--level-to", "3", "--inputs",
                ],
                text=True, capture_output=True, encoding="utf-8", timeout=10,
            )
            self.assertEqual(queried.returncode, 0, queried.stderr)
            payload = json.loads(queried.stdout)
            self.assertEqual(len(payload["item"]["levels"]), 2)
            self.assertEqual(payload["inputs"], ["game.gold"])
            self.assertEqual(payload["field"]["schema"]["unit"], "damage")
            self.assertEqual(payload["field"]["source"]["kind"], "field")

            charted = subprocess.run(
                [
                    sys.executable, str(SCRIPT), "chart", "--snapshot", str(target),
                    "--item", "game.sword", "--field", "attack", "--max-points", "2",
                ],
                text=True, capture_output=True, encoding="utf-8", timeout=10,
            )
            self.assertEqual(charted.returncode, 0, charted.stderr)
            chart_payload = json.loads(charted.stdout)
            self.assertEqual(chart_payload["downsampling"]["returned_points"], 2)
            self.assertEqual([point["level"] for point in chart_payload["points"]], [1, 3])

            changed_items = self.base_items()
            changed_items[1]["levels"]["rows"][1]["attack"] = 16
            changed_source.write_text(json.dumps(evaluation(changed_items)), encoding="utf-8")
            built_changed = subprocess.run(
                [
                    sys.executable, str(SCRIPT), "build", "--evaluation",
                    str(changed_source), "--out", str(changed_target),
                ],
                text=True, capture_output=True, encoding="utf-8", timeout=10,
            )
            self.assertEqual(built_changed.returncode, 0, built_changed.stderr)
            diffed = subprocess.run(
                [
                    sys.executable, str(SCRIPT), "diff", "--before", str(target),
                    "--after", str(changed_target),
                ],
                text=True, capture_output=True, encoding="utf-8", timeout=10,
            )
            self.assertEqual(diffed.returncode, 0, diffed.stderr)
            self.assertEqual(json.loads(diffed.stdout)["changes"][0]["path"], "/levels/rows/1/attack")

    def test_malformed_snapshot_and_cli_arguments_are_structured_errors(self):
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "query.item"):
            SNAPSHOT.query_snapshot({
                "schema": "items.snapshot.v1",
                "items": ["not-an-item"],
            }, item_id="game.gold")

        result = subprocess.run(
            [
                sys.executable, str(SCRIPT), "query", "--snapshot", "unused.json",
                "--item", "game.gold", "--level-from", "not-an-integer",
            ],
            text=True, capture_output=True, encoding="utf-8", timeout=10,
        )
        self.assertEqual(result.returncode, 1)
        payload = json.loads(result.stderr)
        self.assertEqual(payload["schema"], "items.snapshot.error.v1")
        self.assertEqual(payload["error"]["code"], "cli.arguments")


if __name__ == "__main__":
    unittest.main()
