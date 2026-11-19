import argparse
import tempfile
import unittest
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


if __name__ == "__main__":
    unittest.main()
