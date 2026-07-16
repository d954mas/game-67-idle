#!/usr/bin/env python3
"""Focused contract tests for the single-source Items semantic CLI."""

from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import items_cli as CLI


SCRIPT_DIR = Path(__file__).resolve().parent
SCRIPT = SCRIPT_DIR / "items_cli.py"
PROJECT = SCRIPT_DIR.parent / "tests" / "fixtures" / "items_cli"
TEMPLATE_ROOT = SCRIPT_DIR.parents[2] / "templates" / "template"


class ItemsCliTests(unittest.TestCase):
    def test_lua_cli_does_not_depend_on_legacy_json_ops(self):
        self.assertEqual(CLI.receipt_api.__name__, "items_receipt")

    def test_template_lua_matches_the_shipped_release_receipt(self):
        process = subprocess.run(
            [sys.executable, str(SCRIPT), "--project-root", str(TEMPLATE_ROOT), "validate"],
            text=True, capture_output=True, encoding="utf-8", timeout=20,
        )
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertTrue(json.loads(process.stdout)["result"]["receipt"]["ok"])

    def test_list_exposes_bounded_viewer_metadata_without_level_tables(self):
        process = subprocess.run(
            [sys.executable, str(SCRIPT), "--project-root", str(TEMPLATE_ROOT), "list"],
            text=True, capture_output=True, encoding="utf-8", timeout=20,
        )
        self.assertEqual(process.returncode, 0, process.stderr)
        items = json.loads(process.stdout)["result"]
        energy = next(item for item in items if item["id"] == "tmpl.energy")
        self.assertEqual(energy["name"], "Energy")
        self.assertEqual(energy["icon"], "icons/energy")
        self.assertEqual(energy["currency"], {"cap": 100, "hud": "counter"})
        self.assertEqual(energy["tags"], [])
        self.assertNotIn("levels", energy)
        self.assertNotIn("acquire", energy)

    def test_template_lua_builds_the_runtime_projection(self):
        with tempfile.TemporaryDirectory() as tmp:
            process = subprocess.run(
                [
                    sys.executable, str(SCRIPT), "--project-root", str(TEMPLATE_ROOT),
                    "build", "--out-dir", tmp,
                ],
                text=True, capture_output=True, encoding="utf-8", timeout=20,
            )
            self.assertEqual(process.returncode, 0, process.stderr)
            payload = json.loads(process.stdout)
            self.assertTrue(payload["result"]["ok"])
            inspected = CLI.package_api.inspect_package(
                (Path(tmp) / "items.catalog").read_bytes(),
            )
            self.assertEqual(inspected["sections"]["items"]["count"], 6)

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
            "game.curve_sword", "game.generated_sword", "game.gold", "game.iron_sword",
            "game.levelled_sword", "game.other_sword",
        ])
        gold = next(item for item in payload["result"] if item["id"] == "game.gold")
        self.assertEqual(gold["runtime"], {
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
        self.assertRegex(source["result"]["source_hash"], r"^sha256:[0-9a-f]{64}$")

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

        affected = self.payload(
            "validate", "--affected", "game.levelled_sword",
        )["result"]
        self.assertEqual(affected["affected"], {
            "item": "game.levelled_sword",
            "inputs": ["game.gold"],
            "dependents": [],
        })
        self.assertEqual(affected["diagnostics"], [])

    def test_affected_validation_explains_global_failures_without_materializing_levels(self):
        snapshot = {
            "items": [{"id": "game.large", "levels": {"rows": [{}] * 1001}}],
            "dependencies": {"game.large": []},
            "dependents": {"game.large": []},
        }
        global_failure = {
            "ok": False,
            "diagnostics": [{
                "severity": "error", "effective_status": "fail",
                "item": "game.unrelated",
            }],
            "receipt": {"ok": True, "errors": [], "warnings": []},
        }
        args = SimpleNamespace(
            baseline="content/items.lock.json",
            state_schema="state/items.schema.json",
            affected="game.large",
        )
        with (
            mock.patch.object(CLI, "_validation_paths", return_value=global_failure),
            mock.patch.object(
                CLI.snapshot_api, "query_requirements", return_value={"results": []},
            ),
        ):
            result = CLI._validation(PROJECT, {}, snapshot, args)
        self.assertFalse(result["ok"])
        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["requirements"], {
            "ok": False, "global_error_count": 1,
        })
        self.assertEqual(result["affected"]["item"], "game.large")

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

    def test_level_set_preview_apply_conflict_and_inverse_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            project = Path(tmp) / "project"
            shutil.copytree(PROJECT, project)
            source = project / "game" / "items.lua"
            original = source.read_bytes()
            source_payload = self.run_project(project, "source", "--item", "game.levelled_sword")
            source_hash = json.loads(source_payload.stdout)["result"]["source_hash"]
            args = (
                "level-set", "--item", "game.levelled_sword", "--level", "2",
                "--field", "attack", "--value", "17",
                "--expected-source-hash", source_hash,
            )

            preview = self.run_project(project, *args)
            self.assertEqual(preview.returncode, 0, preview.stderr)
            preview_result = json.loads(preview.stdout)["result"]
            self.assertFalse(preview_result["applied"])
            self.assertEqual(source.read_bytes(), original)
            self.assertEqual(preview_result["source_diff"]["old_value"], 15)
            self.assertEqual(preview_result["source_diff"]["new_value"], 17)
            self.assertTrue(preview_result["semantic_diff"]["changes"])

            lock = source.with_name(f".{source.name}.items-edit.lock")
            lock.write_text("stale test lock\n", encoding="utf-8")
            locked = self.run_project(project, *args, "--apply")
            self.assertEqual(locked.returncode, 1)
            self.assertEqual(json.loads(locked.stderr)["error"]["code"], "edit.locked")
            self.assertEqual(source.read_bytes(), original)
            lock.unlink()

            applied = self.run_project(project, *args, "--apply")
            self.assertEqual(applied.returncode, 0, applied.stderr)
            applied_result = json.loads(applied.stdout)["result"]
            self.assertTrue(applied_result["applied"])
            self.assertIn("attack = 17", source.read_text(encoding="utf-8"))

            conflict = self.run_project(project, *args, "--value", "18", "--apply")
            self.assertEqual(conflict.returncode, 1)
            self.assertEqual(json.loads(conflict.stderr)["error"]["code"], "edit.conflict")
            self.assertIn("attack = 17", source.read_text(encoding="utf-8"))

            inverse = applied_result["inverse_patch"]
            reverted = self.run_project(
                project, inverse["operation"],
                "--item", inverse["item"], "--level", str(inverse["level"]),
                "--field", inverse["field"], "--value", str(inverse["value"]),
                "--expected-source-hash", inverse["expected_source_hash"], "--apply",
            )
            self.assertEqual(reverted.returncode, 0, reverted.stderr)
            self.assertEqual(source.read_bytes(), original)

    def test_curve_and_existing_override_preview_use_same_edit_contract(self):
        source = self.payload("source", "--item", "game.curve_sword")["result"]
        common = ("--item", "game.curve_sword", "--field", "attack")
        curve = self.payload(
            "curve-set", *common, "--parameter", "step", "--value", "7",
            "--expected-source-hash", source["source_hash"],
        )
        self.assertEqual(curve["result"]["source_diff"]["old_value"], 5)
        override = self.payload(
            "override-set", *common, "--level", "3", "--value", "25",
            "--expected-source-hash", source["source_hash"],
        )
        self.assertEqual(override["result"]["source_diff"]["old_value"], 21)

    def test_max_level_truncate_apply_inverse_and_release_gate(self):
        with tempfile.TemporaryDirectory() as tmp:
            project = Path(tmp) / "project"
            shutil.copytree(PROJECT, project)
            source = project / "game" / "items.lua"
            original = source.read_bytes()
            source_hash = json.loads(self.run_project(
                project, "source", "--item", "game.generated_sword",
            ).stdout)["result"]["source_hash"]
            args = (
                "max-level-truncate", "--item", "game.generated_sword",
                "--to-level", "2", "--expected-source-hash", source_hash,
            )
            applied = self.run_project(project, *args, "--apply")
            self.assertEqual(applied.returncode, 0, applied.stderr)
            result = json.loads(applied.stdout)["result"]
            self.assertTrue(result["applied"])
            self.assertIn("max_level = 2", source.read_text(encoding="utf-8"))

            inverse = result["inverse_patch"]
            reverted = self.run_project(
                project, inverse["operation"], "--item", inverse["item"],
                "--to-level", str(inverse["to_level"]),
                "--expected-source-hash", inverse["expected_source_hash"], "--apply",
            )
            self.assertEqual(reverted.returncode, 0, reverted.stderr)
            self.assertEqual(source.read_bytes(), original)

            batch_path = project / "max-level.json"
            batch_path.write_text(json.dumps({
                "schema": "items.cli.patch_batch.v1",
                "expected_source_hash": source_hash,
                "operations": [{
                    "operation": "max-level-truncate",
                    "item": "game.generated_sword",
                    "to_level": 2,
                }],
            }), encoding="utf-8")
            batch = self.run_project(
                project, "batch", "--patch-file", str(batch_path),
            )
            self.assertEqual(batch.returncode, 0, batch.stderr)
            batch_result = json.loads(batch.stdout)["result"]
            self.assertEqual(
                batch_result["inverse_patch"]["operations"][0]["operation"],
                "max-level-append",
            )
            self.assertEqual(source.read_bytes(), original)

            lock_path = project / "content" / "items.lock.json"
            lock = json.loads(lock_path.read_text(encoding="utf-8"))
            lock["def_ids"]["game.generated_sword"]["level_count"] = 3
            lock_path.write_text(json.dumps(lock), encoding="utf-8")
            blocked = self.run_project(project, *args)
            self.assertEqual(blocked.returncode, 1, blocked.stderr)
            blocked_result = json.loads(blocked.stdout)["result"]
            self.assertFalse(blocked_result["ok"])
            self.assertFalse(blocked_result["applied"])
            self.assertEqual(source.read_bytes(), original)

            table_hash = json.loads(self.run_project(
                project, "source", "--item", "game.levelled_sword",
            ).stdout)["result"]["source_hash"]
            refused = self.run_project(
                project, "max-level-append", "--item", "game.levelled_sword",
                "--to-level", "4", "--expected-source-hash", table_hash,
            )
            self.assertEqual(refused.returncode, 1)
            self.assertEqual(json.loads(refused.stderr)["error"]["code"], "edit.source_shape")

    def test_same_file_batch_apply_inverse_and_multi_file_refusal(self):
        with tempfile.TemporaryDirectory() as tmp:
            project = Path(tmp) / "project"
            shutil.copytree(PROJECT, project)
            source = project / "game" / "items.lua"
            original = source.read_bytes()
            source_hash = json.loads(self.run_project(
                project, "source", "--item", "game.levelled_sword",
            ).stdout)["result"]["source_hash"]
            patch_path = project / "batch.json"
            patch_path.write_text(json.dumps({
                "schema": "items.cli.patch_batch.v1",
                "expected_source_hash": source_hash,
                "operations": [
                    {
                        "operation": "level-set", "item": "game.levelled_sword",
                        "level": 1, "field": "attack", "value": 11,
                    },
                    {
                        "operation": "level-set", "item": "game.levelled_sword",
                        "level": 2, "field": "attack", "value": 17,
                    },
                ],
            }), encoding="utf-8")
            preview = self.run_project(project, "batch", "--patch-file", str(patch_path))
            self.assertEqual(preview.returncode, 0, preview.stderr)
            self.assertEqual(source.read_bytes(), original)
            self.assertEqual(len(json.loads(preview.stdout)["result"]["source_diff"]["edits"]), 2)

            applied = self.run_project(
                project, "batch", "--patch-file", str(patch_path), "--apply",
            )
            self.assertEqual(applied.returncode, 0, applied.stderr)
            applied_result = json.loads(applied.stdout)["result"]
            self.assertIn("[1] = { attack = 11 }", source.read_text(encoding="utf-8"))
            self.assertIn("[2] = { attack = 17", source.read_text(encoding="utf-8"))

            inverse_path = project / "inverse.json"
            inverse_path.write_text(
                json.dumps(applied_result["inverse_patch"]), encoding="utf-8",
            )
            reverted = self.run_project(
                project, "batch", "--patch-file", str(inverse_path), "--apply",
            )
            self.assertEqual(reverted.returncode, 0, reverted.stderr)
            self.assertEqual(source.read_bytes(), original)

            cross_file = project / "cross-file.json"
            cross_file.write_text(json.dumps({
                "schema": "items.cli.patch_batch.v1",
                "expected_source_hash": source_hash,
                "operations": [
                    {
                        "operation": "level-set", "item": "game.levelled_sword",
                        "level": 1, "field": "attack", "value": 11,
                    },
                    {
                        "operation": "level-set", "item": "game.other_sword",
                        "level": 1, "field": "attack", "value": 31,
                    },
                ],
            }), encoding="utf-8")
            refused = self.run_project(
                project, "batch", "--patch-file", str(cross_file), "--apply",
            )
            self.assertEqual(refused.returncode, 1)
            self.assertEqual(json.loads(refused.stderr)["error"]["code"], "edit.multi_file")

    def test_batch_loader_rechecks_actual_bytes_after_size_probe(self):
        with tempfile.TemporaryDirectory() as tmp:
            patch = Path(tmp) / "oversized.json"
            patch.write_bytes(b" " * (CLI.MAX_PATCH_BYTES + 1))
            with mock.patch.object(Path, "stat", return_value=mock.Mock(st_size=0)):
                with self.assertRaisesRegex(CLI.CliFailure, "patch exceeds"):
                    CLI._load_batch_patch(str(patch))

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

    def run_project(self, project: Path, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(SCRIPT), "--project-root", str(project), *args],
            text=True, capture_output=True, encoding="utf-8", timeout=20,
        )


if __name__ == "__main__":
    unittest.main()
