#!/usr/bin/env python3
"""Tests for the UI monkey's step choice, coordinate mapping and log watch."""

from __future__ import annotations

import os
import random
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ui_monkey as um  # noqa: E402


def tree(nodes, width=1000.0, height=500.0, view=(0, 0, 500, 250)):
    return {"width": width, "height": height, "viewport": {"x": view[0], "y": view[1], "w": view[2], "h": view[3]}, "nodes": nodes}


def button(node_id, x=10.0, y=10.0, w=40.0, h=20.0, visible=True, enabled=True):
    return {"id": node_id, "role": "nt_button", "visible": visible, "enabled": enabled, "bounds": {"x": x, "y": y, "w": w, "h": h}}


class ButtonsTest(unittest.TestCase):
    def test_only_visible_enabled_buttons_with_a_box(self):
        nodes = [button(1), button(2, visible=False), button(3, enabled=False), button(4, w=0.0),
                 {"id": 5, "role": "nt_label", "visible": True, "enabled": True, "bounds": {"x": 0, "y": 0, "w": 9, "h": 9}}]
        self.assertEqual([n["id"] for n in um.buttons(tree(nodes))], [1])


class MappingTest(unittest.TestCase):
    def test_ui_space_scales_into_the_framebuffer_viewport(self):
        self.assertEqual(um.ui_to_framebuffer(tree([], view=(20, 10, 500, 250)), 500.0, 250.0), (270.0, 135.0))


class ChooserTest(unittest.TestCase):
    def steps(self, seed, count=120):
        chooser = um.Chooser(random.Random(seed), ["a", "b", "c"], ["E"])
        return [chooser.next(tree([button(1), button(2, x=100)])) for _ in range(count)]

    def test_a_seed_repeats_its_steps(self):
        self.assertEqual(self.steps(4), self.steps(4))
        self.assertNotEqual(self.steps(4), self.steps(5))

    def test_every_round_visits_each_window_and_the_bare_screen(self):
        opens = [s.get("scene", "") for s in self.steps(9, 400) if s["kind"] in ("scene_show", "close_modals")]
        self.assertGreaterEqual(len(opens), 4)
        self.assertEqual(sorted(opens[:4]), ["", "a", "b", "c"])

    def test_episodes_press_buttons_inside_the_window(self):
        kinds = [s["kind"] for s in self.steps(2, 400)]
        self.assertGreater(kinds.count("button"), kinds.count("scene_show") + kinds.count("close_modals"))

    def test_no_button_falls_back_to_a_touch(self):
        chooser = um.Chooser(random.Random(1), [], ["E"])
        steps = [chooser.next(tree([])) for _ in range(60)]
        self.assertNotIn("button", [s["kind"] for s in steps])


class LogWatchTest(unittest.TestCase):
    def test_reports_only_new_matching_lines(self):
        with tempfile.TemporaryDirectory() as tmp:
            log = Path(tmp) / "game.log"
            log.write_text("INFO [ui] fine\nERROR [ui] clay error type=4\n", encoding="utf-8")
            watch = um.LogWatch(str(log), re.compile(um.DEFAULT_FAIL_PATTERN))
            self.assertEqual(watch.new_failures(), ["ERROR [ui] clay error type=4"])
            self.assertEqual(watch.new_failures(), [])
            with log.open("a", encoding="utf-8") as out:
                out.write("ERROR [http] fetch failed\nNT_ASSERT failed:\n")
            self.assertEqual(watch.new_failures(), ["NT_ASSERT failed:"])


class ViewportTest(unittest.TestCase):
    def test_hand_suffix(self):
        self.assertEqual(um.parse_viewport("p=390x844:hand"), um.Viewport("p", 390, 844, True))
        self.assertFalse(um.parse_viewport("d=1280x720").in_hand)


if __name__ == "__main__":
    unittest.main()
