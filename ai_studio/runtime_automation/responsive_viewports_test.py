#!/usr/bin/env python3
"""Tests for the Studio responsive viewport capture."""

from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import responsive_viewports as rv  # noqa: E402
from png_io import read_png, write_png_rgb  # noqa: E402


class FakeGame:
    def __init__(self, viewport):
        self.viewport = viewport
        self.waits = []

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        return None

    def result(self, method, params=None):
        if method == "ui.tree":
            return {
                "width": self.viewport.width,
                "height": self.viewport.height,
                "viewport": {"x": 0, "y": 0, "w": self.viewport.width, "h": self.viewport.height},
                "nodes": [{"id_string": "settings/gear"}, {"id_string": ""}],
            }
        if method == "view":
            return {"width": self.viewport.width, "height": self.viewport.height}
        raise AssertionError(f"unexpected method: {method}")

    def wait_frames(self, frames=1):
        self.waits.append(frames)
        return {"frame": len(self.waits)}

    def capture_screenshot(self, output, wait_frames=1, audit=True):
        color = bytes((self.viewport.width % 255, self.viewport.height % 255, 96))
        write_png_rgb(output, 2, 1, color * 2)
        return output


def parse(argv):
    return rv.build_parser().parse_args(argv)


class ViewportMatrixTest(unittest.TestCase):
    def test_startup_timeout_can_be_overridden_without_changing_default(self):
        self.assertEqual(parse(["--startup-timeout", "30"]).startup_timeout, 30.0)
        self.assertEqual(parse([]).startup_timeout, 8.0)

    def test_parse_viewport_accepts_named_size(self):
        viewport = rv.parse_viewport("phone=390x844")
        self.assertEqual((viewport.name, viewport.width, viewport.height, viewport.orientation), ("phone", 390, 844, "portrait"))

    def test_selected_viewports_can_filter_landscape(self):
        viewports = rv.selected_viewports([], landscape_only=True)
        self.assertTrue(viewports)
        self.assertTrue(all(viewport.orientation == "landscape" for viewport in viewports))

    def test_make_contact_sheet_writes_combined_png(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = root / "a.png"
            b = root / "b.png"
            write_png_rgb(str(a), 2, 1, b"\x00\x00\x00\xff\x00\x00")
            write_png_rgb(str(b), 1, 2, b"\x00\xff\x00\x00\x00\xff")
            out = root / "sheet.png"
            self.assertEqual(rv.make_contact_sheet([{"name": "a", "screenshot": str(a)}, {"name": "b", "screenshot": str(b)}], out, columns=2, padding=1), str(out))
            width, height, _color_type, _pixels = read_png(str(out))
            self.assertEqual((width, height), (7, 4))

    def test_run_matrix_writes_qclr_summary_and_screenshots(self):
        with tempfile.TemporaryDirectory() as tmp:
            viewports = [rv.ViewportCase("wide", 640, 360), rv.ViewportCase("phone", 390, 844)]
            summary = rv.run_matrix(lambda viewport: FakeGame(viewport), viewports, Path(tmp), audit=False, warmup_frames=1)
            self.assertEqual(summary["quality_rule"], "QCLR_002")
            self.assertEqual(len(summary["viewports"]), 2)
            self.assertTrue(Path(summary["summary"]).exists())
            self.assertTrue(Path(summary["contact_sheet"]).exists())
            self.assertTrue(Path(summary["viewports"][0]["screenshot"]).exists())

    def test_run_matrix_calls_prepare_before_capture(self):
        calls = []

        def prepare(game, viewport):
            calls.append((viewport.name, list(game.waits)))
            return {"prepared": viewport.window_size}

        with tempfile.TemporaryDirectory() as tmp:
            summary = rv.run_matrix(lambda viewport: FakeGame(viewport), [rv.ViewportCase("wide", 640, 360)], Path(tmp), audit=False, warmup_frames=2, prepare=prepare)
            self.assertEqual(calls, [("wide", [2])])
            self.assertEqual(summary["viewports"][0]["scenario_result"], {"prepared": "640x360"})

    def test_run_matrix_reports_poki_pill_overlap(self):
        class PillGame(FakeGame):
            def result(self, method, params=None):
                tree = super().result(method, params)
                if method == "ui.tree":
                    height = self.viewport.height
                    tree["nodes"] = [
                        {"id": 1, "id_string": "hud/coins", "child_count": 0, "visible": True, "bounds": {"x": 4, "y": height - 60, "w": 40, "h": 30}},
                        {"id": 2, "id_string": "bg/full", "child_count": 0, "visible": True, "bounds": {"x": 0, "y": 0, "w": self.viewport.width, "h": height}},
                    ]
                return tree

        with tempfile.TemporaryDirectory() as tmp:
            summary = rv.run_matrix(lambda viewport: PillGame(viewport), [rv.ViewportCase("wide", 640, 360)], Path(tmp), audit=False, warmup_frames=0, portal="poki", zone_ignore=["bg/*"])
            check = summary["viewports"][0]["portal_zones"]
            self.assertEqual([violation["id_string"] for violation in check["violations"]], ["hud/coins"])
            self.assertEqual(summary["portal_zones"]["failures"], 1)

    def test_run_matrix_without_portal_skips_zone_check(self):
        with tempfile.TemporaryDirectory() as tmp:
            summary = rv.run_matrix(lambda viewport: FakeGame(viewport), [rv.ViewportCase("wide", 640, 360)], Path(tmp), audit=False, warmup_frames=0)
            self.assertNotIn("portal_zones", summary)
            self.assertNotIn("portal_zones", summary["viewports"][0])


class LaunchResolutionTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.game = self.root / "games" / "demo"
        (self.game / "capture").mkdir(parents=True)

    def tearDown(self):
        self.tmp.cleanup()

    def write_catalog(self, value):
        (self.game / "capture" / "catalog.json").write_text(json.dumps(value), encoding="utf-8")

    def resolve(self, argv):
        return rv.resolve_launch(parse(argv), resolve_game=lambda _id: self.game, invocation_dir=self.root)

    def test_game_takes_executable_and_launch_flags_from_capture_catalog(self):
        self.write_catalog({
            "version": 1,
            "executable": "build/native-devapi/bin/client.exe",
            "launch": {"fresh_state": False, "autosave_enabled": True, "args": ["--solo"]},
        })
        plan = self.resolve(["--game", "demo", "--launch-arg=--no-vsync"])
        self.assertEqual(plan.exe, (self.game / "build/native-devapi/bin/client.exe").resolve())
        self.assertEqual(plan.cwd, self.game.resolve())
        self.assertEqual((plan.fresh_state, plan.autosave_enabled), (False, True))
        self.assertEqual(plan.extra_args, ("--solo", "--no-vsync"))
        self.assertEqual(plan.out, (self.game / rv.OUT_SUBDIR).resolve())

    def test_game_without_catalog_uses_canonical_devapi_build_and_fresh_state(self):
        plan = self.resolve(["--game", "demo"])
        self.assertEqual(plan.exe.parent, (self.game / "build" / "devapi-debug" / "bin").resolve())
        self.assertEqual((plan.fresh_state, plan.autosave_enabled, plan.extra_args), (True, False, ()))

    def test_cli_overrides_catalog(self):
        self.write_catalog({"version": 1, "launch": {"fresh_state": False, "autosave_enabled": True}})
        plan = self.resolve(["--game", "demo", "--exe", "other.exe", "--cwd", "work", "--fresh-state", "--no-autosave", "--out", "ev"])
        self.assertEqual(plan.exe, (self.root / "other.exe").resolve())
        self.assertEqual(plan.cwd, (self.root / "work").resolve())
        self.assertEqual(plan.out, (self.root / "ev").resolve())
        self.assertEqual((plan.fresh_state, plan.autosave_enabled), (True, False))

    def test_exe_without_game_defaults_cwd_and_out_to_invocation_dir(self):
        plan = rv.resolve_launch(parse(["--exe", "bin/game.exe"]), invocation_dir=self.root)
        self.assertEqual(plan.cwd, self.root.resolve())
        self.assertEqual(plan.out, (self.root / rv.OUT_SUBDIR).resolve())
        self.assertIsNone(plan.game_root)

    def test_neither_game_nor_exe_is_an_error(self):
        with self.assertRaises(ValueError):
            rv.resolve_launch(parse([]), invocation_dir=self.root)

    def test_malformed_catalog_launch_is_rejected(self):
        self.write_catalog({"version": 1, "launch": {"fresh_state": "no"}})
        with self.assertRaises(ValueError):
            self.resolve(["--game", "demo"])

    def test_resolve_game_root_matches_id_or_alias_uniquely(self):
        mounts = [{"gameId": "demo", "root": "games/demo", "aliases": ["old-demo"]}, {"gameId": "other", "root": "games/other"}]
        list_mounts = lambda _root: mounts  # noqa: E731
        self.assertEqual(rv.resolve_game_root("old-demo", self.root, list_mounts), self.game.resolve())
        with self.assertRaises(ValueError):
            rv.resolve_game_root("missing", self.root, list_mounts)

    def test_scenario_path_resolves_against_game_root(self):
        (self.game / "devapi").mkdir()
        (self.game / "devapi" / "hooks.py").write_text("def prepare(game, viewport):\n    return 'ok'\n", encoding="utf-8")
        prepare = rv.load_prepare("devapi/hooks.py:prepare", search_roots=[self.game])
        self.assertEqual(prepare(None, None), "ok")


if __name__ == "__main__":
    unittest.main()
