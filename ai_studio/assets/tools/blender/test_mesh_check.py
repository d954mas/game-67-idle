import unittest
import subprocess
import sys
from pathlib import Path

import numpy as np

from ai_studio.assets.tools.blender import mesh_check
from ai_studio.assets.tools.blender.lib import glb as G
from ai_studio.assets.tools.blender.mesh_metrics import measure_mesh

CUBE = np.array([[x, y, z] for x in (0, 1) for y in (0, 1) for z in (0, 1)], float)
FACES = np.array([[0, 2, 1], [1, 2, 3], [4, 5, 6], [5, 7, 6], [0, 1, 4], [1, 5, 4],
                  [2, 6, 3], [3, 6, 7], [0, 4, 2], [2, 4, 6], [1, 3, 5], [3, 7, 5]])[:, ::-1]  # outward


def scene(faces=FACES, positions=CUBE, name="box"):
    b = G.GlbBuilder()
    b.add_node(name, mesh=b.add_mesh(name, positions, faces))
    return b.to_glb()


def split_cube():
    """Cube with every face's corners duplicated, as a hard-edge export writes it."""
    positions = CUBE[FACES.reshape(-1)]
    return positions, np.arange(len(positions)).reshape(-1, 3)


def checks(report, severity=None):
    return {f["check"] for f in report["findings"] if severity is None or f["severity"] == severity}


def serious(report):
    return {f["check"] for f in report["findings"] if f["severity"] in ("medium", "high")}


class MeshCheck(unittest.TestCase):
    def test_closed_cube_is_clean_and_outward(self):
        report = mesh_check.check(scene(), profile="volume")
        self.assertEqual(serious(report), set())
        row = report["metrics"]["per_part"][0]
        self.assertAlmostEqual(row["signed_volume"], 1.0)

    def test_hard_edge_splits_are_welded_not_openings(self):
        report = mesh_check.check(scene(*reversed(split_cube())), profile="volume")
        self.assertEqual(serious(report), set())
        unwelded = mesh_check.check(scene(*reversed(split_cube())), profile="volume", weld=0)
        self.assertIn("boundary", checks(unwelded, "high"))

    def test_open_cube(self):
        open_faces = FACES[:-2]
        self.assertIn("boundary", checks(mesh_check.check(scene(open_faces), profile="volume"), "high"))
        auto = mesh_check.check(scene(open_faces))
        self.assertIn("boundary", checks(auto, "info"))
        opening = next(f for f in auto["findings"] if f["check"] == "boundary")["where"]["openings"]
        self.assertEqual(len(opening), 1)
        self.assertEqual(opening[0]["centre"], [0.5, 0.5, 1.0])

    def test_inside_out_cube(self):
        self.assertIn("inside_out", checks(mesh_check.check(scene(FACES[:, ::-1])), "high"))

    def test_double_sided_parts_report_winding_as_info(self):
        flipped = scene(FACES[:, ::-1], name="hoop")
        for option in (True, ["ho*"]):
            report = mesh_check.check(flipped, double_sided=option)
            self.assertEqual(serious(report), set())
            note = next(f for f in report["findings"] if f["check"] == "inside_out")
            self.assertIn("double-sided (the double_sided option)", note["message"])
        self.assertIn("inside_out", checks(mesh_check.check(flipped, double_sided=["pet*"]), "high"))
        b = G.GlbBuilder()
        b.add_node("pet", mesh=b.add_mesh("pet", CUBE, FACES[:, ::-1]))
        b.json["materials"] = [{"name": "glow", "doubleSided": True}]
        b.json["meshes"][0]["primitives"][0]["material"] = 0
        report = mesh_check.check(b.to_glb())
        self.assertEqual(serious(report), set())
        self.assertIn("material's doubleSided", next(f for f in report["findings"] if f["check"] == "inside_out")["message"])

    def test_flipped_face_is_miswound_and_inward(self):
        faces = FACES.copy()
        faces[0] = faces[0][::-1]
        report = mesh_check.check(scene(faces), inward=True)
        self.assertIn("miswound", checks(report, "high"))
        inward = next(f for f in report["findings"] if f["check"] == "inward")
        self.assertEqual(inward["value"], 1)
        clean = mesh_check.check(scene(), inward=True)
        self.assertNotIn("inward", checks(clean))

    def test_duplicate_and_degenerate_faces(self):
        positions = np.vstack([CUBE, [[2, 0, 0], [3, 0, 0], [4, 0, 0]]])
        faces = np.vstack([FACES, FACES[:1], [[8, 9, 10]]])
        report = mesh_check.check(scene(faces, positions), profile="volume")
        # Neither draws a wrong pixel: an exact copy cannot z-fight, a zero-area face draws nothing.
        self.assertTrue({"duplicate", "degenerate"} <= checks(report, "low"))

    def test_copy_with_another_colour_z_fights(self):
        colors = np.ones((9, 4))
        colors[8] = (1, 0, 0, 1)
        positions = np.vstack([CUBE, CUBE[FACES[0, :1]]])
        faces = np.vstack([FACES, [[8, FACES[0, 1], FACES[0, 2]]]])
        b = G.GlbBuilder()
        b.add_node("box", mesh=b.add_mesh("box", positions, faces, colors=colors))
        report = mesh_check.check(b.to_glb())
        self.assertIn("duplicate", checks(report, "medium"))
        self.assertIn("z-fighting", next(f for f in report["findings"] if f["check"] == "duplicate")["message"])

    def test_tri_budget_and_config_override(self):
        self.assertIn("tri_budget", checks(mesh_check.check(scene(), tri_budget=10)))
        self.assertNotIn("tri_budget", checks(mesh_check.check(scene())))
        with self.assertRaises(ValueError):
            mesh_check.check(scene(), tri_bugdet=10)

    def test_measure_mesh_keeps_keys_and_adds_shell_counts(self):
        vertices = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1)]
        result = measure_mesh(vertices, [(0, 2, 1), (0, 1, 3), (1, 2, 3), (2, 0, 3)])
        self.assertEqual(result["boundary_edges"], 0)
        self.assertEqual(result["miswound_edges"], 0)
        self.assertEqual(result["components"], 1)
        self.assertAlmostEqual(result["signed_volume"], 1 / 6)
        quad = measure_mesh([(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)], [(0, 1, 2, 3)])
        self.assertEqual(quad["boundary_edges"], 4)
        self.assertEqual(quad["edges"], 4)


def flat(positions, faces):
    """Every face with its own corners, as a flat-shaded export writes it."""
    return positions[faces.reshape(-1)], np.arange(faces.size).reshape(-1, 3)


class NonManifold(unittest.TestCase):
    def test_fin_on_a_flat_shaded_box_is_genuine(self):
        positions = np.vstack([CUBE, [[0, 1, 0], [1, 1, 0], [0.5, 1.5, -0.5]]])
        faces = np.vstack([FACES, [[8, 9, 10]]])
        for smooth in (True, False):
            report = mesh_check.check(scene(*((faces, positions) if smooth else reversed(flat(positions, faces)))))
            self.assertIn("non_manifold", checks(report, "low"), smooth)
            self.assertNotIn("touching", checks(report), smooth)

    def test_closed_boxes_meeting_on_an_edge_are_touching(self):
        other = CUBE + [1, 1, 0]
        positions, faces = flat(np.vstack([CUBE, other]), np.vstack([FACES, FACES + 8]))
        report = mesh_check.check(scene(faces, positions))
        self.assertIn("touching", checks(report, "info"))
        self.assertNotIn("non_manifold", checks(report))

    def test_non_manifold_ignores_twins_copies_and_slivers(self):
        # A closed box plus a reversed twin of one face, an exact copy of another and a zero-area face
        # on an edge: every extra face sits on existing edges, yet none makes a real fin.
        positions = np.vstack([CUBE, CUBE[[0, 1]]])
        faces = np.vstack([FACES, FACES[:1, ::-1], FACES[1:2], [[0, 1, 9]]])
        report = mesh_check.check(scene(faces, positions))
        self.assertNotIn("non_manifold", checks(report))
        self.assertEqual(report["metrics"]["per_part"][0]["non_manifold_edges"], 0)

    def test_measure_mesh_runs_without_numpy(self):
        code = "\n".join([
            "import builtins, sys",
            "real = builtins.__import__",
            "def block(name, *a, **k):",
            "    if name.split('.')[0] == 'numpy': raise ImportError(name)",
            "    return real(name, *a, **k)",
            "builtins.__import__ = block",
            f"sys.path.insert(0, {str(Path(mesh_check.__file__).parent)!r})",
            "from mesh_metrics import measure_mesh",
            "r = measure_mesh([(0,0,0),(1,0,0),(0,1,0),(0,0,1)], [(0,1,2),(0,1,3),(1,2,3),(2,0,3)])",
            "print(r['miswound_edges'], r['components'], r['boundary_edges'])"])
        out = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, check=True).stdout
        self.assertEqual(out.split(), ["3", "1", "0"])

def box_mesh(size, at, flip=False):
    return (np.asarray(at, float) + CUBE * size, FACES[:, ::-1] if flip else FACES)


def join(meshes):
    points, faces, base = [], [], 0
    for p, f in meshes:
        points.append(p)
        faces.append(f + base)
        base += len(p)
    return np.vstack(faces), np.vstack(points)


class Winding(unittest.TestCase):
    def test_mirrored_node_flips_winding_back(self):
        b = G.GlbBuilder()
        b.add_node("box", mesh=b.add_mesh("box", CUBE, FACES), scale=(-1, 1, 1))
        self.assertNotIn("inside_out", checks(mesh_check.check(b.to_glb())))
        b = G.GlbBuilder()
        b.add_node("box", mesh=b.add_mesh("box", CUBE, FACES[:, ::-1]), scale=(-1, 1, 1))
        self.assertIn("inside_out", checks(mesh_check.check(b.to_glb()), "high"))

    def test_twin_sheet_shell_is_not_inverted(self):
        # A closed quad sheet with its reversed twin: zero volume, nothing drawn wrong.
        plane = np.array([[0, 0, 0], [1, 0, 0], [1, 0, 1], [0, 0, 1]], float)
        faces = np.array([[0, 2, 1], [0, 3, 2], [0, 1, 2], [0, 2, 3]])
        report = mesh_check.check(scene(faces, plane))
        self.assertFalse({"inverted_shell", "inside_out", "miswound"} & checks(report))

    def test_coplanar_opposite_faces_are_not_miswound(self):
        # Two halves of a flat card wound the same way along their shared edge: opposite normals.
        points = np.array([[0, 0, 0], [1, 0, 0], [0.5, 0, 1], [0.5, 0, -1]], float)
        report = mesh_check.check(scene(np.array([[0, 1, 2], [0, 1, 3]]), points))
        self.assertNotIn("miswound", checks(report))
        self.assertIn("coplanar_flip", checks(report, "info"))

    def test_winding_severity_follows_what_culling_changes(self):
        # One small flipped shell among many: a few pixels change, low. A whole hollow shell flipped
        # beside an equal one: a big share of the picture, high.
        small = box_mesh(0.02, (2, 0, 0), flip=True)
        big = [box_mesh(1, (0, 0, 0)), box_mesh(1, (3, 0, 0)), box_mesh(1, (6, 0, 0)), box_mesh(1, (9, 0, 0))]
        tiny = mesh_check.check(scene(*join(big + [small])))
        self.assertIn("inverted_shell", checks(tiny, "low"))
        self.assertLess(tiny["metrics"]["per_part"][0]["culled_silhouette_pct"], 2)
        whole = mesh_check.check(scene(*join([box_mesh(1, (0, 0, 0)), box_mesh(1, (3, 0, 0), flip=True)])))
        self.assertIn("inverted_shell", checks(whole, "high"))
        self.assertIn("silhouette", next(f for f in whole["findings"] if f["check"] == "inverted_shell")["message"])

    def test_zbuffer_keeps_the_nearest_triangle(self):
        from ai_studio.assets.tools.blender.lib import rays
        uv = np.array([[[0, 0], [1, 0], [0, 1]], [[0, 0], [1, 0], [0, 1]], [[2, 2], [3, 2], [2, 3]]], float)
        depth = np.array([[5.0] * 3, [2.0] * 3, [1.0] * 3])
        xs = ys = np.array([0.1, 0.8, 1.5, 2.2])
        z, tri = rays.zbuffer(uv, depth, xs, ys)
        self.assertEqual((tri[0, 0], z[0, 0]), (1, 2.0))  # two triangles cover it: the nearer wins
        self.assertEqual((tri[0, 1], tri[1, 1]), (1, -1))  # (0.8, 0.1) inside, (0.8, 0.8) outside
        self.assertEqual((tri[3, 3], z[3, 3]), (2, 1.0))

    def test_open_sheet_facing_is_the_authors_choice(self):
        # Two open quads wound against each other along a shared edge: a level wall meeting a floor.
        # Open sheets have no outside, so nothing is culled by mistake.
        points = np.array([[0, 0, 0], [1, 0, 0], [1, 0, 1], [0, 0, 1], [1, 1, 0], [1, 1, 1]], float)
        faces = np.array([[0, 2, 1], [0, 3, 2], [1, 2, 5], [1, 5, 4]])
        report = mesh_check.check(scene(faces, points))
        self.assertEqual(serious(report), set())

    def test_untextured_copies_do_not_z_fight(self):
        # A copied face with other UVs but no texture draws the same, unless the game binds one.
        uv = np.zeros((9, 2))
        uv[8] = (0.5, 0.5)
        positions = np.vstack([CUBE, CUBE[FACES[0, :1]]])
        faces = np.vstack([FACES, [[8, FACES[0, 1], FACES[0, 2]]]])
        b = G.GlbBuilder()
        b.add_node("box", mesh=b.add_mesh("box", positions, faces, extra={"TEXCOORD_0": (uv, G.FLOAT)}))
        self.assertIn("duplicate", checks(mesh_check.check(b.to_glb()), "low"))
        self.assertIn("duplicate", checks(mesh_check.check(b.to_glb(), runtime_texture=True), "medium"))

    def test_reversed_twin_is_double_sided_not_z_fighting(self):
        plane = np.array([[0, 0, 0], [1, 0, 0], [1, 0, 1], [0, 0, 1]], float)
        faces = np.array([[0, 2, 1], [0, 3, 2], [0, 1, 2], [0, 2, 3]])
        report = mesh_check.check(scene(faces, plane))
        self.assertIn("double_sided", checks(report, "info"))
        self.assertNotIn("duplicate", checks(report))


if __name__ == "__main__":
    unittest.main()
