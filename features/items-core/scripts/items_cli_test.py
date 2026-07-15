#!/usr/bin/env python3
"""Focused contract tests for the single-source Items semantic CLI."""

from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


SCRIPT_DIR = Path(__file__).resolve().parent
SCRIPT = SCRIPT_DIR / "items_cli.py"
PROJECT = SCRIPT_DIR.parent / "tests" / "fixtures" / "items_cli"


class ItemsCliTests(unittest.TestCase):
    def run_cli(self, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(SCRIPT), "--project-root", str(PROJECT), *args],
            text=True, capture_output=True, encoding="utf-8", timeout=20,
        )

    def payload(self, *args: str) -> dict:
        result = self.run_cli(*args)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout)

    def test_list_is_bounded_and_returns_compact_runtime_identity(self):
        payload = self.payload("list")
        self.assertEqual(payload["schema"], "items.cli.result.v1")
        self.assertEqual(payload["operation"], "list")
        self.assertEqual([item["id"] for item in payload["result"]], [
            "game.gold", "game.iron_sword", "game.levelled_sword",
        ])
        self.assertEqual(payload["result"][0]["runtime"], {
            "storage": "stack", "level_count": 0,
        })

        bounded = self.run_cli("list", "--max-items", "2")
        self.assertEqual(bounded.returncode, 1)
        self.assertEqual(json.loads(bounded.stderr)["error"]["code"], "cli.result_limit")

    def test_inspect_dependencies_and_source_reuse_snapshot_queries(self):
        inspected = self.payload(
            "inspect", "--item", "game.levelled_sword",
            "--level-from", "2", "--level-to", "3",
        )
        levels = inspected["result"]["item"]["levels"]
        self.assertEqual([row["level"] for row in levels], [2, 3])
        self.assertEqual([row["values"]["attack"] for row in levels], [15, 20])

        dependencies = self.payload("dependencies", "--item", "game.levelled_sword")
        self.assertEqual(dependencies["result"], {
            "item": "game.levelled_sword",
            "inputs": ["game.gold"],
            "dependents": [],
        })
        invalid_bound = self.run_cli(
            "dependencies", "--item", "game.levelled_sword", "--max-related", "0",
        )
        self.assertEqual(json.loads(invalid_bound.stderr)["error"]["code"], "cli.max_related")

        source = self.payload("source", "--item", "game.levelled_sword")
        self.assertEqual(source["result"]["definition"]["file"], "game/items.lua")
        self.assertGreater(source["result"]["definition"]["line"], 1)

    def test_schema_and_validate_are_focused_snapshot_views(self):
        schema = self.payload("schema")
        self.assertEqual(schema["result"]["kinds"], ["currency", "weapon"])
        self.assertEqual([field["id"] for field in schema["result"]["fields"]], [
            "game.weapon.level.attack",
        ])

        validated = self.payload("validate")
        self.assertTrue(validated["result"]["ok"])
        self.assertEqual(validated["result"]["diagnostics"], [])
        self.assertTrue(validated["result"]["receipt"]["ok"])

    def test_chart_and_requirements_delegate_to_bounded_snapshot_reports(self):
        chart = self.payload(
            "chart", "--item", "game.levelled_sword", "--field", "attack",
            "--max-points", "2",
        )
        self.assertEqual(chart["result"]["schema"], "items.snapshot.chart.v1")
        self.assertEqual(chart["result"]["points"], [
            {"level": 1, "value": 10, "provenance": "table"},
            {"level": 3, "value": 20, "provenance": "table"},
        ])

        requirements = self.payload(
            "requirements", "--item", "game.levelled_sword", "--severity", "warning",
        )
        self.assertEqual(requirements["result"]["schema"], "items.snapshot.requirements.v1")
        self.assertEqual(requirements["result"]["results"], [])

    def test_build_validates_then_writes_snapshot_blob_and_stable_header(self):
        with tempfile.TemporaryDirectory() as tmp:
            first = self.payload("build", "--out-dir", tmp)
            self.assertTrue(first["result"]["ok"])
            self.assertEqual(first["result"]["changed"], {
                "snapshot": True, "blob": True, "header": True,
            })
            root = Path(tmp)
            self.assertTrue((root / "items.snapshot.json").is_file())
            self.assertTrue((root / "items.catalog").is_file())
            self.assertTrue((root / "items_catalog_abi.gen.h").is_file())

            second = self.payload("build", "--out-dir", tmp)
            self.assertEqual(second["result"]["changed"], {
                "snapshot": False, "blob": False, "header": False,
            })

    def test_build_refuses_receipt_failure_before_writing_outputs(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            project = root / "project"
            out = root / "out"
            shutil.copytree(PROJECT, project)
            lock_path = project / "content" / "items.lock.json"
            lock = json.loads(lock_path.read_text(encoding="utf-8"))
            lock["def_ids"]["game.removed"] = {"storage": "stack", "level_count": 0}
            lock_path.write_text(json.dumps(lock), encoding="utf-8")
            result = subprocess.run(
                [
                    sys.executable, str(SCRIPT), "--project-root", str(project),
                    "build", "--out-dir", str(out),
                ],
                text=True, capture_output=True, encoding="utf-8", timeout=20,
            )
            self.assertEqual(result.returncode, 1)
            payload = json.loads(result.stdout)
            self.assertFalse(payload["result"]["ok"])
            self.assertFalse(out.exists())

    def test_project_context_is_required_and_manifest_cannot_escape_it(self):
        missing = subprocess.run(
            [sys.executable, str(SCRIPT), "list"],
            text=True, capture_output=True, encoding="utf-8", timeout=10,
        )
        self.assertEqual(missing.returncode, 1)
        self.assertEqual(json.loads(missing.stderr)["error"]["code"], "cli.arguments")

        escaped = subprocess.run(
            [
                sys.executable, str(SCRIPT), "--project-root", str(PROJECT),
                "--manifest", "../outside.json", "list",
            ],
            text=True, capture_output=True, encoding="utf-8", timeout=10,
        )
        self.assertEqual(escaped.returncode, 1)
        self.assertEqual(json.loads(escaped.stderr)["error"]["code"], "cli.manifest")


if __name__ == "__main__":
    unittest.main()
