import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from garment_paint import FREE_FLOOR, baseline_freedom, coincident, covered, surface_distance, weld
from synthetic import grid


class Paint(unittest.TestCase):
    def setUp(self):
        self.points, self.faces = grid(0, 0.3, 0, 0.6, 0, 7, 13)

    def test_seam_band_is_pinned_and_freedom_grows_down_the_panel(self):
        f = baseline_freedom(self.points, self.faces, 0.2, (0.8, 0.2, 1.0))
        y = self.points[:, 1]
        np.testing.assert_array_equal(f[y >= 0.6 - 0.2 * 0.6 - 1e-9], 0)
        free = f > 0
        self.assertTrue(np.all(f[free] >= FREE_FLOOR) and np.all(f <= 1))
        column = np.flatnonzero(np.isclose(self.points[:, 0], 0.15))
        by_height = f[column[np.argsort(-y[column])]]
        self.assertTrue(np.all(np.diff(by_height) >= -1e-12))

    def test_further_pins_are_pins_and_an_undriven_piece_is_free(self):
        extra = [0, 1]
        f = baseline_freedom(self.points, self.faces, 0.1, None, extra)
        self.assertTrue(np.all(f[extra] == 0))
        self.assertTrue(np.all((f == 0) | (f == 1)))

    def test_a_full_drive_weight_stays_free(self):
        f = baseline_freedom(self.points, self.faces, 0.1, (1.0, 1.0, 1.0))
        self.assertTrue(np.all(f[f > 0] == FREE_FLOOR))

    def test_coincident_finds_split_copies_only(self):
        others = np.array([[0.0, 0.6, 0.0], [5.0, 5.0, 5.0]])
        hit = coincident(self.points, others)
        np.testing.assert_array_equal(np.flatnonzero(hit), np.flatnonzero(np.all(np.isclose(self.points, others[0]), 1)))
        self.assertFalse(coincident(self.points + [0, 0, 1e-4], others).any())
        self.assertEqual(len(coincident(self.points, np.zeros((0, 3)))), len(self.points))

    def test_weld_maps_equal_positions_to_one_vertex(self):
        rep = weld([[0, 0, 0], [1, 0, 0], [0, 0, 0]])
        self.assertEqual(rep[0], rep[2])
        self.assertNotEqual(rep[0], rep[1])

    def test_surface_distance_runs_over_welded_copies_and_across_a_gap(self):
        points = np.concatenate([self.points, self.points[:1], [[0.0, -0.05, 0.0]]])
        faces = np.concatenate([self.faces, [[len(self.points), 1, 7]]])
        top = np.flatnonzero(np.isclose(self.points[:, 1], 0.6))
        d = surface_distance(points, faces, top)
        np.testing.assert_allclose(d[top], 0)
        self.assertEqual(d[0], d[len(self.points)])
        column = np.flatnonzero(np.isclose(self.points[:, 0], 0.15))
        self.assertTrue(np.all(np.diff(d[column[np.argsort(-self.points[column, 1])]]) > 0))
        self.assertTrue(np.isnan(d[-1]))


class Layers(unittest.TestCase):
    def setUp(self):
        self.points, self.faces = grid(0, 0.3, 0, 0.6, 0, 7, 13)
        self.normals = np.tile([0.0, 0.0, 1.0], (len(self.points), 1))
        # An outer layer 1 cm in front of the upper half.
        self.cover, self.cover_faces = grid(-0.05, 0.35, 0.3, 0.65, 0.01, 5, 5)

    def test_the_covered_band_is_pinned_and_the_rest_stays_free(self):
        under = covered(self.points, self.normals, self.cover, self.cover_faces, 0.05)
        y = self.points[:, 1]
        self.assertTrue(np.all(under[y > 0.31]) and not np.any(under[y < 0.29]))
        f = baseline_freedom(self.points, self.faces, 0.1, (0.8, 0.2, 1.0), np.flatnonzero(under))
        self.assertTrue(np.all(f[under] == 0) and np.all(f[y < 0.29] > 0))
        self.assertFalse(np.any(covered(self.points, self.normals, self.cover, self.cover_faces, 0.005)))

    def test_a_piece_that_names_no_cover_is_unchanged(self):
        plain = baseline_freedom(self.points, self.faces, 0.1, (0.8, 0.2, 1.0))
        again = baseline_freedom(self.points, self.faces, 0.1, (0.8, 0.2, 1.0), np.array([], int))
        np.testing.assert_array_equal(plain, again)


if __name__ == "__main__":
    unittest.main()
