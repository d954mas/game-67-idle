import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from garment_cook import BodySurface, body_maps, cook_mesh, top_influences, _tethers


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

    def test_max_distance_is_zero_exactly_on_the_pins(self):
        mesh = cook_mesh("fold", POINTS, FACES, [0, 1], wearable=True)
        self.assertTrue(np.all(mesh["max_distance"][[0, 1]] == 0) and np.all(mesh["max_distance"][[2, 3]] > 0))
        self.assertTrue(np.all(mesh["inv_area"] > 0))
        with self.assertRaisesRegex(ValueError, "zero on the pins"):
            cook_mesh("fold", POINTS, FACES, [0, 1], max_distance=[0, 0.1, 0.1, 0.1])

    def test_backstop_radius_follows_the_body_not_its_tessellation(self):
        def cylinder(around, rows, radius=0.1, sign=1):
            angle = np.linspace(0, 2 * np.pi, around, endpoint=False)
            y = np.linspace(-0.3, 0.3, rows)
            v = np.array([(radius * np.cos(a), h, radius * np.sin(a)) for h in y for a in angle])
            f = []
            for j in range(rows - 1):
                for i in range(around):
                    a, b, c, d = j * around + i, j * around + (i + 1) % around, (j + 1) * around + i, (j + 1) * around + (i + 1) % around
                    f += [(a, c, b), (b, c, d)] if sign > 0 else [(a, b, c), (b, d, c)]
            n = sign * v * [1, 0, 1] / radius
            return BodySurface(v, np.array(f), n)

        # A small sim patch 2 cm off the surface, facing the cylinder.
        grid = np.array([(0.12, y, z) for y in (-0.02, 0.0, 0.02) for z in (-0.02, 0.0, 0.02)])
        faces = np.array([(0, 3, 1), (1, 3, 4), (1, 4, 2), (2, 4, 5), (3, 6, 4), (4, 6, 7), (4, 7, 5), (5, 7, 8)])
        r = np.full(9, 0.05)
        radii = [body_maps(grid, faces, r, cylinder(*shape), 0.002)[1][:, 1] for shape in ((24, 13), (64, 41))]
        self.assertTrue(np.all(np.abs(radii[1] - 0.1) < 0.01) and np.all(np.isfinite(radii[0])))
        np.testing.assert_allclose(radii[0], radii[1], rtol=0.1)
        b, backstop = body_maps(grid, faces, r, cylinder(64, 41), 0.002)
        self.assertTrue(np.all(b[:, 0] < -0.9))  # toward the body
        np.testing.assert_allclose(backstop[:, 0], 0.018, atol=0.003)
        # Inside a tube the body is concave: the backstop takes its plane form.
        inner = np.array([(0.18, y, z) for y in (-0.02, 0.0, 0.02) for z in (-0.02, 0.0, 0.02)])
        self.assertTrue(np.all(np.isinf(body_maps(inner, faces, r, cylinder(64, 41, 0.2, -1), 0.002)[1][:, 1])))
        # A max distance of twice the limb radius reaches past the limb's side; the backstop stays the limb.
        for shape in ((24, 13), (64, 41)):
            np.testing.assert_allclose(body_maps(grid, faces, np.full(9, 0.2), cylinder(*shape), 0.002)[1][:, 1], 0.1,
                                       rtol=0.05)
        # Where the cloth is farther from the body than r, the backstop can never act: off.
        self.assertTrue(np.all(body_maps(grid, faces, np.full(9, 0.01), cylinder(64, 41), 0.002)[1][:, 1] == 0))

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
    out.write(f"static const float {name}_r[] = {vector(mesh['max_distance'])};\n")
    array(f"ClothBackstop {name}_backstop[]", (f"{{{d:.9e}F,{r:.9e}F}}" for d, r in mesh["backstop"]))
    tethers = "0"
    if mesh["tethers"] is not None:
        tethers = name + "_tethers"
        array(f"ClothTether {tethers}[]", (f"{{{pin},{length:.9e}F}}" for pin, length in mesh["tethers"]))
    out.write(f"static const ClothMesh {name} = {{{len(mesh['rest'])}, {len(mesh['faces'])}, {len(mesh['edges'])}, "
              f"{name}_rest, {name}_inv, {name}_faces, {name}_edges, {tethers}, {name}_r, {name}_backstop}};\n")


if __name__ == "__main__":
    if sys.argv[1:2] == ["--emit"]:
        with Path(sys.argv[2]).open("w", encoding="ascii", newline="\n") as out:
            emit(out, "fold", cook_mesh("fold", POINTS, FACES, [0, 1, 2], wearable=True))
            emit(out, "seam", cook_mesh("seam", POINTS[:3], FACES[:1], [0, 1], wearable=True, tether_slack=1.0))
            emit(out, "small_seam", cook_mesh("small_seam", np.asarray(POINTS[:3]) * 0.055, FACES[:1], [0, 1], wearable=True))
            # A 0.4 m square hanging in the XY plane, its top row pinned.
            n = 9
            grid = [(-0.2 + 0.4 * i / (n - 1), 0.4 * j / (n - 1), 0.0) for j in range(n) for i in range(n)]
            cells = [(j * n + i, j * n + i + 1, (j + 1) * n + i, (j + 1) * n + i + 1) for j in range(n - 1) for i in range(n - 1)]
            tris = [t for a, b, c, d in cells for t in ((a, b, d), (a, d, c))]
            emit(out, "panel", cook_mesh("panel", grid, tris, list(range(n * (n - 1), n * n)), wearable=True,
                                         tether_slack=1.05))
    else:
        unittest.main()
