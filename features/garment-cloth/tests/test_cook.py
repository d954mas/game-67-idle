import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from garment_cook import cook_mesh, top_influences, _tethers


POINTS = [[-0.5, 0, 0], [0.5, 0, 0], [0, 1, 0], [0, 0, 1]]
FACES = [[0, 1, 2], [1, 0, 3]]


class TestCook(unittest.TestCase):
    def test_top_influences_keep_four_and_renormalize(self):
        joints, values = top_influences([[0.1, 0.4, 0.2, 0.05, 0.25], [1, 0, 0, 0, 0]])
        self.assertEqual(joints[0].tolist(), [1, 4, 2, 0])
        np.testing.assert_allclose(values.sum(1), 1)
        self.assertEqual(joints[1].tolist(), [0, 0, 0, 0])

    def test_check_winding_before_writing(self):
        with self.assertRaisesRegex(ValueError, "winding"):
            cook_mesh("panel", POINTS, [[0, 1, 2], [0, 1, 3]], [0])

    def test_author_decides_whether_disconnected_free_parts_are_allowed(self):
        points = [[0, 0, 0], [0, 1, 0], [1, 1, 0], [0, 0, 1], [0, 1, 1], [1, 1, 1]]
        faces = [[0, 1, 2], [3, 4, 5]]
        with self.assertRaisesRegex(ValueError, "pin on each"):
            cook_mesh("arbitrary", points, faces, [0], wearable=True)
        cook_mesh("wearable_by_name_only", points, faces, [])

    def test_numpy_indices_work_and_bad_geometry_fails(self):
        cook_mesh("fold", POINTS, np.array(FACES), np.array([0, 1]), wearable=True)
        for faces in ([[0, 1, 1]], [[0, 1, -1]], [[0, 1, 9]], [[0, 1, 2], [2, 0, 1]]):
            with self.assertRaises(ValueError):
                cook_mesh("invalid", POINTS, faces, [])

    def test_tether_path_follows_edges_and_leaves_free_islands(self):
        points = np.asarray([(0,0,0), (0,1,0), (1,1,0), (1,0,0), (0,0,2)])
        limits = _tethers(points, [(0,1), (1,2), (2,3)], [0], 1.1)
        self.assertEqual(0, limits[3][0])
        self.assertAlmostEqual(3 * 1.1, limits[3][1])
        self.assertEqual((-1, 0), limits[4])
        self.assertIsNone(cook_mesh("drape", POINTS, FACES, [], tether_slack=1.0)["tethers"])


def emit(out, name, mesh):
    """C initializers for the solver tests, which build no blob."""
    def vector(values):
        return "{" + ",".join(f"{float(x):.9e}F" for x in values) + "}"

    def array(decl, rows):
        out.write(f"static const {decl} = {{\n" + ",\n".join(rows) + "\n};\n")

    array(f"float {name}_rest[][3]", map(vector, mesh["rest"]))
    array(f"ClothFace {name}_faces[]", ("{%d,%d,%d}" % tuple(f) for f in mesh["faces"]))
    array(f"ClothEdge {name}_edges[]", (f"{{{a},{b},{c},{d},{length:.9e}F,{angle:.9e}F,{kind}}}"
                                        for a, b, c, d, length, angle, kind in mesh["edges"]))
    out.write(f"static const float {name}_inv[] = {vector(mesh['inv_area'])};\n")
    tethers = "0"
    if mesh["tethers"] is not None:
        tethers = name + "_tethers"
        array(f"ClothTether {tethers}[]", (f"{{{pin},{length:.9e}F}}" for pin, length in mesh["tethers"]))
    # Fixtures start undriven; a test that needs a drive sets its own array.
    out.write(f"static const ClothMesh {name} = {{{len(mesh['rest'])}, {len(mesh['faces'])}, {len(mesh['edges'])}, "
              f"{name}_rest, {name}_inv, {name}_faces, {name}_edges, {tethers}, 0}};\n")


if __name__ == "__main__":
    if sys.argv[1:2] == ["--emit"]:
        with Path(sys.argv[2]).open("w", encoding="ascii", newline="\n") as out:
            emit(out, "fold", cook_mesh("fold", POINTS, FACES, [0, 1, 2], wearable=True))
            emit(out, "seam", cook_mesh("seam", POINTS[:3], FACES[:1], [0, 1], wearable=True, tether_slack=1.0))
            emit(out, "small_seam", cook_mesh("small_seam", np.asarray(POINTS[:3]) * 0.055, FACES[:1], [0, 1], wearable=True))
    else:
        unittest.main()
