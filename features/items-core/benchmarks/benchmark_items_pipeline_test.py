#!/usr/bin/env python3
"""Contract tests for the finished Items pipeline benchmark."""

from __future__ import annotations

import json
import os
from pathlib import Path
import sys
import unittest

import benchmark_items_pipeline as BENCHMARK


class ItemsPipelineBenchmarkTests(unittest.TestCase):
    def test_runner_records_wall_memory_and_exact_context_bytes(self):
        measured = BENCHMARK.measure_command([
            sys.executable,
            "-c",
            "import sys; print('ready'); print('note', file=sys.stderr)",
        ])
        self.assertEqual(measured["exit_code"], 0)
        self.assertGreater(measured["wall_ms"], 0)
        self.assertGreater(measured["peak_process_tree_rss_bytes"], 0)
        self.assertEqual(measured["stdout_bytes"], len(f"ready{os.linesep}".encode()))
        self.assertEqual(measured["stderr_bytes"], len(f"note{os.linesep}".encode()))

    def test_backend_ratification_requires_finished_pipeline_evidence(self):
        snapshot = {
            "evaluator": {"module": "lupa.lua54", "version": "5.4"},
            "items": [{"id": "tmpl.sword"}],
        }
        build = {"exit_code": 0, "result": {"ok": True}}
        noop = {
            "exit_code": 0,
            "result": {"ok": True, "changed": {"snapshot": False, "blob": False, "header": False}},
        }
        runtime = {"exit_code": 0, "result": {"bind_samples": 9, "steady_owned_bytes": 568}}
        decision = BENCHMARK.ratify_backend(snapshot, build, noop, runtime, lupa_version="2.8")
        self.assertEqual(decision["status"], "ratified")
        self.assertEqual(decision["backend"], "lupa.lua54")
        self.assertEqual(decision["package"], "lupa@2.8")
        self.assertEqual(decision["runtime_format"], "compact-blob-v2")

        runtime["result"]["bind_samples"] = 0
        self.assertEqual(
            BENCHMARK.ratify_backend(snapshot, build, noop, runtime, lupa_version="2.8")["status"],
            "unresolved",
        )

    def test_conflict_diagnostic_quality_is_explicit_and_actionable(self):
        payload = {
            "schema": "items.cli.error.v1",
            "error": {
                "code": "edit.conflict",
                "message": "source hash changed",
                "expected": "sha256:old",
                "actual": "sha256:new",
            },
        }
        quality = BENCHMARK.conflict_quality(json.dumps(payload).encode("utf-8"))
        self.assertTrue(all(quality.values()))
        self.assertFalse(all(BENCHMARK.conflict_quality(b"not-json").values()))


if __name__ == "__main__":
    unittest.main()
