import tempfile
import unittest
from pathlib import Path

import numpy as np

from ai_studio.assets.tools.blender.lib import glb as G

CUBE_POSITIONS = np.array([[x, y, z] for x in (0, 1) for y in (0, 1) for z in (0, 1)], float)
CUBE_FACES = np.array([[0, 2, 1], [1, 2, 3], [4, 5, 6], [5, 7, 6], [0, 1, 4], [1, 5, 4],
                       [2, 6, 3], [3, 6, 7], [0, 4, 2], [2, 4, 6], [1, 3, 5], [3, 7, 5]])


class ReadWrite(unittest.TestCase):
    def test_round_trip_keeps_geometry_and_hierarchy(self):
        b = G.GlbBuilder()
        mesh = b.add_mesh("cube", CUBE_POSITIONS, CUBE_FACES)
        parent = b.add_node("parent", translation=(0, 2, 0))
        b.add_node("child", mesh=mesh, translation=(1, 0, 0), scale=(2, 2, 2), parent=parent)
        with tempfile.TemporaryDirectory() as tmp:
            path = b.write(Path(tmp) / "cube.glb")
            g = G.Glb.load(path)
        parts = g.parts()
        self.assertEqual(len(parts), 1)
        self.assertEqual(parts[0].name, "child")
        np.testing.assert_allclose(parts[0].positions.min(axis=0), [1, 2, 0])
        np.testing.assert_allclose(parts[0].positions.max(axis=0), [3, 4, 2])
        np.testing.assert_array_equal(parts[0].faces, CUBE_FACES)

    def test_rejects_non_glb(self):
        with self.assertRaises(G.GlbError):
            G.parse_glb(b"not a glb at all, just bytes")

    def test_normalized_and_strided_accessors(self):
        b = G.GlbBuilder()
        idx = b.add_accessor(np.array([[0, 255], [128, 64]]), G.UNSIGNED_BYTE, "VEC2", normalized=True)
        g = b.to_glb()
        np.testing.assert_allclose(g.accessor(idx), [[0, 1], [128 / 255, 64 / 255]])
        # Interleave two VEC3 float streams in one view to exercise byteStride.
        data = np.arange(12, dtype="<f4").reshape(2, 6)
        g.json["bufferViews"].append({"buffer": 0, "byteOffset": len(g.bin), "byteLength": data.nbytes,
                                      "byteStride": 24})
        g.bin = g.bin + data.tobytes()
        g.json["accessors"].append({"bufferView": len(g.json["bufferViews"]) - 1, "byteOffset": 12,
                                    "componentType": G.FLOAT, "count": 2, "type": "VEC3"})
        np.testing.assert_allclose(g.accessor(len(g.json["accessors"]) - 1), [[3, 4, 5], [9, 10, 11]])

    def test_matrix_node_matches_trs(self):
        t, r, s = (1, 2, 3), (0, np.sin(0.3), 0, np.cos(0.3)), (2, 2, 2)
        m = G.trs_matrix(t, r, s)
        b = G.GlbBuilder()
        b.add_node("n", matrix=m.T.reshape(-1).tolist())
        g = b.to_glb()
        np.testing.assert_allclose(g.local_matrix(0), m, atol=1e-12)
        tt, rr, ss = g.local_trs(0)
        np.testing.assert_allclose(G.trs_matrix(tt, rr, ss), m, atol=1e-9)


class Animation(unittest.TestCase):
    def test_linear_rotation_takes_short_path(self):
        a = np.array([0, 0, 0, 1.0])
        b = -np.array([0, np.sin(0.5), 0, np.cos(0.5)])
        q = G.sample_channel([0, 1], np.stack([a, b]), 0.5, "rotation")
        self.assertAlmostEqual(G.quat_angle_deg(q, [0, np.sin(0.25), 0, np.cos(0.25)]), 0, places=4)

    def test_step_and_cubic(self):
        times = [0.0, 1.0]
        self.assertEqual(G.sample_channel(times, np.array([[0.0], [1.0]]), 0.9, "translation", "STEP")[0], 0.0)
        # in-tangent, value, out-tangent per key; zero tangents give smoothstep.
        cubic = np.array([[0.0], [0.0], [0.0], [0.0], [1.0], [0.0]])
        self.assertAlmostEqual(G.sample_channel(times, cubic, 0.5, "translation", "CUBICSPLINE")[0], 0.5)

    def test_skinned_positions_follow_joint(self):
        b = G.GlbBuilder()
        joints = np.zeros((3, 4), int)
        weights = np.tile([1.0, 0, 0, 0], (3, 1))
        mesh = b.add_mesh("tri", [[0, 0, 0], [1, 0, 0], [0, 1, 0]], [[0, 1, 2]],
                          extra={"JOINTS_0": (joints, G.UNSIGNED_BYTE), "WEIGHTS_0": (weights, G.FLOAT)})
        bone = b.add_node("bone")
        ibm = b.add_accessor(np.eye(4).reshape(1, 16), kind="MAT4")
        b.json["skins"] = [{"joints": [bone], "inverseBindMatrices": ibm}]
        b.add_node("body", mesh=mesh, skin=0)
        times = b.add_accessor(np.array([0.0, 1.0]), kind="SCALAR", bounds=True)
        moves = b.add_accessor(np.array([[0, 0, 0], [0, 5, 0]], float))
        b.json["animations"] = [{"name": "lift", "samplers": [{"input": times, "output": moves}],
                                 "channels": [{"sampler": 0, "target": {"node": bone, "path": "translation"}}]}]
        g = b.to_glb()
        self.assertEqual(g.animation_names(), ["lift"])
        self.assertEqual(g.duration("lift"), 1.0)
        world = g.posed_world(g.channels("lift"), 0.5)
        moved = g.skinned_positions(world, "body")[0]
        np.testing.assert_allclose(moved[:, 1], [2.5, 2.5, 3.5])

    def test_lowest_skinned_y_matches_skinning_every_vertex(self):
        rng = np.random.default_rng(7)
        for trial in range(40):
            b = G.GlbBuilder()
            count = int(rng.integers(1, 5))
            bones = [b.add_node("root", translation=rng.normal(0, 0.3, 3))]
            for k in range(1, count):
                bones.append(b.add_node(f"b{k}", translation=rng.normal(0, 0.3, 3), parent=bones[-1]))
            n = int(rng.integers(3, 300))
            joints = rng.integers(0, count, (n, 4))
            weights = rng.random((n, 4)) * (rng.random((n, 4)) < 0.6)
            weights[rng.random(n) < 0.1] = 0.0  # some vertices carry no weight at all
            if trial % 10 == 0:
                weights[:] = 0.0
            if trial % 2:
                sums = weights.sum(axis=1, keepdims=True)
                weights = np.where(sums > 0, weights / np.where(sums > 0, sums, 1), 0)
            mesh = b.add_mesh("skin", rng.normal(0, 0.5, (n, 3)), np.arange(n - n % 3).reshape(-1, 3),
                              extra={"JOINTS_0": (joints, G.UNSIGNED_BYTE), "WEIGHTS_0": (weights, G.FLOAT)})
            ibm = [np.linalg.inv(G.trs_matrix(rng.normal(0, 0.3, 3), rng.normal(size=4), (1, 1, 1))).T.reshape(-1)
                   for _ in bones]
            b.json["skins"] = [{"joints": bones, "inverseBindMatrices": b.add_accessor(np.array(ibm), kind="MAT4")}]
            b.add_node("body", mesh=mesh, skin=0)
            times = b.add_accessor(np.array([0.0, 1.0]), kind="SCALAR", bounds=True)
            b.json["animations"] = [{"name": "a", "samplers": [], "channels": []}]
            for k, bone in enumerate(bones):
                for path, width in (("translation", 3), ("rotation", 4)):
                    # Every third trial lifts the whole body above y = 0, past any unweighted vertex.
                    lift = [[0, 3.0 * (k == 0 and trial % 3 == 0 and path == "translation"), 0, 0][:width]]
                    keys = b.add_accessor(rng.normal(0, 1.0, (2, width)) + lift)
                    b.json["animations"][0]["samplers"].append({"input": times, "output": keys})
                    b.json["animations"][0]["channels"].append(
                        {"sampler": len(b.json["animations"][0]["samplers"]) - 1, "target": {"node": bone, "path": path}})
            g = b.to_glb()
            channels = g.channels("a")
            ts = np.linspace(0, 1, 7)
            body = g.by_name["body"]
            fast = g.lowest_skinned_y(g.posed_worlds(channels, ts), [body])
            brute = [g.skinned_positions(g.posed_world(channels, t), body)[0][:, 1].min() for t in ts]
            np.testing.assert_allclose(fast, brute, rtol=0, atol=1e-9, err_msg=f"trial {trial}")


if __name__ == "__main__":
    unittest.main()
