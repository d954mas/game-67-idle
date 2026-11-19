import argparse
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
        # A nested section replaces the flat keys for that tool.
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


if __name__ == "__main__":
    unittest.main()
