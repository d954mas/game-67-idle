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
        self.assertTrue({"duplicate", "degenerate"} <= serious(report))

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
            self.assertIn("non_manifold", checks(report, "medium"), smooth)
            self.assertNotIn("touching", checks(report), smooth)

    def test_closed_boxes_meeting_on_an_edge_are_touching(self):
        other = CUBE + [1, 1, 0]
        positions, faces = flat(np.vstack([CUBE, other]), np.vstack([FACES, FACES + 8]))
        report = mesh_check.check(scene(faces, positions))
        self.assertIn("touching", checks(report, "low"))
        self.assertNotIn("non_manifold", checks(report))

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

class Winding(unittest.TestCase):
    def test_mirrored_node_flips_winding_back(self):
        b = G.GlbBuilder()
        b.add_node("box", mesh=b.add_mesh("box", CUBE, FACES), scale=(-1, 1, 1))
        self.assertNotIn("inside_out", checks(mesh_check.check(b.to_glb())))
        b = G.GlbBuilder()
        b.add_node("box", mesh=b.add_mesh("box", CUBE, FACES[:, ::-1]), scale=(-1, 1, 1))
        self.assertIn("inside_out", checks(mesh_check.check(b.to_glb()), "high"))

    def test_reversed_twin_is_double_sided_not_z_fighting(self):
        plane = np.array([[0, 0, 0], [1, 0, 0], [1, 0, 1], [0, 0, 1]], float)
        faces = np.array([[0, 2, 1], [0, 3, 2], [0, 1, 2], [0, 2, 3]])
        report = mesh_check.check(scene(faces, plane))
        self.assertIn("double_sided", checks(report, "info"))
        self.assertNotIn("duplicate", checks(report))


if __name__ == "__main__":
    unittest.main()
