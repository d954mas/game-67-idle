import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from garment_keyframes import authored_track, normal_offsets, normal_weight_map, vertex_map


PANEL = np.array([[0, 1, 0], [1, 1, 0], [0, 0, 0], [1, 0, 0]], dtype=float)
FACES = np.array([[0, 2, 1], [1, 2, 3]])


class TestKeyframes(unittest.TestCase):
    def test_spatial_masks_can_be_cooked_without_a_clip(self):
        points = [[0.5, 0, 0], [0, 0, 0], [2, 0, 0]]
        patch = {"center_m": [0, 0, 0], "radii_m": [1, 1, 1]}
        np.testing.assert_allclose(normal_weight_map(patch, points), [0.5, 1, 0])
        for invalid in (dict(patch, weights=1), {}, {"weights": [-1, 0, 1]}):
            with self.assertRaises(ValueError):
                normal_weight_map(invalid, points)

    def test_scalar_and_painted_radii(self):
        np.testing.assert_array_equal(vertex_map(0.04, 4), [0.04]*4)
        np.testing.assert_array_equal(vertex_map([0, 0, 0.02, 0.1], 4), [0, 0, 0.02, 0.1])
        for invalid in ([0, 0.1], [0, 0, 0, -1], np.nan):
            with self.assertRaises(ValueError):
                vertex_map(invalid, 4)

    def test_signed_region_and_painted_patch_add_at_each_tick(self):
        patches = [
            {"weights": [0, 0, 1, 1], "offset_m": 0.02, "keys": [[0, 0], [4, 1], [8, 0]]},
            {"center_m": PANEL[2], "radii_m": [0.5, 0.5, 0.5],
             "offset_m": -0.01, "keys": [[2, 0], [4, 1], [6, 0]]},
        ]
        frames = normal_offsets(patches, PANEL, 9)
        np.testing.assert_array_equal(frames[[0, 8]], np.zeros((2, 4)))
        np.testing.assert_array_equal(frames[:, :2], np.zeros((9, 2)))
        np.testing.assert_allclose(frames[4], [0, 0, 0.01, 0.02])
        self.assertAlmostEqual(frames[1, 3], 0.02*(3*0.25**2 - 2*0.25**3))
        with self.assertRaises(ValueError):
            normal_offsets([dict(patches[0], center_m=PANEL[2])], PANEL, 9)

    def test_full_pose_keys_accept_caller_loader_and_require_identical_topology(self):
        target = PANEL.copy()
        target[2:, 2] = 0.02
        keys = [{"tick": 0, "vertices_m": PANEL}, {"tick": 2, "obj": "raised.obj"}]
        directory = Path("poses")

        def load(path):
            self.assertEqual(path, directory / "raised.obj")
            return target, FACES

        frames = authored_track(keys, 3, PANEL, FACES, load, directory)
        np.testing.assert_array_equal(frames[0], PANEL)
        np.testing.assert_allclose(frames[1], (PANEL + target)/2)
        np.testing.assert_array_equal(frames[-1], target)
        with self.assertRaises(ValueError):
            authored_track(keys, 3, PANEL, FACES, lambda _: (target, FACES[::-1]), directory)

    def test_scalar_amplitude_and_key_weights_have_explicit_numeric_types(self):
        patch = {"weights": 1, "offset_m": np.float32(-0.02), "keys": [[0, np.int64(1)], [2, 0.5]]}
        frames = normal_offsets([patch], PANEL, 3)
        np.testing.assert_allclose(frames[0], -0.02)
        np.testing.assert_allclose(frames[2], -0.01)
        for bad in (True, np.bool_(True), "0.5", [1], np.array(1), np.array([1, 2]), np.nan, np.inf, 10**1000):
            with self.subTest(value=repr(bad)):
                with self.assertRaises(ValueError):
                    normal_offsets([dict(patch, offset_m=bad)], PANEL, 3)
                with self.assertRaises(ValueError):
                    normal_offsets([dict(patch, keys=[[0, bad], [2, 1]])], PANEL, 3)


if __name__ == "__main__":
    unittest.main()
