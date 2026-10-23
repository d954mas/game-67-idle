#!/usr/bin/env python3
"""Tests for folded-stack analysis."""

from __future__ import annotations

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stack_fold import load_busy, neighbours, paths_to  # noqa: E402

FOLDED = "\n".join([
    "main;A;B;target;leaf1",
    "main;A;B;target;leaf1",
    "main;C;target;leaf2",
    "main;X;Y;Z;W;wait_for;NtWaitForSingleObject",
    "main;?abc;D;E;F;target",
    "main;other",
]) + "\n"


class FoldTest(unittest.TestCase):
    def setUp(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "folded.txt")
            with open(path, "w", encoding="utf-8") as f:
                f.write(FOLDED)
            self.busy, self.total = load_busy(path)

    def test_wait_stacks_are_not_busy(self):
        self.assertEqual((len(self.busy), self.total), (5, 6))

    def test_callers_and_callees(self):
        self.assertEqual(neighbours(self.busy, "target", -1), {"B": 2, "C": 1, "F": 1})
        callees = neighbours(self.busy, "target", 1)
        self.assertEqual(callees["leaf1"], 2)
        self.assertEqual(callees["<self>"], 1)

    def test_paths_group_by_three_deepest_named_frames(self):
        got = paths_to(self.busy, "target")
        self.assertEqual(got["main > A > B"], 2)
        self.assertEqual(got["main > C"], 1)
        # The unresolved "?abc" frame is dropped from the path.
        self.assertEqual(got["D > E > F"], 1)
        self.assertEqual(sum(got.values()), 4)


if __name__ == "__main__":
    unittest.main()
