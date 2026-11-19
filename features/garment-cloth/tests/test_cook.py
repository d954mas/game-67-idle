import io
import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from garment_cook import cook_mesh, _tethers


POINTS = [[-0.5, 0, 0], [0.5, 0, 0], [0, 1, 0], [0, 0, 1]]
FACES = [[0, 1, 2], [1, 0, 3]]


class TestCook(unittest.TestCase):
    def test_check_winding_before_writing(self):
        out = io.StringIO()
        with self.assertRaisesRegex(ValueError, "winding"):
            cook_mesh(out, "panel", POINTS, [[0, 1, 2], [0, 1, 3]], [0])
        self.assertEqual(out.getvalue(), "")

    def test_author_decides_whether_disconnected_free_parts_are_allowed(self):
        points = [[0, 0, 0], [0, 1, 0], [1, 1, 0], [0, 0, 1], [0, 1, 1], [1, 1, 1]]
        faces = [[0, 1, 2], [3, 4, 5]]
        with self.assertRaisesRegex(ValueError, "pin on each"):
            cook_mesh(io.StringIO(), "arbitrary", points, faces, [0], wearable=True)
        cook_mesh(io.StringIO(), "wearable_by_name_only", points, faces, [])

    def test_numpy_indices_work_and_bad_geometry_fails(self):
        cook_mesh(io.StringIO(), "fold", POINTS, np.array(FACES), np.array([0, 1]), wearable=True)
        for faces in ([[0, 1, 1]], [[0, 1, -1]], [[0, 1, 9]], [[0, 1, 2], [2, 0, 1]]):
            with self.assertRaises(ValueError):
                cook_mesh(io.StringIO(), "invalid", POINTS, faces, [])

    def test_tether_path_follows_edges_and_leaves_free_islands(self):
        points = np.asarray([(0,0,0), (0,1,0), (1,1,0), (1,0,0), (0,0,2)])
        limits = _tethers(points, [(0,1), (1,2), (2,3)], [0], 1.1)
        self.assertEqual(0, limits[3][0])
        self.assertAlmostEqual(3 * 1.1, limits[3][1])
        self.assertEqual((-1, 0), limits[4])
        out = io.StringIO()
        cook_mesh(out, "drape", POINTS, FACES, [], tether_slack=1.0)
        self.assertNotIn("ClothTether", out.getvalue())


if __name__ == "__main__":
    if sys.argv[1:2] == ["--emit"]:
        with Path(sys.argv[2]).open("w", encoding="ascii", newline="\n") as out:
            cook_mesh(out, "fold", POINTS, FACES, [0, 1, 2], wearable=True)
            cook_mesh(out, "seam", POINTS[:3], FACES[:1], [0, 1], wearable=True, tether_slack=1.0)
    else:
        unittest.main()
