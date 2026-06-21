#!/usr/bin/env python3
"""Tests for the universal StateCapture helper (no live game required)."""
from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from state_capture import STANDARD_STATES, StateCapture  # noqa: E402


class FakeClient:
    """Stand-in for DevApiClient: writes a placeholder PNG and records the call."""

    def __init__(self) -> None:
        self.captured: list[tuple[str, bool]] = []

    def capture_screenshot(self, output, wait_frames=1, audit=False):
        os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
        with open(output, "w", encoding="utf-8") as handle:
            handle.write("png")
        self.captured.append((output, audit))
        return output


class StateCaptureTest(unittest.TestCase):
    def test_matrix_records_covered_and_debt(self):
        with tempfile.TemporaryDirectory() as tmp:
            client = FakeClient()
            sc = StateCapture(client, "demo", out_dir=os.path.join(tmp, "caps"))
            sc.require("first_screen", "reward_active")
            sc.capture("first_screen")
            sc.mark_debt("reward_active", "no reward state in this slice")

            matrix = sc.matrix()
            self.assertEqual(matrix["schema"], "game.live_state_acceptance_matrix")
            self.assertEqual(matrix["project"], "demo")
            self.assertIn("first_screen", matrix["required_states"])
            self.assertIn("reward_active", matrix["required_states"])
            self.assertEqual(matrix["states"]["first_screen"]["status"], "covered")
            self.assertEqual(matrix["states"]["reward_active"]["status"], "not_covered")
            self.assertEqual(len(client.captured), 1)
            self.assertTrue(client.captured[0][1])  # audit defaults on

            out = sc.write_matrix(os.path.join(tmp, "state_matrix.json"))
            with open(out, encoding="utf-8") as handle:
                loaded = json.load(handle)
            self.assertEqual(
                loaded["states"]["first_screen"]["evidence"],
                matrix["states"]["first_screen"]["evidence"],
            )

    def test_capture_auto_adds_required_and_is_idempotent_per_tag(self):
        with tempfile.TemporaryDirectory() as tmp:
            client = FakeClient()
            sc = StateCapture(client, "demo", out_dir=os.path.join(tmp, "caps"))
            sc.capture("combat")
            sc.capture("combat")  # re-capture same tag should not duplicate
            self.assertEqual([entry["tag"] for entry in sc.covered], ["combat"])
            self.assertIn("combat", sc.required)
            self.assertEqual(sc.shots_args(), [f"--shot combat:{os.path.join(tmp, 'caps')}/state_combat.png"])

    def test_standard_states_present(self):
        for tag in ("first_screen", "transient_stress_state", "reward_active"):
            self.assertIn(tag, STANDARD_STATES)


if __name__ == "__main__":
    unittest.main()
