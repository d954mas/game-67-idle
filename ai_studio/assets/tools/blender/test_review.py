import argparse
import json
import tempfile
import unittest
import unittest.mock
from pathlib import Path

import numpy as np

from ai_studio.assets.tools.blender import review
from ai_studio.assets.tools.blender.lib import glb as G


def clip_only(path):
    b = G.GlbBuilder()
    bone = b.add_node("Hips")
    times = b.add_accessor(np.array([0.0, 1.0]), kind="SCALAR", bounds=True)
    moves = b.add_accessor(np.array([[0, 1, 0], [0, 1.1, 0]], float))
    b.json["animations"] = [{"name": "Idle", "samplers": [{"input": times, "output": moves}],
                             "channels": [{"sampler": 0, "target": {"node": bone, "path": "translation"}}]}]
    return b.write(path)


class Options(unittest.TestCase):
    def test_flags_win_over_config_and_flat_keys_go_to_their_tool(self):
        config = {"pivot": "floor", "profile": "volume", "glb_check": {"jpeg": "off"}}
        self.assertEqual(review.tool_options("mesh_check", config, profile=None), {"profile": "volume"})
        self.assertEqual(review.tool_options("mesh_check", config, profile="surface"), {"profile": "surface"})
        # A nested section adds to and overrides the flat keys for that tool.
        self.assertEqual(review.tool_options("glb_check", config, pivot="top"), {"jpeg": "off", "pivot": "top"})


class ClipOnly(unittest.TestCase):
    def test_animation_only_file_gets_a_hint_not_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = clip_only(Path(tmp) / "idle.glb")
            args = argparse.Namespace(
                profile=None, pivot=None, contacts=None, floating=False, clip_model=None)
            reports = review.run_checks(path, args, {})
        tools = {r["tool"]: r for r in reports}
        self.assertNotIn("mesh_check", tools)
        self.assertEqual(tools["clip_check"]["findings"][0]["check"], "clip_only")
        self.assertFalse(any(f["severity"] == "high" for r in reports for f in r["findings"]))


class Batch(unittest.TestCase):
    def args(self, out):
        return argparse.Namespace(profile=None, pivot=None, contacts=None, floating=False, clip_model=None,
                                  no_render=True, out=str(out), game_crop=None)

    def test_unreadable_file_is_a_finding_and_the_batch_goes_on(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            (tmp / "broken.glb").write_bytes(b"glTF not really")
            good = clip_only(tmp / "idle.glb")
            results, _ = review.review([tmp / "broken.glb", good], self.args(tmp / "out"), {})
        by_name = {Path(s["asset"]).name: reports for s, reports in results}
        self.assertEqual(by_name["broken.glb"][0]["findings"][0]["check"], "unreadable")
        self.assertIn("idle.glb", by_name)

    def test_folders_and_globs_expand_relative_to_the_caller(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            for name in ("a.glb", "b.glb"):
                clip_only(tmp / name)
            with unittest.mock.patch.dict("os.environ", {"NT_CALLER_CWD": str(tmp)}):
                self.assertEqual([m.name for m in review.expand_models(["."])], ["a.glb", "b.glb"])
                self.assertEqual([m.name for m in review.expand_models(["b*.glb"])], ["b.glb"])

    def test_config_typo_is_refused(self):
        with self.assertRaises(SystemExit):
            review.validate_config({"pivot_tolerence": 0.01})
        review.validate_config({"pivot_tolerance": 0.01, "mesh_check": {"profile": "volume"}})


class Presets(unittest.TestCase):
    def test_nearest_preset_wins_and_paths_are_relative_to_it(self):
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp) / "game"
            props = game / "assets" / "props"
            props.mkdir(parents=True)
            (game / "game.json").write_text("{}", encoding="utf-8")
            (game / "blender_review.json").write_text(json.dumps(
                {"pivot": "floor", "size": 512, "mesh_check": {"profile": "volume"}}), encoding="utf-8")
            (props / "blender_review.json").write_text(json.dumps(
                {"size": 384, "beside": ["approved.glb"], "mesh_check": {"weld": 1e-5}}), encoding="utf-8")
            model = clip_only(props / "crate.glb")
            args = argparse.Namespace(size=None, pivot="top")
            opts, tools = review.model_options(model, args, {"jpeg": "off"})
        self.assertEqual(opts.size, 384)
        self.assertEqual(opts.pivot, "top")
        self.assertEqual(Path(opts.beside[0]).name, "approved.glb")
        self.assertEqual(Path(opts.beside[0]).parent.name, "props")
        self.assertEqual(tools["mesh_check"], {"profile": "volume", "weld": 1e-5})
        self.assertEqual(tools["glb_check"], {"jpeg": "off"})
        self.assertEqual(len(opts.presets), 2)


class Precedence(unittest.TestCase):
    def test_zero_is_a_value_and_flat_nearer_keys_beat_farther_nested_ones(self):
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp) / "game"
            props = game / "props"
            props.mkdir(parents=True)
            (game / "game.json").write_text("{}", encoding="utf-8")
            (game / "blender_review.json").write_text(json.dumps(
                {"max_poses": 4, "turntable": 8, "clip_check": {"slide": 0.1}}), encoding="utf-8")
            (props / "blender_review.json").write_text(json.dumps({"slide": 0.2}), encoding="utf-8")
            model = clip_only(props / "a.glb")
            opts, tools = review.model_options(model, argparse.Namespace(max_poses=0, turntable=0), {})
            self.assertEqual((opts.max_poses, opts.turntable), (0, 0))
            self.assertEqual(tools["clip_check"]["slide"], 0.2)
            _, tools = review.model_options(model, argparse.Namespace(), {"slide": 0.3})
            self.assertEqual(tools["clip_check"]["slide"], 0.3)

    def test_preset_typos_are_refused_with_the_file_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            preset = Path(tmp) / "blender_review.json"
            preset.write_text(json.dumps({"besides": ["a.glb"]}), encoding="utf-8")
            with self.assertRaises(SystemExit) as raised:
                review.read_preset(preset)
            self.assertIn("besides", str(raised.exception))
            preset.write_text(json.dumps({"mesh_check": {"wled": 1}}), encoding="utf-8")
            with self.assertRaises(SystemExit):
                review.read_preset(preset)


class Poses(unittest.TestCase):
    def test_clip_findings_become_pose_moments(self):
        reports = [{"tool": "clip_check", "findings": [
            {"check": "foot_slide", "severity": "medium", "where": {"animation": "Walk", "time": 0.4}},
            {"check": "pop", "severity": "medium", "where": {"animation": "Run", "pops": [
                {"joint": "Head", "time": 1.25}]}},
            {"check": "pop", "severity": "medium", "where": {"clips": [{"animation": "Run", "time": 1.25}]}},
            {"check": "feet", "severity": "info", "where": {"animation": "Idle", "time": 0}}]}]
        self.assertEqual(review.finding_poses(reports, 4), ["Walk@0.4", "Run@1.25"])
        self.assertEqual(review.finding_poses(reports, 1), ["Walk@0.4"])
        nested = [{"tool": "clip_check", "findings": [
            {"check": "pop", "severity": "medium", "where": {"animation": "Run", "time": 1.25,
                                                             "pops": [{"time": 1.5}]}}]}]
        self.assertEqual(review.finding_poses(nested, 4), ["Run@1.25", "Run@1.5"])


if __name__ == "__main__":
    unittest.main()
