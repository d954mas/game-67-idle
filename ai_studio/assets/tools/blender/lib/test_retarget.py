import tempfile
import unittest
from pathlib import Path

import numpy as np

from ai_studio.assets.tools.blender.lib import glb as G, retarget as RT

# Mixamo-named T-pose humanoid facing +Z, Y up: name -> (parent, world position).
SKELETON = {
    "Hips": (None, (0.0, 1.0, 0.0)),
    "Spine": ("Hips", (0.0, 1.1, 0.0)), "Spine1": ("Spine", (0.0, 1.2, 0.01)), "Spine2": ("Spine1", (0.0, 1.32, 0.0)),
    "Neck": ("Spine2", (0.0, 1.5, 0.0)), "Head": ("Neck", (0.0, 1.6, 0.02)), "HeadTop_End": ("Head", (0.0, 1.8, 0.02)),
}
for _side, _x in (("Left", 1.0), ("Right", -1.0)):
    SKELETON.update({
        f"{_side}Shoulder": ("Spine2", (0.05 * _x, 1.45, 0.0)), f"{_side}Arm": (f"{_side}Shoulder", (0.2 * _x, 1.45, 0.0)),
        f"{_side}ForeArm": (f"{_side}Arm", (0.45 * _x, 1.45, 0.0)), f"{_side}Hand": (f"{_side}ForeArm", (0.7 * _x, 1.45, 0.0)),
        f"{_side}HandMiddle1": (f"{_side}Hand", (0.8 * _x, 1.45, 0.0)),
        f"{_side}HandThumb1": (f"{_side}Hand", (0.73 * _x, 1.44, 0.04)),
        f"{_side}UpLeg": ("Hips", (0.1 * _x, 0.95, 0.0)), f"{_side}Leg": (f"{_side}UpLeg", (0.1 * _x, 0.52, 0.01)),
        f"{_side}Foot": (f"{_side}Leg", (0.1 * _x, 0.08, 0.0)), f"{_side}ToeBase": (f"{_side}Foot", (0.1 * _x, 0.02, 0.12)),
        f"{_side}Toe_End": (f"{_side}ToeBase", (0.1 * _x, 0.02, 0.2)),
    })
ARM_CHAIN = ("Arm", "ForeArm", "Hand", "HandMiddle1", "HandThumb1")


def rot(axis, degrees):
    axis = np.asarray(axis, float) / np.linalg.norm(axis)
    k = np.array([[0, -axis[2], axis[1]], [axis[2], 0, -axis[0]], [-axis[1], axis[0], 0]])
    a = np.radians(degrees)
    return np.eye(3) + np.sin(a) * k + (1 - np.cos(a)) * k @ k


def positions(a_pose=0.0, scale=1.0, facing=0.0):
    """World positions with the arms lowered by a_pose degrees, scaled, turned facing degrees about up."""
    pos = {n: np.array(p, float) for n, (_, p) in SKELETON.items()}
    for side, sign in (("Left", -1.0), ("Right", 1.0)):
        pivot = pos[f"{side}Arm"].copy()
        r = rot((0, 0, 1), sign * a_pose)
        for j in ARM_CHAIN:
            pos[f"{side}{j}"] = pivot + r @ (pos[f"{side}{j}"] - pivot)
    turn = rot((0, 1, 0), facing)
    return {n: turn @ (p * scale) for n, p in pos.items()}


def build_rig(pos, prefix="mixamorig:", seed=0, names=None):
    """GlbBuilder rig with random joint frames (generator rigs disagree on local axes); returns the builder
    and {name: node}."""
    rng = np.random.default_rng(seed)
    world_rot = {}
    for n in SKELETON:
        q = rng.normal(size=4)
        world_rot[n] = G.quat_matrix(q / np.linalg.norm(q))
    builder, nodes = G.GlbBuilder(), {}
    for n, (parent, _) in SKELETON.items():
        if parent is None:
            t, r = pos[n], world_rot[n]
        else:
            wp = world_rot[parent]
            t, r = wp.T @ (pos[n] - pos[parent]), wp.T @ world_rot[n]
        label = (names or {}).get(n, prefix + n)
        nodes[n] = builder.add_node(label, translation=t, rotation=G.matrix_to_quat(r),
                                    parent=nodes[parent] if parent else None)
    return builder, nodes


def add_motion(builder, nodes, frames=31, fps=30.0, travel=0.0, legs=1.0, seed=1):
    """A clip of sinusoidal local rotations on every joint plus hips travel along +Z."""
    rng = np.random.default_rng(seed)
    times = np.arange(frames) / fps
    rotations, translations = {}, {}
    doc = builder.json["nodes"]
    for n, i in nodes.items():
        if n.endswith(("_End", "HandMiddle1", "HandThumb1")):
            continue
        amp = 25.0 * (legs if any(k in n for k in ("Hips", "Leg", "Foot", "Toe")) else 1.0)
        axis = rng.normal(size=3)
        phase = rng.uniform(0, 2 * np.pi)
        rest = G.quat_matrix(doc[i]["rotation"])
        mats = np.stack([rest @ rot(axis, amp * np.sin(2 * np.pi * t + phase)) for t in times])
        rotations[i] = RT.quats(mats)
    base = np.array(doc[nodes["Hips"]]["translation"])
    translations[nodes["Hips"]] = base + np.c_[np.zeros(frames), 0.02 * np.sin(4 * np.pi * times), travel * times]
    RT.add_animation(builder, RT.ClipResult("move", times, rotations, translations, {}))
    return times


def loaded(builder):
    with tempfile.TemporaryDirectory() as tmp:
        return G.Glb.load(builder.write(Path(tmp) / "rig.glb"))


def joint_paths(glb, result, names):
    worlds = glb.posed_worlds({(i, p): (result.times, v, "LINEAR") for p, tracks in
                               (("rotation", result.rotations), ("translation", result.translations))
                               for i, v in tracks.items()}, result.times)
    return {n: worlds[glb.by_name[n]][:, :3, 3] for n in names}


def source_paths(glb, names, times):
    worlds = glb.posed_worlds(glb.channels(0, by_index=True), times)
    return {n: worlds[glb.by_name[n]][:, :3, 3] for n in names}


def directions(paths, a, b):
    v = paths[b] - paths[a]
    return v / np.linalg.norm(v, axis=1, keepdims=True)


class RetargetTests(unittest.TestCase):
    def setUp(self):
        builder, nodes = build_rig(positions())
        self.times = add_motion(builder, nodes)
        self.source = loaded(builder)

    def target(self, **kwargs):
        builder, _ = build_rig(positions(**kwargs), seed=7)
        return loaded(builder)

    def test_identity_retarget_reproduces_the_clip(self):
        target = loaded(build_rig(positions())[0])
        result = RT.Retargeter(self.source, target).clip(self.source, 0, legs=False)
        names = [f"mixamorig:{n}" for n in SKELETON]
        ours, theirs = joint_paths(target, result, names), source_paths(self.source, names, result.times)
        error = max(float(np.abs(ours[n] - theirs[n]).max()) for n in names)
        self.assertLess(error, 1e-5)
        with_legs = RT.Retargeter(self.source, target).clip(self.source, 0)
        ours = joint_paths(target, with_legs, names)
        self.assertLess(max(float(np.abs(ours[n] - theirs[n]).max()) for n in names), 5e-3)

    def test_a_pose_target_gets_the_source_bone_directions(self):
        target = self.target(a_pose=45.0)
        result = RT.Retargeter(self.source, target).clip(self.source, 0)
        names = [f"mixamorig:{n}" for n in SKELETON]
        ours, theirs = joint_paths(target, result, names), source_paths(self.source, names, result.times)
        for side in ("Left", "Right"):
            for a, b in (("Arm", "ForeArm"), ("ForeArm", "Hand"), ("UpLeg", "Leg"), ("Leg", "Foot")):
                a, b = f"mixamorig:{side}{a}", f"mixamorig:{side}{b}"
                cos = (directions(ours, a, b) * directions(theirs, a, b)).sum(axis=1)
                self.assertGreater(cos.min(), np.cos(np.radians(0.5)), f"{a}->{b}")

    def test_turned_target_keeps_its_facing(self):
        target = self.target(facing=-90.0)
        result = RT.Retargeter(self.source, target).clip(self.source, 0)
        names = ["mixamorig:LeftArm", "mixamorig:LeftForeArm", "mixamorig:Hips"]
        ours, theirs = joint_paths(target, result, names), source_paths(self.source, names, result.times)
        turn = rot((0, 1, 0), -90.0)
        want = directions(theirs, *names[:2]) @ turn.T
        self.assertGreater((directions(ours, *names[:2]) * want).sum(axis=1).min(), np.cos(np.radians(0.5)))

    def test_scaled_target_scales_root_travel(self):
        builder, nodes = build_rig(positions())
        add_motion(builder, nodes, travel=0.5)
        source = loaded(builder)
        target = self.target(scale=2.0)
        result = RT.Retargeter(source, target).clip(source, 0)
        self.assertAlmostEqual(result.report["scale"], 2.0, places=3)
        hips = target.by_name["mixamorig:Hips"]
        ours = joint_paths(target, result, ["mixamorig:Hips"])["mixamorig:Hips"]
        theirs = source_paths(source, ["mixamorig:Hips"], result.times)["mixamorig:Hips"]
        self.assertAlmostEqual(float(ours[-1, 2] - ours[0, 2]) / float(theirs[-1, 2] - theirs[0, 2]), 2.0, places=3)
        in_place = RT.Retargeter(source, target).clip(source, 0, root="in_place")
        path = in_place.translations[hips]
        self.assertLess(abs(float(path[-1, 2] - path[0, 2])), 1e-6)

    def test_foot_lock_reduces_slide(self):
        builder, nodes = build_rig(positions())
        add_motion(builder, nodes, travel=0.2, legs=0.05)
        source = loaded(builder)
        target = self.target(a_pose=30.0)
        plain = RT.Retargeter(source, target).clip(source, 0).report["feet"]
        locked = RT.Retargeter(source, target).clip(source, 0, foot_lock=True).report["feet"]
        for side in ("left", "right"):
            self.assertGreater(plain[side]["planted_frames"], 10)
            self.assertGreater(plain[side]["slide_before_m"], 0.03)
            self.assertLess(locked[side]["slide_after_m"], 0.2 * plain[side]["slide_before_m"])

    def test_untwist_keeps_shin_direction(self):
        target = self.target()
        plain = RT.Retargeter(self.source, target).clip(self.source, 0, legs=False)
        untwisted = RT.Retargeter(self.source, target).clip(self.source, 0, legs=False, untwist=RT.HINGES)
        names = ["mixamorig:LeftLeg", "mixamorig:LeftFoot"]
        a, b = joint_paths(target, plain, names), joint_paths(target, untwisted, names)
        self.assertLess(float(np.abs(a["mixamorig:LeftFoot"] - b["mixamorig:LeftFoot"]).max()), 1e-6)

    def test_map_auto_detect(self):
        maps = RT.load_maps()
        for prefix in ("mixamorig:", "", "mixamorig1:"):
            rig = loaded(build_rig(positions(), prefix=prefix)[0])
            chosen, resolved, rows = RT.detect_map(rig, maps)
            self.assertEqual(chosen.id, "mixamo")
            self.assertEqual(rows[0]["missing_required"], [])
            self.assertEqual(rig.name(resolved["left_lower_leg"]), f"{prefix}LeftLeg")
        hero = {"Hips": "Pelvis", "Spine2": "Chest", "LeftArm": "UpperArm_L", "LeftForeArm": "Forearm_L",
                "LeftHand": "Hand_L", "RightArm": "UpperArm_R", "RightForeArm": "Forearm_R", "RightHand": "Hand_R",
                "LeftUpLeg": "Thigh_L", "LeftLeg": "Calf_L", "LeftFoot": "Foot_L", "RightUpLeg": "Thigh_R",
                "RightLeg": "Calf_R", "RightFoot": "Foot_R", "Spine": "Spine", "Neck": "Neck", "Head": "Head"}
        names = {n: hero.get(n, f"Volume_{n}") for n in SKELETON}
        self.assertEqual(RT.detect_map(loaded(build_rig(positions(), names=names)[0]), maps)[0].id, "hero17")
        soma = {"LeftUpLeg": "LeftLeg", "LeftLeg": "LeftShin", "RightUpLeg": "RightLeg", "RightLeg": "RightShin",
                "Spine": "Spine1", "Spine1": "Spine2", "Spine2": "Chest", "Neck": "Neck1"}
        rig = loaded(build_rig(positions(), names={n: soma.get(n, n) for n in SKELETON})[0])
        chosen, resolved, _ = RT.detect_map(rig, maps)
        self.assertEqual(chosen.id, "soma")
        self.assertEqual(rig.name(resolved["left_upper_leg"]), "LeftLeg")

    def test_written_files_play_back_the_result(self):
        target_builder, _ = build_rig(positions(a_pose=40.0), seed=3)
        mesh = target_builder.add_mesh("Box", [[0, 0, 0], [1, 0, 0], [0, 1, 0]], [[0, 1, 2]])
        target_builder.add_node("Body", mesh=mesh)
        target = loaded(target_builder)
        result = RT.Retargeter(self.source, target).clip(self.source, 0, foot_lock=True)
        names = [f"mixamorig:{n}" for n in SKELETON]
        want = joint_paths(target, result, names)
        with tempfile.TemporaryDirectory() as tmp:
            for mode in ("full", "clip"):
                out = G.Glb.load(RT.write(target, [result, result], Path(tmp) / f"{mode}.glb", mode))
                self.assertEqual(out.animation_names(), ["move", "move_2"])
                self.assertEqual(any("mesh" in n for n in out.nodes), mode == "full")
                worlds = target.posed_worlds(out.channels("move"), result.times)
                for n in names:
                    self.assertLess(float(np.abs(worlds[target.by_name[n]][:, :3, 3] - want[n]).max()), 1e-5)

    def test_single_key_pose_writes_one_key(self):
        builder, nodes = build_rig(positions())
        doc = builder.json["nodes"]
        rotations = {i: np.array([doc[i]["rotation"]]) for i in nodes.values()}
        hips = nodes["Hips"]
        RT.add_animation(builder, RT.ClipResult("pose", np.zeros(1), rotations,
                                                {hips: np.array([doc[hips]["translation"]])}, {}))
        source = loaded(builder)
        target = self.target(a_pose=30.0)
        result = RT.Retargeter(source, target).clip(source, 0, foot_lock=True)
        self.assertEqual(result.times.tolist(), [0.0])
        with tempfile.TemporaryDirectory() as tmp:
            out = G.Glb.load(RT.write(target, [result], Path(tmp) / "pose.glb", "clip"))
            for times, _, _ in out.channels(0).values():
                self.assertEqual(len(times), 1)

    def test_rest_differences_stay_below_the_shoulder_and_hip(self):
        pos = positions()
        for side, x in (("Left", 1.0), ("Right", -1.0)):
            pos[f"{side}HandThumb1"] = np.array([0.73 * x, 1.49, 0.0])  # palms forward: thumbs up
            ankle = pos[f"{side}Foot"].copy()
            for j in ("Foot", "ToeBase", "Toe_End"):  # feet toed out 30 degrees
                pos[f"{side}{j}"] = ankle + rot((0, 1, 0), 30.0 * x) @ (pos[f"{side}{j}"] - ankle)
        target = loaded(build_rig(pos, seed=7)[0])
        builder, nodes = build_rig(positions())
        doc = builder.json["nodes"]
        rotations = {i: np.repeat([doc[i]["rotation"]], 2, axis=0) for i in nodes.values()}
        RT.add_animation(builder, RT.ClipResult("rest", np.array([0.0, 1.0]), rotations, {}, {}))
        source = loaded(builder)
        result = RT.Retargeter(source, target).clip(source, 0, legs=False)
        for joint in ("LeftArm", "RightArm", "LeftUpLeg", "RightUpLeg", "LeftLeg"):
            i = target.by_name[f"mixamorig:{joint}"]
            self.assertLess(G.quat_angle_deg(result.rotations[i][0], target.local_trs(i)[1]), 1.0, joint)
        thumb = joint_paths(target, result, ["mixamorig:LeftHand", "mixamorig:LeftHandThumb1"])
        side = thumb["mixamorig:LeftHandThumb1"][0] - thumb["mixamorig:LeftHand"][0]
        self.assertGreater(side[2], abs(side[1]))  # the source's palms-down thumb points forward

    def test_z_up_target_finds_its_plants(self):
        builder, nodes = build_rig(positions())
        add_motion(builder, nodes, travel=0.2, legs=0.05)
        source = loaded(builder)
        z_up = {n: rot((1, 0, 0), 90.0) @ p for n, p in positions().items()}
        target = loaded(build_rig(z_up, seed=5)[0])
        report = RT.Retargeter(source, target).clip(source, 0, foot_lock=True).report
        for side in ("left", "right"):
            self.assertGreater(report["feet"][side]["planted_frames"], 10)
            self.assertLess(report["feet"][side]["slide_after_m"], 0.2 * report["feet"][side]["slide_before_m"])

    def test_animated_matrix_nodes_are_written_as_trs(self):
        builder, nodes = build_rig(positions(), seed=4)
        for n in ("Hips", "LeftArm"):
            node = builder.json["nodes"][nodes[n]]
            m = G.trs_matrix(node.pop("translation"), node.pop("rotation"), [1.0, 1.0, 1.0])
            node["matrix"] = m.T.reshape(-1).tolist()
        target = loaded(builder)
        result = RT.Retargeter(self.source, target).clip(self.source, 0)
        names = [f"mixamorig:{n}" for n in SKELETON]
        want = joint_paths(target, result, names)
        with tempfile.TemporaryDirectory() as tmp:
            out = G.Glb.load(RT.write(target, [result], Path(tmp) / "out.glb", "clip"))
            animated = {c["target"]["node"] for c in out.json["animations"][0]["channels"]}
            self.assertFalse(any("matrix" in out.nodes[i] for i in animated))
            worlds = out.posed_worlds(out.channels(0, by_index=True), result.times)
            for n in names:
                self.assertLess(float(np.abs(worlds[out.by_name[n]][:, :3, 3] - want[n]).max()), 1e-5)

    def test_warnings_for_a_lock_without_plants_and_a_posed_rest(self):
        builder, nodes = build_rig(positions())
        add_motion(builder, nodes, travel=3.0, legs=0.05)
        source = loaded(builder)
        report = RT.Retargeter(source, self.target()).clip(source, 0, root="in_place", foot_lock=True).report
        self.assertTrue(any("no planted frames" in w for w in report["warnings"]))
        self.assertFalse(any("rest equals its first frame" in w for w in report["warnings"]))
        builder, nodes = build_rig(positions())
        add_motion(builder, nodes)
        for (i, path), (_, values, _) in loaded(builder).channels(0, by_index=True).items():
            builder.json["nodes"][i][path] = values[0].tolist()  # frame 0 baked into the rest
        posed = loaded(builder)
        report = RT.Retargeter(posed, self.target()).clip(posed, 0).report
        self.assertTrue(any("rest equals its first frame" in w for w in report["warnings"]))

    def test_out_dir_names_sources_that_share_a_name_apart(self):
        from ai_studio.assets.tools.blender import retarget as cli
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            for folder in ("a", "b"):
                (tmp / folder).mkdir()
                builder, nodes = build_rig(positions())
                add_motion(builder, nodes, seed=3 if folder == "a" else 4)
                builder.write(tmp / folder / "walk.glb")
            builder, _ = build_rig(positions(a_pose=30.0), seed=7)
            builder.write(tmp / "body.glb")
            cli.run(cli.parse(["--source", str(tmp / "a" / "walk.glb"), str(tmp / "b" / "walk.glb"),
                               "--target", str(tmp / "body.glb"), "--out-dir", str(tmp / "out"), "--no-check"]))
            self.assertEqual(sorted(p.name for p in (tmp / "out").iterdir()), ["a__walk.glb", "b__walk.glb"])


if __name__ == "__main__":
    unittest.main()
