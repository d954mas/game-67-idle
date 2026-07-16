#!/usr/bin/env python3
"""Contract test for the bounded T0366 CLI benchmark."""

from __future__ import annotations

import json
import hashlib
from pathlib import Path
import unittest

import benchmark_items_cli as BENCHMARK


PROJECT = Path(__file__).resolve().parents[1] / "tests" / "fixtures" / "items_cli"
RESULT = Path(__file__).resolve().parent / "results" / "windows-cli-2026-07-15.json"


class ItemsCliBenchmarkTests(unittest.TestCase):
    def test_representative_edit_loop_reports_required_metrics(self):
        result = BENCHMARK.benchmark(PROJECT)
        self.assertEqual(result["schema"], "items.cli.benchmark.v1")
        self.assertEqual(result["command_count"], 5)
        self.assertGreater(result["totals"]["wall_ms"], 0)
        self.assertEqual(result["totals"]["logical_project_file_reads"], 28)
        self.assertGreater(result["totals"]["stdout_bytes"], 0)
        self.assertGreater(result["totals"]["stderr_bytes"], 0)
        self.assertTrue(all(result["diagnostic_quality"].values()))

    def test_recorded_result_matches_current_cli_source(self):
        recorded = json.loads(RESULT.read_text(encoding="utf-8"))
        source_hash = "sha256:" + hashlib.sha256(BENCHMARK.SCRIPT.read_bytes()).hexdigest()
        self.assertEqual(recorded["method"]["source_sha256"], source_hash)
        self.assertEqual(recorded["totals"]["logical_project_file_reads"], 28)


if __name__ == "__main__":
    unittest.main()
