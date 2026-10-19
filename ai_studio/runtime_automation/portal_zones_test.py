#!/usr/bin/env python3
"""Tests for the portal reserved-zone check."""

from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import portal_zones as pz  # noqa: E402

PILL = {
    "id": "pill",
    "anchor": "top-left",
    "top_px": 24,
    "sizes": [
        {"max_frame_width_px": 1210, "width_px": 46, "height_px": 62},
        {"min_frame_width_px": 1211, "width_px": 92, "height_px": 64},
    ],
    "source": "test",
}


def tree(width, height, nodes, viewport=None, dpr=1.0):
    return {
        "width": width,
        "height": height,
        "dpr": dpr,
        "viewport": viewport if viewport is not None else {"x": 0, "y": 0, "w": width * dpr, "h": height * dpr},
        "nodes": nodes,
    }


def leaf(id_string, x, y, w, h, **extra):
    node = {"id": len(id_string), "id_string": id_string, "child_count": 0, "visible": True, "bounds": {"x": x, "y": y, "w": w, "h": h}}
    node.update(extra)
    return node


class ZoneRectTest(unittest.TestCase):
    def test_narrow_frame_uses_small_pill(self):
        self.assertEqual(pz.zone_rect(PILL, 1210, 700), (0.0, 24.0, 46.0, 86.0))

    def test_wide_frame_uses_large_pill(self):
        self.assertEqual(pz.zone_rect(PILL, 1211, 700), (0.0, 24.0, 92.0, 88.0))

    def test_top_right_anchor_hugs_right_edge(self):
        zone = dict(PILL, anchor="top-right")
        self.assertEqual(pz.zone_rect(zone, 800, 600), (754.0, 24.0, 800.0, 86.0))

    def test_load_target_zones_reads_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            Path(tmp, "p.json").write_text(json.dumps({"metadata": {"reserved_zones": [PILL]}}), encoding="utf-8")
            Path(tmp, "q.json").write_text(json.dumps({"metadata": {}}), encoding="utf-8")
            self.assertEqual(pz.load_target_zones("p", Path(tmp))[0]["id"], "pill")
            self.assertEqual(pz.load_target_zones("q", Path(tmp)), [])
            with self.assertRaises(pz.PortalZoneError):
                pz.load_target_zones("missing", Path(tmp))

    def test_studio_poki_target_declares_a_zone(self):
        self.assertTrue(pz.load_target_zones("poki"))


class NodeMappingTest(unittest.TestCase):
    def test_y_up_bounds_flip_to_window_top(self):
        t = tree(400, 300, [])
        # A node touching the layout top (y + h == height) lands at window top.
        self.assertEqual(pz.node_window_rect({"x": 10, "y": 280, "w": 20, "h": 20}, t), (10.0, 0.0, 30.0, 20.0))
        self.assertEqual(pz.node_window_rect({"x": 0, "y": 0, "w": 50, "h": 10}, t), (0.0, 290.0, 50.0, 300.0))

    def test_scaled_letterboxed_viewport_and_dpr(self):
        # Layout 200x100 drawn at 2x into device px offset (0, 100), dpr 2 -> window scale 1x, y offset 50.
        t = tree(200, 100, [], viewport={"x": 0, "y": 100, "w": 400, "h": 200}, dpr=2.0)
        self.assertEqual(pz.node_window_rect({"x": 0, "y": 90, "w": 10, "h": 10}, t), (0.0, 50.0, 10.0, 60.0))

    def test_negative_viewport_height_normalises(self):
        t = tree(100, 100, [], viewport={"x": 0, "y": 100, "w": 100, "h": -100})
        self.assertEqual(pz.node_window_rect({"x": 0, "y": 90, "w": 10, "h": 10}, t), (0.0, 90.0, 10.0, 100.0))

    def test_missing_viewport_reports_error(self):
        t = tree(100, 100, [leaf("hud/coins", 0, 90, 10, 10)])
        t.pop("viewport")
        self.assertIn("error", pz.check_tree(t, [PILL], 100, 100))


class CheckTreeTest(unittest.TestCase):
    def test_reports_visible_leaf_in_zone(self):
        t = tree(640, 360, [
            leaf("hud/coins", 4, 300, 60, 30),          # window y 30..60 -> inside the Pill
            leaf("hud/score", 500, 300, 60, 30),        # far right
            leaf("hud/hidden", 4, 300, 60, 30, visible=False),
            leaf("hud/panel", 0, 0, 640, 360, child_count=3),
            leaf("hud/empty", 4, 300, 0, 30),
        ])
        result = pz.check_tree(t, [PILL], 640, 360)
        self.assertEqual([v["id_string"] for v in result["violations"]], ["hud/coins"])
        self.assertEqual(result["violations"][0]["zone"], "pill")

    def test_ignore_patterns_skip_decorative_leaves(self):
        t = tree(640, 360, [leaf("bg/backdrop", 0, 0, 640, 360), leaf("hud/coins", 4, 300, 60, 30)])
        result = pz.check_tree(t, [PILL], 640, 360, ignore=["bg/*"])
        self.assertEqual([v["id_string"] for v in result["violations"]], ["hud/coins"])

    def test_leaf_below_pill_is_clear(self):
        # Pill bottom is window y 86 at width 640; a leaf from y 90 down is clear.
        t = tree(640, 360, [leaf("hud/coins", 0, 240, 60, 30)])
        self.assertEqual(pz.check_tree(t, [PILL], 640, 360)["violations"], [])

    def test_wide_breakpoint_catches_leaf_the_narrow_pill_misses(self):
        t = tree(1280, 720, [leaf("hud/coins", 60, 650, 20, 20)])  # window x 60..80, y 50..70
        self.assertEqual(len(pz.check_tree(t, [PILL], 1280, 720)["violations"]), 1)
        t = tree(1200, 720, [leaf("hud/coins", 60, 650, 20, 20)])
        self.assertEqual(pz.check_tree(t, [PILL], 1200, 720)["violations"], [])


if __name__ == "__main__":
    unittest.main()
