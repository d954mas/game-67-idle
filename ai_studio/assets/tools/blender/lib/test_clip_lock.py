import unittest

import numpy as np

from ai_studio.assets.tools.blender.lib import clip_lock as CL, glb as G
from ai_studio.assets.tools.blender.lib.test_retarget import add_motion, build_rig, loaded, positions

LEGS = {f"mixamorig:{s}{j}" for s in ("Left", "Right") for j in ("UpLeg", "Leg", "Foot")}


def gliding_clip(travel=0.2, frames=31, legs=0.05):
    """Hips drift forward while the legs barely move: both feet glide while planted."""
    builder, nodes = build_rig(positions())
    add_motion(builder, nodes, frames=frames, travel=travel, legs=legs)
    return loaded(builder)


class FootLockTests(unittest.TestCase):
    def test_lock_pins_feet_and_changes_only_leg_rotations(self):
        clip = gliding_clip()
        document, binary, report = CL.foot_lock(clip)
        row = report["clips"]["move"]
        for side in ("left", "right"):
            self.assertTrue(row["feet"][side]["locked"])
            self.assertGreater(row["feet"][side]["slide_before"], 0.03)
            self.assertLess(row["feet"][side]["slide_after"], 0.1 * row["feet"][side]["slide_before"])
        fixed = G.Glb(document, binary)
        self.assertEqual(len(binary), len(clip.bin))  # rewritten in place
        before, after = clip.channels("move"), fixed.channels("move")
        changed = {name for (name, path) in before if not np.array_equal(before[(name, path)][1], after[(name, path)][1])}
        self.assertTrue(changed)
        self.assertLessEqual(changed, LEGS)

    def test_a_locked_clip_is_stamped_and_left_alone(self):
        document, binary, _ = CL.foot_lock(gliding_clip())
        again = G.Glb(document, binary)
        self.assertIn(CL.STAMP, again.json["animations"][0]["extras"])
        document2, binary2, report = CL.foot_lock(again)
        self.assertIn("already locked", report["clips"]["move"]["skipped"])
        self.assertEqual(bytes(binary2), bytes(binary))

    def test_small_drift_is_left_alone(self):
        report = CL.foot_lock(gliding_clip(travel=0.005, legs=0.0))[2]
        self.assertFalse(any(f["locked"] for f in report["clips"]["move"]["feet"].values()))

    def test_loop_keeps_the_seam_closed(self):
        builder, nodes = build_rig(positions())
        times = add_motion(builder, nodes, frames=31, legs=0.05)
        # Close the loop and make the feet glide back and forth: a plant that runs through the seam.
        doc = builder.json
        out = doc["animations"][0]["samplers"][doc["animations"][0]["channels"][-1]["sampler"]]["output"]
        clip = loaded(builder)
        hips = clip.accessor(out)
        hips[:, 2] = hips[0, 2] + 0.05 * np.sin(2 * np.pi * times / times[-1])
        start = doc["bufferViews"][doc["accessors"][out]["bufferView"]]["byteOffset"]
        builder.payload[start:start + hips.size * 4] = hips.astype("<f4").tobytes()
        clip = loaded(builder)
        document, binary, _ = CL.foot_lock(clip, loop=True)
        fixed = G.Glb(document, binary)
        foot = fixed.by_name["mixamorig:LeftFoot"]
        worlds = fixed.posed_worlds(fixed.channels(0, by_index=True), [times[0], times[-1]])
        self.assertLess(float(np.linalg.norm(worlds[foot][0, :3, 3] - worlds[foot][1, :3, 3])), 1e-3)

    def test_in_place_space_follows_the_games_drift_removal(self):
        # The hips travel 0.2 m and the legs ride along: in world the feet glide, but a game that
        # removes the root's start-to-end travel linearly shows them standing still.
        clip = gliding_clip(legs=0.0)
        keys = clip.channels("move")[("mixamorig:LeftUpLeg", "rotation")][0]
        worlds = clip.posed_worlds(clip.channels("move", by_index=True), keys)
        shift = CL.in_place_shift(clip, 0, clip, worlds, keys, np.array([0.0, 1.0, 0.0]))
        hips = worlds[clip.by_name["mixamorig:Hips"]][:, :3, 3]
        self.assertAlmostEqual(float(shift[-1, 2]), float(hips[-1, 2] - hips[0, 2]), places=5)
        self.assertAlmostEqual(float(np.abs(shift[:, 1]).max()), 0.0, places=6)  # height is never taken away
        world = CL.foot_lock(clip)[2]["clips"]["move"]["feet"]
        in_place = CL.foot_lock(clip, space="in_place")
        feet = in_place[2]["clips"]["move"]["feet"]
        self.assertTrue(all(f["locked"] for f in world.values()))
        for side in feet:
            self.assertLess(feet[side]["slide_before"], 0.25 * world[side]["slide_before"])

    def test_toe_pin_keeps_the_toe_still(self):
        clip = gliding_clip()
        document, binary, report = CL.foot_lock(clip, pin="toe")
        fixed = G.Glb(document, binary)
        keys = fixed.channels("move")[("mixamorig:LeftUpLeg", "rotation")][0]
        worlds = fixed.posed_worlds(fixed.channels("move", by_index=True), keys)
        toe = worlds[fixed.by_name["mixamorig:LeftToeBase"]][:, :3, 3]
        self.assertEqual(report["clips"]["move"]["pin"], "toe")
        self.assertLess(float(np.ptp(toe[:, [0, 2]], axis=0).max()), 0.01)

    def test_clip_check_linear_drift_judges_the_games_space(self):
        import tempfile
        from pathlib import Path
        from ai_studio.assets.tools.blender import clip_check
        builder, nodes = build_rig(positions())
        add_motion(builder, nodes, travel=0.4, legs=0.0)
        with tempfile.TemporaryDirectory() as tmp:
            path = builder.write(Path(tmp) / "clip.glb")

            def slides(mode):
                report = clip_check.check(str(path), in_place=mode, skinned_soles=False)
                return [f for f in report["findings"] if f["check"] == "foot_slide"]

            self.assertTrue(slides("no"))  # in root-motion space the riding feet glide with the hips
            self.assertFalse(slides("linear_drift"))  # in place, as the game plays it, they stand still


if __name__ == "__main__":
    unittest.main()
