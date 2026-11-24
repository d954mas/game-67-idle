"""The collider fit on synthetic limbs whose answer is known: a round limb is covered by a cone of its own radius, an
elliptic one by the circle of its long axis with the difference standing proud, a fitted reference vertex thins the
cone's near end while its far end and length stay, a reference inside the skin itself (a G4 failure) leaves the fit
alone, a segment list without arms gives no arm shapes, and the asset reads back as written."""
import math
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE.parent / "tools"), str(HERE)]
from garment import colliders as C  # noqa: E402
from garment.body import Body  # noqa: E402

Y0, Y1 = 0.10, 0.50
JOINTS = ["hip", "knee"]
THICKNESS = 0.002


def limb(a, b, around=48, rows=9):
    """An open elliptic tube along -Y from the hip (y 0.5) to the knee (y 0.1), semi-axes a (x) and b (z), every
    vertex on the hip joint."""
    angle = np.linspace(0, 2 * np.pi, around, endpoint=False)
    points = np.array([[a * math.cos(t), y, b * math.sin(t)] for y in np.linspace(Y0, Y1, rows) for t in angle])
    faces = []
    for j in range(rows - 1):
        for i in range(around):
            p, q = j * around + i, j * around + (i + 1) % around
            faces += [(p, q + around, q), (p, p + around, q + around)]
    normals = points * [1 / a ** 2, 0, 1 / b ** 2]
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    weights = np.zeros((len(points), 2))
    weights[:, 0] = 1
    return Body(points, np.array(faces), normals, weights, JOINTS, complete=False)


RIG = C.Rig(JOINTS, [(0, Y1, 0), (0, Y0, 0)])
SEGMENTS = C.segments_from({"segments": [{"name": "thigh", "joints": JOINTS}]})
# The rest pose and a clip that carries the whole leg sideways: the shapes and the cloth ride the same joint.
SHIFT = np.eye(4)
SHIFT[0, 3] = 0.1
CLIPS = [(C.REST, np.zeros(1), np.stack([np.eye(4)] * 2)[None]), ("step", np.array([0.0]), np.stack([SHIFT] * 2)[None])]


def piece(points, r):
    """A triangle of cloth on the hip joint with max distances r."""
    points = np.asarray(points, float)
    w = np.zeros((len(points), 2))
    w[:, 0] = 1
    return C.Piece("panel", points, np.array([[0, 1, 2]]), np.asarray(r, float), w, THICKNESS)


def held(shapes, pieces):
    return C.g7(shapes, RIG, pieces, CLIPS)["panel"]["held_vertices"]


class CoverTest(unittest.TestCase):
    def test_a_round_limb_is_covered_by_a_cone_of_its_radius(self):
        body = limb(0.06, 0.06)
        shapes = C.fit(body, RIG, SEGMENTS, [], CLIPS)
        self.assertEqual(len(shapes.cones), 1)
        a, b = shapes.spheres
        self.assertAlmostEqual(a.radius, 0.06, delta=1e-4)
        self.assertAlmostEqual(b.radius, 0.06, delta=1e-4)
        self.assertLess(np.linalg.norm(a.center[[0, 2]]), 1e-4)
        self.assertAlmostEqual(abs(a.center[1] - b.center[1]), Y1 - Y0, delta=1e-9)
        centres = shapes.posed(RIG, CLIPS[0][2])
        self.assertLessEqual(shapes.distance(body.points[None], centres)[0].max(), 1e-9)
        # The tube's faces are chords of its circle, 0.5 mm inside it at the middle.
        self.assertLess(C.proud(shapes, body)["all"][2], 1.0)

    def test_an_elliptic_limb_is_covered_by_its_long_axis_and_stands_proud_by_the_difference(self):
        body = limb(0.08, 0.05)
        shapes = C.fit(body, RIG, SEGMENTS, [], CLIPS)
        for s in shapes.spheres:
            self.assertAlmostEqual(s.radius, 0.08, delta=2e-4)
        p50, p90, top = C.proud(shapes, body)["all"]
        self.assertGreater(top, 29.0)
        self.assertLessEqual(top, 30.5)


class ReachTest(unittest.TestCase):
    def test_a_fitted_vertex_near_the_hip_thins_the_hip_end_and_the_cone_keeps_its_length_and_knee(self):
        body = limb(0.06, 0.06)
        # 2 mm inside the cover near the hip, free by 0.5 mm only; the other two corners stand clear of the leg.
        cloth = [piece([(0.058, Y1 - 0.03, 0.0), (0.2, Y1, 0.0), (0.2, Y1 - 0.1, 0.0)], [0.0005, 0.0, 0.0])]
        whole = C.fit(body, RIG, SEGMENTS, [], CLIPS)
        self.assertEqual(held(whole, cloth), 1)
        shapes = C.fit(body, RIG, SEGMENTS, cloth, CLIPS)
        self.assertEqual(held(shapes, cloth), 0)
        self.assertEqual(shapes.log[1]["decision"], "given way, length kept 1.00")
        hip, knee = shapes.spheres
        self.assertLess(hip.radius, 0.06 - 0.001)
        self.assertAlmostEqual(knee.radius, 0.06, delta=1e-4)
        self.assertAlmostEqual(abs(hip.center[1] - knee.center[1]), Y1 - Y0, delta=1e-9)

    def test_a_reference_inside_the_skin_deeper_than_its_reach_does_not_change_the_fit(self):
        body = limb(0.06, 0.06)
        # 40 mm inside the leg with 5 mm of reach: a G4 failure, content to fix, not a reason to shrink the leg.
        cloth = [piece([(0.02, 0.3, 0.0), (0.2, Y1, 0.0), (0.2, Y0, 0.0)], [0.005, 0.0, 0.0])]
        alone = C.fit(body, RIG, SEGMENTS, [], CLIPS)
        shapes = C.fit(body, RIG, SEGMENTS, cloth, CLIPS)
        self.assertEqual(shapes.log[0]["g4_failing"], {C.REST: 1, "step": 1})
        for s, t in zip(alone.spheres, shapes.spheres):
            np.testing.assert_allclose(s.center, t.center)
            self.assertEqual(s.radius, t.radius)
        self.assertEqual(C.g7(shapes, RIG, cloth, CLIPS, C.g4_failing(body, cloth, CLIPS))["panel"]["held_vertices"], 0)

    def test_a_segment_list_without_arms_gives_no_arm_shapes(self):
        leg, arm = limb(0.06, 0.06), limb(0.04, 0.04)
        points = np.concatenate([leg.points, arm.points + [0.4, 0.6, 0.0]])
        weights = np.zeros((len(points), 4))
        weights[:len(leg.points), 0] = 1
        weights[len(leg.points):, 2] = 1
        joints = JOINTS + ["arm", "hand"]
        body = Body(points, np.concatenate([leg.faces, arm.faces + len(leg.points)]),
                    np.concatenate([leg.normals, arm.normals]), weights, joints, complete=False)
        rig = C.Rig(joints, [(0, Y1, 0), (0, Y0, 0), (0.4, Y1 + 0.6, 0), (0.4, Y0 + 0.6, 0)])
        clips = [(C.REST, np.zeros(1), np.stack([np.eye(4)] * 4)[None])]
        shapes = C.fit(body, rig, SEGMENTS, [], clips)
        self.assertEqual({s.bone for s in shapes.spheres}, {"hip"})
        both = C.segments_from({"segments": [{"name": "thigh", "joints": JOINTS}, {"name": "arm", "joints": ["arm", "hand"]}]})
        self.assertEqual({s.bone for s in C.fit(body, rig, both, [], clips).spheres}, {"hip", "arm"})


class FileTest(unittest.TestCase):
    def test_the_asset_reads_back_as_written(self):
        shapes = C.fit(limb(0.08, 0.05), RIG, SEGMENTS, [], CLIPS)
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "body.cloth-colliders.json"
            doc = C.write(path, shapes, "ab" * 32)
            back = C.read(path)
        self.assertEqual(doc["schema"], C.SCHEMA)
        self.assertEqual(back.cones, shapes.cones)
        self.assertEqual(back.log[0]["body_sha256"], "ab" * 32)
        for s, t in zip(shapes.spheres, back.spheres):
            self.assertEqual((s.name, s.bone), (t.name, t.bone))
            np.testing.assert_allclose(t.center, s.center, atol=1e-6)
            self.assertAlmostEqual(t.radius, s.radius, delta=1e-6)

    def test_a_degenerate_segment_and_a_non_finite_shape_are_refused(self):
        # Two joints at one rest position give a cone without an axis.
        rig = C.Rig(JOINTS, [(0, Y1, 0), (0, Y1, 0)])
        with self.assertRaisesRegex(ValueError, "share a rest position"):
            C.fit(limb(0.08, 0.05), rig, SEGMENTS, [], CLIPS)
        shapes = C.fit(limb(0.08, 0.05), RIG, SEGMENTS, [], CLIPS)
        shapes.spheres[0].radius = float("nan")
        with tempfile.TemporaryDirectory() as folder, self.assertRaisesRegex(ValueError, "not finite"):
            C.write(Path(folder) / "x.cloth-colliders.json", shapes, "ab" * 32)


if __name__ == "__main__":
    unittest.main()
