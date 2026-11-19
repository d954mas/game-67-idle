import unittest

import numpy as np

from ai_studio.assets.tools.blender import clip_check
from ai_studio.assets.tools.blender.lib import glb as G

UNIT = np.array([[x, y, z] for x in (0, 1) for y in (0, 1) for z in (0, 1)], float)
FACES = np.array([[1, 2, 0], [3, 2, 1], [6, 5, 4], [6, 7, 5], [4, 1, 0], [4, 5, 1],
                  [3, 6, 2], [7, 6, 3], [2, 4, 0], [6, 4, 2], [5, 3, 1], [5, 7, 3]])
JOINTS = {"Hips": (0, 0.9, 0), "LeftFoot": (0.1, -0.82, 0), "RightFoot": (-0.1, -0.82, 0)}
FPS = 30
T = np.arange(FPS + 1) / FPS


def box(lo, hi):
    return np.asarray(lo, float) + UNIT * (np.asarray(hi, float) - lo)


def add_rig(b, skinned=True, unweighted=0):
    hips = b.add_node("Hips", translation=JOINTS["Hips"])
    feet = [b.add_node(n, translation=JOINTS[n], parent=hips) for n in ("LeftFoot", "RightFoot")]
    if not skinned:
        return
    world = {"Hips": np.array(JOINTS["Hips"]), "LeftFoot": np.array([0.1, 0.08, 0]),
             "RightFoot": np.array([-0.1, 0.08, 0])}
    points = np.vstack([box((-0.15, 0.6, -0.1), (0.15, 1.2, 0.1)), box((0.05, 0, -0.05), (0.15, 0.1, 0.12)),
                        box((-0.15, 0, -0.05), (-0.05, 0.1, 0.12))])
    joints = np.repeat([[0, 0, 0, 0], [1, 0, 0, 0], [2, 0, 0, 0]], 8, axis=0)
    weights = np.tile([1.0, 0, 0, 0], (24, 1))
    weights[:unweighted] = 0.0
    mesh = b.add_mesh("body", points, np.vstack([FACES, FACES + 8, FACES + 16]),
                      extra={"JOINTS_0": (joints, G.UNSIGNED_BYTE), "WEIGHTS_0": (weights, G.FLOAT)})
    ibm = [np.linalg.inv(G.trs_matrix(world[n], (0, 0, 0, 1), (1, 1, 1))).T.reshape(-1) for n in JOINTS]
    b.json["skins"] = [{"joints": [hips] + feet, "inverseBindMatrices": b.add_accessor(np.array(ibm), kind="MAT4")}]
    b.add_node("body", mesh=mesh, skin=0)


def add_clip(b, name, tracks):
    """tracks: {node name: values (len(T), 3)} of translation keys at 30 fps."""
    times = b.add_accessor(T, kind="SCALAR", bounds=True)
    anim = {"name": name, "samplers": [], "channels": []}
    for node, values in tracks.items():
        anim["samplers"].append({"input": times, "output": b.add_accessor(np.asarray(values, float))})
        anim["channels"].append({"sampler": len(anim["samplers"]) - 1,
                                 "target": {"node": b.json["nodes"].index(next(
                                     n for n in b.json["nodes"] if n["name"] == node)), "path": "translation"}})
    b.json.setdefault("animations", []).append(anim)


def hips(dy=0.0, dx=None):
    values = np.tile(JOINTS["Hips"], (len(T), 1)).astype(float)
    values[:, 1] += dy
    if dx is not None:
        values[:, 0] += dx
    return values


def character(unweighted=0, **clips):
    b = G.GlbBuilder()
    add_rig(b, unweighted=unweighted)
    for name, tracks in clips.items():
        add_clip(b, name, tracks)
    return b.to_glb()


def found(report, severity=None):
    return {f["check"] for f in report["findings"] if severity is None or f["severity"] == severity}


def serious(report):
    return found(report, "medium") | found(report, "high")


BOB = {"Hips": hips(0.01 * np.sin(2 * np.pi * T)[:, None][:, 0])}
SLIDE = {"LeftFoot": np.c_[np.linspace(0.1, 0.3, len(T)), np.full(len(T), -0.82), np.zeros(len(T))]}


class ClipCheck(unittest.TestCase):
    def test_unweighted_vertex_stays_at_origin_while_body_lifts(self):
        report = clip_check.check(character(unweighted=1, Lift={"Hips": hips(0.5)}))
        self.assertNotIn("check_error", found(report))
        row = report["metrics"]["per_animation"][0]
        self.assertEqual((row["lowest_y"], row["lowest_source"]), (0.0, "skinned mesh"))

    def test_idle_is_clean_and_loops(self):
        report = clip_check.check(character(Idle=BOB), loop=True)
        self.assertEqual(serious(report), set())
        row = report["metrics"]["per_animation"][0]
        self.assertEqual(row["foot_joints"], ["LeftFoot", "RightFoot"])
        self.assertEqual(row["lowest_source"], "skinned mesh")

    def test_sliding_foot(self):
        report = clip_check.check(character(Slide=SLIDE), loop=True)
        self.assertTrue({"foot_slide", "loop_seam"} <= serious(report))

    def test_in_place_walk_is_judged_on_its_treadmill(self):
        # Both feet glide back at 0.4 m/s while low and swing forward while lifted: a treadmill stride.
        def foot(x0, offset):
            p = (T + offset) % 1.0
            z = np.where(p < 0.5, 0.1 - 0.4 * p, -0.1 + 0.4 * (p - 0.5))
            y = np.where(p < 0.5, -0.82, -0.82 + 0.15 * np.sin((p - 0.5) * 2 * np.pi))
            return np.c_[np.full(len(T), x0), y, z]
        walk = {"LeftFoot": foot(0.1, 0.0), "RightFoot": foot(-0.1, 0.5)}
        report = clip_check.check(character(Walk=walk))
        self.assertNotIn("foot_slide", found(report))
        self.assertTrue(report["metrics"]["per_animation"][0]["in_place"])
        self.assertIn("foot_slide", found(clip_check.check(character(Walk=walk), in_place="no")))

    def test_lifted_foot_moving_low_is_a_step_not_a_slide(self):
        # The foot drifts 20 cm while 5 cm up (a lunge's back foot), then sets down: its ankle stays
        # within the old ankle-height plant band, its sole does not touch.
        lift = np.where(T < 0.6, 0.05, 0.0)
        drift = np.minimum(T, 0.6) / 0.6 * 0.2
        lunge = {"LeftFoot": np.c_[0.1 + drift, -0.82 + lift, np.zeros(len(T))]}
        report = clip_check.check(character(Lunge=lunge))
        self.assertNotIn("foot_slide", found(report))
        self.assertEqual(report["metrics"]["per_animation"][0]["feet"]["LeftFoot"]["points"], "sole")

    def test_foot_pivoting_on_its_toe_is_not_a_slide(self):
        b = G.GlbBuilder()
        add_rig(b)
        model = b.to_glb()
        toe = clip_check.sole_vertices(model, ["LeftFoot"], 0.01)["LeftFoot"][-1]
        pivot = model.skinned_points({i: m[None] for i, m in model.world_rest().items()},
                                     [model.by_name["body"]], [toe])[0, 0]
        angle = np.radians(60) * T
        rotation = np.c_[np.zeros(len(T)), np.sin(angle / 2), np.zeros(len(T)), np.cos(angle / 2)]
        offset = pivot - (np.array(JOINTS["Hips"]) + JOINTS["LeftFoot"])
        turned = np.einsum("fij,j->fi", G.quat_matrices(rotation), -offset)
        translation = pivot + turned - np.array(JOINTS["Hips"])
        add_clip(b, "Pivot", {"LeftFoot": translation})
        anim = b.json["animations"][0]
        anim["samplers"].append({"input": anim["samplers"][0]["input"], "output": b.add_accessor(rotation)})
        anim["channels"].append({"sampler": 1, "target": {"node": model.by_name["LeftFoot"], "path": "rotation"}})
        report = clip_check.check(b.to_glb())
        self.assertNotIn("foot_slide", found(report))
        self.assertGreater(report["metrics"]["per_animation"][0]["feet"]["LeftFoot"]["planted_frames"], 20)
        self.assertIn("foot_slide", serious(clip_check.check(character(Slide=SLIDE))))

    def test_airborne_clips_skip_floor_and_slide(self):
        held = {"Carried": {"Hips": hips(0.5)}, "Dropped": {"Hips": hips(-0.2)}, "Slide": SLIDE}
        report = clip_check.check(character(**held), airborne=["Carr*", "Drop*"])
        self.assertIn("airborne", found(report, "info"))
        self.assertEqual(found(report) & {"sole_float", "sole_sink"}, set())
        self.assertIn("foot_slide", serious(report))
        plain = clip_check.check(character(**held))
        self.assertTrue({"sole_float", "sole_sink"} <= found(plain))
        floating = next(f for f in plain["findings"] if f["check"] == "sole_float")
        self.assertRegex(floating["message"], r"within 0\.\d{3} m ")

    def test_sink_reports_duration_part_and_game_metres(self):
        report = clip_check.check(character(Sink={"Hips": hips(-0.15)}))
        sink = next(f for f in report["findings"] if f["check"] == "sole_sink")
        self.assertEqual((sink["severity"], sink["where"]["part"]), ("medium", "sole"))
        self.assertAlmostEqual(sink["where"]["duration"], 1.0 + 1 / FPS, places=3)
        self.assertAlmostEqual(sink["value"], 0.15, places=3)
        scaled = clip_check.check(character(Sink={"Hips": hips(-0.15)}), model_scale=2.0)
        sink = next(f for f in scaled["findings"] if f["check"] == "sole_sink")
        self.assertAlmostEqual(sink["value"], 0.3, places=3)
        self.assertIn("game m", sink["message"])
        self.assertEqual(scaled["metrics"]["units"], "game m")

    def test_short_deep_dip_is_low(self):
        dip = np.zeros(len(T))
        dip[10:16] = -0.3  # 0.2 s: a foot flicks through the floor mid-stride
        sink = next(f for f in clip_check.check(character(Dodge={"Hips": hips(dip)}))["findings"]
                    if f["check"] == "sole_sink")
        self.assertEqual(sink["severity"], "low")

    def test_brief_shallow_dip_is_low(self):
        dip = np.zeros(len(T))
        dip[15:17] = -0.025
        report = clip_check.check(character(Land={"Hips": hips(dip)}))
        sink = next(f for f in report["findings"] if f["check"] == "sole_sink")
        self.assertEqual(sink["severity"], "low")
        self.assertLess(sink["where"]["duration"], 0.1)

    def test_sink_against_a_baseline_clip(self):
        clips = {"Idle": {"Hips": hips(-0.05)}, "Crouch": {"Hips": hips(-0.08)}}
        report = clip_check.check(character(**clips), baseline="Idle")
        rows = {r["animation"]: r for r in report["metrics"]["per_animation"]}
        self.assertAlmostEqual(rows["Crouch"]["dip_m"], 0.03, places=3)
        self.assertAlmostEqual(rows["Idle"]["dip_m"], 0.05, places=3)  # the baseline itself meets the floor
        sink = next(f for f in report["findings"] if f["check"] == "sole_sink")
        crouch = next(i for i in sink["where"]["items"] if i["animation"] == "Crouch")
        self.assertIn("Idle baseline", crouch["message"])
        missing = clip_check.check(character(**clips), baseline="Stand")
        self.assertIn("baseline", found(missing, "info"))

    def test_toe_spike_at_a_foot_contact_is_not_a_pop(self):
        b = G.GlbBuilder()
        add_rig(b)
        foot = next(i for i, n in enumerate(b.json["nodes"]) if n["name"] == "LeftFoot")
        toe = b.add_node("LeftToeBase", translation=(0, -0.05, 0.1), parent=foot)
        b.add_node("LeftToe_End", translation=(0, 0, 0.05), parent=toe)
        spike = np.tile([0, -0.05, 0.1], (len(T), 1)).astype(float)
        spike[15, 1] += 0.2
        add_clip(b, "Stomp", {"LeftToeBase": spike})
        report = clip_check.check(b.to_glb())
        self.assertNotIn("pop", found(report))
        self.assertIn("pop", serious(clip_check.check(character(Pop={"Hips": hips(np.where(np.arange(len(T)) == 15, 0.2, 0))}))))

    def test_short_clip_pop_limit_grows(self):
        row = {}
        times = np.arange(7) / FPS
        clip_check.check_pops(["Hips"], np.zeros((7, 1, 3)), times, {"pop_accel": 180.0, "pop_window": 6,
                                                                       "pop_ratio": 4.0}, "Jab", [], row)
        self.assertAlmostEqual(row["pop_limit_mps2"], 180.0 * (0.4 / 0.2) ** 2, places=1)

    def test_popping_joint(self):
        spike = np.zeros(len(T))
        spike[15] = 0.2
        self.assertIn("pop", serious(clip_check.check(character(Pop={"Hips": hips(spike)}))))

    def test_teleport_and_sink(self):
        jump = np.where(T > 0.5, 1.0, 0.0)
        self.assertIn("teleport", found(clip_check.check(character(Jump={"Hips": hips(dx=jump)})), "high"))
        self.assertIn("sole_sink", serious(clip_check.check(character(Sink={"Hips": hips(-0.15)}))))
        self.assertNotIn("sole_sink", found(clip_check.check(character(Sink={"Hips": hips(-0.1)}), sink=0.2)))

    def test_nan_keys(self):
        bad = hips()
        bad[3, 1] = np.nan
        self.assertIn("finite", found(clip_check.check(character(Broken={"Hips": bad})), "high"))

    def test_clip_file_on_model(self):
        clip = G.GlbBuilder()
        add_rig(clip, skinned=False)
        add_clip(clip, "Slide", SLIDE)
        model = character()
        report = clip_check.check(clip.to_glb(), model=model)
        self.assertIn("foot_slide", serious(report))

    def test_foot_names(self):
        self.assertEqual(clip_check.detect_feet(["mixamorig:LeftFoot", "mixamorig:LeftToeBase",
                                                 "mixamorig:RightFoot", "mixamorig:Hips"]),
                         ["mixamorig:LeftFoot", "mixamorig:RightFoot"])
        self.assertEqual(clip_check.detect_feet(["foot_l", "foot_r", "ball_l", "ik_foot_l"]), ["foot_l", "foot_r"])
        self.assertEqual(clip_check.detect_feet(["Bip01 L Foot", "Bip01 R Foot"]), ["Bip01 L Foot", "Bip01 R Foot"])
        self.assertEqual(clip_check.detect_feet(["DEF-foot.L", "DEF-foot.R", "DEF-toe.L"]), ["DEF-foot.L", "DEF-foot.R"])


class Malformed(unittest.TestCase):
    def test_joint_outside_scene_or_bad_index_is_a_finding(self):
        glb = character(Idle=BOB)
        glb.json["scenes"][0]["nodes"] = [n for n in glb.json["scenes"][0]["nodes"]
                                          if glb.json["nodes"][n]["name"] != "Hips"]
        report = clip_check.check(glb)
        self.assertTrue({"skin", "joints_in_scene"} <= found(report, "high"))
        glb = character(Idle=BOB)
        glb.json["skins"][0]["joints"].append(99)
        self.assertIn("high", {f["severity"] for f in clip_check.check(glb)["findings"]})
        broken = character(Idle=BOB)
        joints = broken.accessor(broken.json["meshes"][0]["primitives"][0]["attributes"]["JOINTS_0"])
        view = broken.json["bufferViews"][broken.json["accessors"][
            broken.json["meshes"][0]["primitives"][0]["attributes"]["JOINTS_0"]]["bufferView"]]
        data = bytearray(broken.bin)
        data[view["byteOffset"]] = 9  # first vertex weighted to joint 9 of 3
        broken.bin = bytes(data)
        self.assertEqual(len(joints), 24)
        self.assertIn("skin", found(clip_check.check(broken), "high"))


class Channels(unittest.TestCase):
    def test_cubic_scale_tangents_are_not_values(self):
        b = G.GlbBuilder()
        add_rig(b)
        times = b.add_accessor(np.array([0.0, 1.0]), kind="SCALAR", bounds=True)
        keys = np.array([[0, 0, 0], [1, 1, 1], [0, 0, 0]] * 2, float)
        out = b.add_accessor(keys)
        b.json["animations"] = [{"name": "Hold", "samplers": [{"input": times, "output": out,
                                                              "interpolation": "CUBICSPLINE"}],
                                 "channels": [{"sampler": 0, "target": {"node": 0, "path": "scale"}}]}]
        self.assertNotIn("scale", found(clip_check.check(b.to_glb())))

    def test_duplicate_names_move_only_the_target(self):
        b = G.GlbBuilder()
        first = b.add_node("Object", translation=(0, 0, 0))
        second = b.add_node("Object", translation=(5, 0, 0))
        times = b.add_accessor(np.array([0.0, 1.0]), kind="SCALAR", bounds=True)
        moves = b.add_accessor(np.array([[5, 0, 0], [5, 3, 0]], float))
        b.json["animations"] = [{"name": "Lift", "samplers": [{"input": times, "output": moves}],
                                 "channels": [{"sampler": 0, "target": {"node": second, "path": "translation"}}]}]
        glb = b.to_glb()
        world = glb.posed_world(glb.channels("Lift", by_index=True), 1.0)
        np.testing.assert_allclose(world[first][:3, 3], [0, 0, 0])
        np.testing.assert_allclose(world[second][:3, 3], [5, 3, 0])
        model = G.GlbBuilder()
        model.add_node("Object")
        model.add_node("Object")
        report = clip_check.check(glb, model=model.to_glb())
        self.assertIn("ambiguous_targets", found(report, "medium"))

    def test_findings_group_by_cause_across_clips(self):
        report = clip_check.check(character(SlideA=SLIDE, SlideB=SLIDE))
        slides = [f for f in report["findings"] if f["check"] == "foot_slide"]
        self.assertEqual(len(slides), 1)
        self.assertEqual(slides[0]["where"]["clips"], ["SlideA", "SlideB"])


if __name__ == "__main__":
    unittest.main()
