#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import io
import unittest
from contextlib import redirect_stderr
from pathlib import Path


SCRIPT = Path(__file__).with_name("devapi_playable_smoke.py")
SPEC = importlib.util.spec_from_file_location("devapi_playable_smoke", SCRIPT)
assert SPEC is not None
smoke = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(smoke)


class SmokeSuiteArgsTest(unittest.TestCase):
    def test_default_runs_all_suites(self) -> None:
        args = smoke.parse_args([])
        self.assertEqual(args.port, 9124)
        self.assertEqual(args.suites, smoke.SUITE_ORDER)

    def test_selected_suites_run_in_canonical_order(self) -> None:
        args = smoke.parse_args(["--suite", "reward-loop", "--suite", "movement,asset-load"])
        self.assertEqual(args.suites, ["asset-load", "movement", "reward-loop"])

    def test_movement_only_keeps_legacy_scope(self) -> None:
        args = smoke.parse_args(["9125", "--movement-only"])
        self.assertEqual(args.port, 9125)
        self.assertEqual(args.suites, ["contract", "asset-load", "visual-framing", "movement", "combat-pacing"])

    def test_unknown_suite_fails(self) -> None:
        with redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                smoke.parse_args(["--suite", "unknown"])


if __name__ == "__main__":
    unittest.main()
