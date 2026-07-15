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


def evaluation(items):
    return {
        "schema": "items.lua.evaluation.v1",
        "backend": {"module": "lupa.lua54", "version": "5.4"},
        "items": items,
    }


class ItemsSnapshotTests(unittest.TestCase):
    def base_items(self):
        gold = {"id": "game.gold", "kind": "currency", "stack": 0}
        sword = {
            "id": "game.sword",
            "kind": "weapon",
            "stack": 1,
            "acquire": {
                "cost": {
                    "__studio_kind": "cost",
                    "count": 100,
                    "item": {"__studio_kind": "item_ref", "id": "game.gold"},
                }
            },
            "levels": {
                "mode": "table",
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
                    {"attack": 20},
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
                    {"level": 2, "values": {"attack": 15}},
                    {"level": 3, "values": {"attack": 20}},
                ],
            },
            "inputs": ["game.gold"],
            "dependents": [],
        })

        item_only = SNAPSHOT.query_snapshot(snapshot, item_id="game.gold")
        self.assertEqual(item_only["item"], {
            "id": "game.gold",
            "values": {"kind": "currency", "stack": 0},
        })

    def test_non_finite_values_and_unknown_references_are_rejected(self):
        bad_number = self.base_items()
        bad_number[1]["levels"]["rows"][0]["attack"] = math.inf
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.non_finite"):
            SNAPSHOT.build_snapshot(evaluation(bad_number))

        bad_reference = self.base_items()
        bad_reference[1]["acquire"]["cost"]["item"]["id"] = "game.missing"
        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "snapshot.unknown_reference"):
            SNAPSHOT.build_snapshot(evaluation(bad_reference))

    def test_query_requires_a_range_for_more_than_1000_levels(self):
        items = self.base_items()
        items[1]["levels"]["rows"] = [{"attack": level} for level in range(1, 1002)]
        snapshot = SNAPSHOT.build_snapshot(evaluation(items))

        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "query.row_limit"):
            SNAPSHOT.query_snapshot(snapshot, item_id="game.sword")

        bounded = SNAPSHOT.query_snapshot(
            snapshot, item_id="game.sword", level_from=1000, level_to=1001,
        )
        self.assertEqual([row["level"] for row in bounded["item"]["levels"]], [1000, 1001])

        with self.assertRaisesRegex(SNAPSHOT.SnapshotFailure, "query.level_range"):
            SNAPSHOT.query_snapshot(snapshot, item_id="game.sword", level_from=1001, level_to=1002)

    def test_cli_build_then_query_emits_bounded_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "evaluation.json"
            target = root / "snapshot.json"
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
