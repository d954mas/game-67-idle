#!/usr/bin/env python3
"""Tests for the shared atlas review-label contract (atlas_review_labels.py)."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.assets.pack.atlas_review_labels import (  # noqa: E402
    DEFAULT_LABEL_FONT_SIZE,
    label_font,
    measure_label,
    review_label_text,
)


class ReviewLabelText(unittest.TestCase):
    def test_no_aliases_is_just_the_id(self) -> None:
        self.assertEqual(review_label_text("button", []), "button")
        self.assertEqual(review_label_text("button", set()), "button")

    def test_aliases_are_sorted_and_comma_joined(self) -> None:
        self.assertEqual(
            review_label_text("button", ["b_alt", "a_alt"]),
            "button (+a_alt,b_alt)",
        )

    def test_set_input_is_sorted_deterministically(self) -> None:
        # Producer (build) passes a list, validator (audit) passes a set; both
        # must yield the same string for the same alias members.
        self.assertEqual(
            review_label_text("panel", {"z", "a", "m"}),
            review_label_text("panel", ["m", "z", "a"]),
        )
        self.assertEqual(review_label_text("panel", {"z", "a", "m"}), "panel (+a,m,z)")

    def test_single_alias(self) -> None:
        self.assertEqual(review_label_text("icon", ["icon_2x"]), "icon (+icon_2x)")


class FontHelpers(unittest.TestCase):
    def test_label_font_is_cached(self) -> None:
        self.assertIs(label_font(DEFAULT_LABEL_FONT_SIZE), label_font(DEFAULT_LABEL_FONT_SIZE))

    def test_measure_label_returns_positive_dims_for_text(self) -> None:
        w, h = measure_label("hello", DEFAULT_LABEL_FONT_SIZE)
        self.assertGreater(w, 0)
        self.assertGreater(h, 0)


if __name__ == "__main__":
    unittest.main()
