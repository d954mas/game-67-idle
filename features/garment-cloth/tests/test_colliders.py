"""The collider fit on synthetic limbs whose answer is known: a round limb is covered by a cone of its own radius, an
elliptic one by the circle of its long axis with the difference standing proud, a fitted reference vertex makes the
covering cone give way until gate G7 passes, and the asset reads back as written."""
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
    def test_a_fitted_vertex_inside_the_covering_cone_makes_it_give_way_until_g7_passes(self):
        body = limb(0.06, 0.06)
        # 2 mm inside the cone near the hip, free by 0.5 mm only; the other two corners stand clear of the leg.
        cloth = [piece([(0.058, Y1 - 0.03, 0.0), (0.2, Y1, 0.0), (0.2, Y1 - 0.1, 0.0)], [0.0005, 0.0, 0.0])]
        whole = C.fit(body, RIG, SEGMENTS, [], CLIPS)
        self.assertEqual(held(whole, cloth), 1)
        shapes = C.fit(body, RIG, SEGMENTS, cloth, CLIPS)
        self.assertEqual(held(shapes, cloth), 0)
        log = shapes.log[0]
        self.assertIn(log["decision"], ("cut", "split"))
        self.assertGreater(log["kept_share"], 0.8)

    def test_an_elliptic_limb_under_cloth_on_its_flat_side_splits_rather_than_loses_its_length(self):
        body = limb(0.08, 0.05)
        middle = 0.5 * (Y0 + Y1)
        cloth = [piece([(0.0, middle, 0.055), (0.2, Y1, 0.2), (0.2, Y0, 0.2)], [0.001, 0.0, 0.0])]
        shapes = C.fit(body, RIG, SEGMENTS, cloth, CLIPS)
        self.assertEqual(held(shapes, cloth), 0)
        self.assertEqual(shapes.log[0]["decision"], "split")
        self.assertEqual(len(shapes.cones), 2)
        self.assertGreater(shapes.log[0]["kept_share"], 0.99)
        centres = shapes.posed(RIG, CLIPS[0][2])
        self.assertLessEqual(shapes.distance(body.points[None], centres)[0].max(), 1e-9)


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


if __name__ == "__main__":
    unittest.main()
