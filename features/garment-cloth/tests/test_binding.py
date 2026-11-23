import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from garment_binding import bind_surface, pack_indexed_surface, unpack_vertex_binding_u16x4


class SurfaceBinding(unittest.TestCase):
    def test_indexed_surface_binds_only_movable_vertices_of_this_piece(self):
        rows = pack_indexed_surface(6, [0, 2, 5], palette_offset=11)
        self.assertEqual(np.flatnonzero(rows.any(axis=1)).tolist(), [1, 3, 4])
        for index in (1, 3, 4):
            _, ids, weights = unpack_vertex_binding_u16x4(rows[index])
            self.assertEqual(ids, (12 + index, 0, 0))
            self.assertEqual(weights, (1., 0., 0.))

    def test_interior_seam_and_signed_thickness_reconstruct(self):
        points = np.array([(0,0,0), (1,0,0), (0,1,0), (1,1,0)])
        faces = [(0,1,2), (2,1,3)]
        target = [(0.2,0.3,0.01), (1.002,0.9,-0.012), (0.5,0.5,0)]
        for x, (i,u,v,h) in zip(target, bind_surface(points, faces, target, 0.02)):
            a,b,c = points[list(faces[i])]
            n = np.cross(b-a,c-a)
            np.testing.assert_allclose(x, a+(b-a)*u+(c-a)*v+h*n/np.linalg.norm(n), atol=1e-7)

    def test_missing_fit_fails_during_cooking(self):
        with self.assertRaisesRegex(ValueError, "exceeds binding distance"):
            bind_surface([(0,0,0),(1,0,0),(0,1,0)], [(0,1,2)], [(0.1,0.1,1)], 0.01)

    def test_coincident_seam_lips_keep_their_authored_sides(self):
        points = [(0,0,0),(1,0,0),(0,1,0)] * 2
        rows = bind_surface(points, [(0,1,2),(3,4,5)], [(0.5,0,0)] * 2, 0.001, face_candidates=[[1],[0]])
        self.assertEqual([r[0] for r in rows], [1,0])


if __name__ == "__main__":
    unittest.main()
