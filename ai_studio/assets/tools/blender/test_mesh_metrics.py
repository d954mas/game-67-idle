import unittest

from ai_studio.assets.tools.blender.mesh_metrics import measure_mesh


class MeshMetricsTests(unittest.TestCase):
    def test_closed_tetrahedron_is_manifold(self):
        vertices = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1)]
        polygons = [(0, 2, 1), (0, 1, 3), (1, 2, 3), (2, 0, 3)]
        result = measure_mesh(vertices, polygons)
        self.assertEqual(result["boundary_edges"], 0)
        self.assertEqual(result["non_manifold_edges"], 0)
        self.assertEqual(result["degenerate_faces"], 0)
        self.assertEqual(result["duplicate_faces"], 0)

    def test_open_quad_reports_boundary_edges(self):
        result = measure_mesh(
            [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)],
            [(0, 1, 2, 3)],
        )
        self.assertEqual(result["boundary_edges"], 4)

    def test_zero_area_face_is_degenerate(self):
        result = measure_mesh([(0, 0, 0), (1, 0, 0), (2, 0, 0)], [(0, 1, 2)])
        self.assertEqual(result["degenerate_faces"], 1)

    def test_repeated_face_is_reported(self):
        vertices = [(0, 0, 0), (1, 0, 0), (0, 1, 0)]
        result = measure_mesh(vertices, [(0, 1, 2), (2, 1, 0)])
        self.assertEqual(result["duplicate_faces"], 1)

    def test_three_faces_on_one_edge_are_non_manifold(self):
        vertices = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1), (0, -1, 0)]
        polygons = [(0, 1, 2), (1, 0, 3), (0, 1, 4)]
        result = measure_mesh(vertices, polygons)
        self.assertGreaterEqual(result["non_manifold_edges"], 1)


if __name__ == "__main__":
    unittest.main()
