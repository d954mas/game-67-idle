import tempfile
import unittest
from pathlib import Path

import numpy as np

from ai_studio.assets.tools.blender.lib import clip_ground as CG, glb as G

FPS = 30.0
SOLE = -0.02  # the body's resting lowest point sits a little under zero, as real soles do


def body(scale_root=1.0):
    """Root (scaled) > Hips > Foot, a skinned box under the foot and one at the hips; returns the
    builder and node indices."""
    b = G.GlbBuilder()
    root = b.add_node("Root", scale=[scale_root] * 3)
    hips = b.add_node("Hips", translation=[0, 1.0 / scale_root, 0], parent=root)
    foot = b.add_node("Foot", translation=[0, -0.9 / scale_root, 0], parent=hips)
    corners = np.array([[x, y, z] for x in (-0.1, 0.1) for y in (0, 0.1) for z in (-0.1, 0.1)], float)
    positions = np.vstack([corners + [0, SOLE, 0], corners + [0, 0.95, 0]])
    faces = np.array([[0, 1, 2], [8, 9, 10]])
    joints = np.array([[1, 0, 0, 0]] * 8 + [[0, 0, 0, 0]] * 8, np.uint16)
    weights = np.array([[1, 0, 0, 0]] * 16, float)
    mesh = b.add_mesh("Body", positions, faces, extra={"JOINTS_0": (joints, G.UNSIGNED_SHORT),
                                                      "WEIGHTS_0": (weights, G.FLOAT)})
    rest = b.to_glb().world_rest()
    binds = b.add_accessor(np.stack([np.linalg.inv(rest[i]).T.reshape(-1) for i in (hips, foot)]), kind="MAT4")
    b.json["skins"] = [{"joints": [hips, foot], "inverseBindMatrices": binds}]
    b.add_node("BodyMesh", mesh=mesh, skin=0)
    return b, root, hips


def add_clip(b, hips, name, dip, frames=31, shared_with=None):
    """Hips translation dipping by dip(t) metres in world (Root scale undone); returns the sampler."""
    times = np.arange(frames) / FPS
    scale = b.json["nodes"][0].get("scale", [1, 1, 1])[1]
    values = np.c_[np.zeros(frames), (1.0 - np.array([dip(t) for t in times])) / scale, np.zeros(frames)]
    inp = b.add_accessor(times, bounds=True)
    out = shared_with if shared_with is not None else b.add_accessor(values)
    b.json.setdefault("animations", []).append({"name": name, "samplers": [
        {"input": inp, "output": out, "interpolation": "LINEAR"}],
        "channels": [{"sampler": 0, "target": {"node": hips, "path": "translation"}}]})
    return out


def dip_at(centre, depth, width=0.08):
    return lambda t: depth * max(0.0, 1.0 - abs(t - centre) / width)


class GroundTests(unittest.TestCase):
    def build(self, scale_root=1.0, shared=False):
        b, root, hips = body(scale_root)
        add_clip(b, hips, "Idle", lambda t: 0.0)
        out = add_clip(b, hips, "Slam", dip_at(0.5, 0.3))
        add_clip(b, hips, "Jump", lambda t: -0.4 * np.sin(np.pi * t))  # airborne: never lowered
        if shared:
            add_clip(b, hips, "SlamCopy", dip_at(0.5, 0.3), shared_with=out)
        with tempfile.TemporaryDirectory() as tmp:
            return G.Glb.load(b.write(Path(tmp) / "clips.glb"))

    def test_sink_is_lifted_to_the_baseline_floor_without_pops(self):
        for scale_root in (1.0, 0.01):
            clip = self.build(scale_root)
            document, binary, report = CG.ground(clip, baseline="Idle", animations=["Slam"])
            row = report["clips"]["Slam"]
            self.assertAlmostEqual(report["floor_y"], SOLE, places=4)
            self.assertAlmostEqual(row["max_sink_before"], 0.3, delta=0.01)
            self.assertLessEqual(row["max_sink_after"], 0.0101)
            fixed = G.Glb(document, binary)
            before = clip.channels("Slam")[("Hips", "translation")][1][:, 1] * scale_root
            after = fixed.channels("Slam")[("Hips", "translation")][1][:, 1] * scale_root
            lift = after - before
            self.assertGreaterEqual(lift.min(), -1e-6)
            self.assertLess(np.abs(np.diff(lift)).max(), 0.6 * row["max_lift"])  # spread, no one-key pop
            far = np.abs(np.arange(31) / FPS - 0.5) > 0.2
            self.assertTrue(np.array_equal(before[far], after[far]))  # floor contacts keep their keys

    def test_other_clips_and_airborne_clips_stay_byte_identical(self):
        clip = self.build()
        document, binary, report = CG.ground(clip, baseline="Idle")
        self.assertEqual(len(binary), len(clip.bin))
        fixed = G.Glb(document, binary)
        for name in ("Idle", "Jump"):
            self.assertEqual(report["clips"][name]["frames_touched"], 0)
            self.assertTrue(np.array_equal(clip.channels(name)[("Hips", "translation")][1],
                                           fixed.channels(name)[("Hips", "translation")][1]))

    def test_shared_accessor_gets_a_new_one(self):
        clip = self.build(shared=True)
        document, binary, report = CG.ground(clip, baseline="Idle", animations=["Slam"])
        fixed = G.Glb(document, binary)
        self.assertGreater(len(binary), len(clip.bin))
        self.assertLessEqual(report["clips"]["Slam"]["max_sink_after"], 0.0101)
        self.assertTrue(np.array_equal(clip.channels("SlamCopy")[("Hips", "translation")][1],
                                       fixed.channels("SlamCopy")[("Hips", "translation")][1]))
        with tempfile.TemporaryDirectory() as tmp:
            G.write_glb(Path(tmp) / "out.glb", document, binary)
            again = G.Glb.load(Path(tmp) / "out.glb")
            self.assertEqual(again.json["buffers"][0]["byteLength"], len(binary))

    def test_lift_profile_never_undercuts_a_key(self):
        sink = np.array([0, 0, 0, 0.4, 0, 0, 0, 0.1, 0, 0], float)
        lift = CG.lift_profile(sink, 3)
        self.assertTrue((lift >= sink).all())
        self.assertEqual(lift[0], sink[3] * 0.5 * (1 + np.cos(np.pi * 3 / 4)))
        wrapped = CG.lift_profile(np.r_[0.4, np.zeros(9)], 2, loop=True)
        self.assertGreater(wrapped[-1], 0)

    def test_floor_height_and_tolerance(self):
        clip = self.build()
        _, _, report = CG.ground(clip, floor=-0.25, animations=["Slam"])
        self.assertEqual(report["clips"]["Slam"]["frames_touched"], 7)  # one key 0.24 m deep, ramped 3 each side
        _, _, report = CG.ground(clip, floor=SOLE, tolerance=0.5, animations=["Slam"])
        self.assertEqual(report["clips"]["Slam"]["frames_touched"], 0)


if __name__ == "__main__":
    unittest.main()
