import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from garment_binding import bind_render, bind_surface, surface_rows
from garment_proxy import corner_owners, sewn_sheet


class SurfaceBinding(unittest.TestCase):
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


class RenderSides(unittest.TestCase):
    def test_coincident_lips_and_lining_bind_their_own_side(self):
        # The skirt connects above the slit. Its lips meet at the hem and have distinct UV corners.
        positions = np.array([[0,0,0], [-.02,0,0], [0,.01,0],
                              [0,0,0], [.0004,.01,0], [.02,0,0], [.0004,.01,.003],
                              [.0004,.01,0], [.0004,.01,.003], [.02,0,.003]])
        faces = np.array([[0,1,2],[3,4,5],[3,4,6],[7,8,9]])
        proxy = np.vstack((positions[:6], [[-.02,.04,0],[-.01,.05,0],[.02,.04,0],[.03,.05,0]]))
        proxy[[3,4],0] = .005
        patch = np.vstack((faces[:2], [[1,2,6],[2,7,6],[6,7,8],[7,9,8],[8,9,4],[8,4,5]]))
        corners = corner_owners(positions, faces[:2], faces[:2])
        active = np.ones(len(positions), bool)
        left_corners = {0,1,2}
        inverse, rows = surface_rows(proxy, patch, positions, faces, active, None, .05)
        self.assertTrue(left_corners.intersection(rows[inverse[8]][:3]))
        inverse, rows = surface_rows(proxy, patch, positions, faces, active, corners, .05)
        self.assertEqual(set(rows[inverse[0]][:3]), left_corners)
        for vertex in (3,6,8):
            self.assertFalse(left_corners.intersection(rows[inverse[vertex]][:3]))

    def test_closed_uv_seam_shares_a_fan(self):
        positions = np.array([[0,0,0],[.1,0,0],[0,.1,0],[.1,0,0],[.1,.1,0],[0,.1,0]])
        faces = np.array([[0,1,2],[3,4,5]])
        proxy, sewn, _, source = sewn_sheet(positions, faces)
        corners = corner_owners(positions, source, sewn)
        inverse, rows = surface_rows(proxy, sewn, positions, faces, np.ones(len(positions), bool), corners, .1)
        for a,b in ((1,3),(2,5)):
            self.assertEqual(rows[inverse[a]], rows[inverse[b]])


class OpeningGuard(unittest.TestCase):
    """A sheet split from its bottom edge up to the middle, and a ribbon that lies on it below the split's top."""

    def setUp(self):
        # 9 x 9 grid over 0.4 m; the split runs up the centre column from y = 0 to y = 0.2 and doubles its vertices.
        n, size = 9, 0.4
        xs = np.linspace(-size / 2, size / 2, n)
        points = [(x, y, 0.0) for y in np.linspace(0, size, n) for x in xs]
        index = lambda i, j: j * n + i
        split = {}
        for j in range(n // 2 + 1):
            if j < n // 2:
                split[index(n // 2, j)] = len(points)
                points.append(points[index(n // 2, j)])
        faces = []
        for j in range(n - 1):
            for i in range(n - 1):
                quad = [index(i, j), index(i + 1, j), index(i + 1, j + 1), index(i, j + 1)]
                if i >= n // 2:
                    quad = [split.get(v, v) for v in quad]
                faces += [(quad[0], quad[1], quad[2]), (quad[0], quad[2], quad[3])]
        self.proxy, self.triangles = np.array(points), np.array(faces)

    def ribbon(self, x0, x1, y):
        positions = np.array([(x0, y, 0.002), (x1, y, 0.002), (x0, y + 0.02, 0.002), (x1, y + 0.02, 0.002)])
        faces = np.array([(0, 1, 3), (0, 3, 2)])
        return positions, faces

    def bind(self, positions, faces):
        piece = (1, self.proxy, self.triangles, None, np.zeros(len(self.proxy), bool))
        return bind_render(positions, faces, None, np.ones(len(positions)), np.ones(len(positions)), [piece], [0])

    def test_a_ribbon_across_the_opening_fails_the_cook(self):
        with self.assertRaisesRegex(ValueError, "across an opening"):
            self.bind(*self.ribbon(-0.03, 0.03, 0.05))

    def test_a_ribbon_beside_the_opening_or_above_it_binds(self):
        for x0, x1, y in ((-0.12, -0.04, 0.05), (-0.03, 0.03, 0.3)):
            stream, amount = self.bind(*self.ribbon(x0, x1, y))
            self.assertTrue(np.all(amount > 0))


if __name__ == "__main__":
    unittest.main()
