import io
import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from garment_binding import bind_vertex_frames, pack_vertex_binding_u16x4, unpack_vertex_binding_u16x4
from garment_cook import cook_mesh
from garment_fit_capsules import fit_tapered_capsule
from garment_proxy import (
    EDGE_DIAG,
    EDGE_WARP,
    EDGE_WEFT,
    classify_edges,
    geodesic_depth,
    subdivide_long_edges,
    triangulate_quads_checkerboard,
    validate_proxy,
)


class TestProxyAndBinding(unittest.TestCase):
    def test_checkerboard_quad_triangulation_and_geodesic_warp_weft(self):
        # 3x3 regular grid lying horizontally in XZ (non-vertical, proving independence from Y).
        pts = np.array([[x * 0.1, 0.0, z * 0.1] for z in range(3) for x in range(3)], dtype=float)
        quads = []
        coords = []
        for z in range(2):
            for x in range(2):
                a, b = z * 3 + x, z * 3 + x + 1
                c, d = a + 3, b + 3
                quads.append((a, b, d, c))
                coords.append((x, z))
        faces = triangulate_quads_checkerboard(quads, grid_coords=coords)
        self.assertEqual(len(faces), 8)
        pins = [0, 1, 2]
        dist_m, norm = geodesic_depth(pts, faces, pins)
        self.assertTrue(np.allclose(dist_m[:3], 0.0))
        self.assertTrue(np.allclose(norm[6:], 1.0))
        kinds = classify_edges(pts, faces, pins)
        # Vertical along z (0-3) is WARP; horizontal along x (3-4) is WEFT; diagonal (0-4) is DIAG.
        self.assertEqual(kinds[(0, 3)], EDGE_WARP)
        self.assertEqual(kinds[(3, 4)], EDGE_WEFT)
        self.assertEqual(kinds[(0, 4)], EDGE_DIAG)
        out = io.StringIO()
        cook_mesh(out, "cape", pts, faces, pins, wearable=True, edge_kinds=kinds, anim_drive=1.0 - norm)
        text = out.getvalue()
        self.assertIn("cape_anim_drive", text)

    def test_validate_proxy_detects_bind_pose_penetration_and_edge_limit(self):
        pts = np.array([[0.0, 1.0, 0.0], [0.1, 1.0, 0.0], [0.05, 0.9, 0.0]])
        faces = np.array([[0, 1, 2]])
        cap = [{"a": [0.05, 0.9, 0.0], "b": [0.05, 0.8, 0.0], "radius": 0.05, "radius_b": 0.03}]
        report = validate_proxy(pts, faces, [0, 1], max_edge_m=0.05, capsules=cap)
        self.assertFalse(report["edge_limit_ok"])
        self.assertEqual(len(report["bind_penetrations"]), 1)
        self.assertEqual(report["bind_penetrations"][0]["vertex"], 2)

    def test_tapered_capsule_fitter_hugs_conical_limb(self):
        rng = np.random.default_rng(7)
        t = rng.uniform(0.0, 1.0, 400)
        ang = rng.uniform(0.0, 2.0 * np.pi, 400)
        true_r = 0.09 - 0.04 * t
        pts = np.column_stack([true_r * np.cos(ang), t * 0.4, true_r * np.sin(ang)])
        ra, rb = fit_tapered_capsule(pts, [0, 0, 0], [0, 0.4, 0], percentile=99.0, margin_m=0.002)
        self.assertGreater(ra, rb + 0.025)
        self.assertAlmostEqual(ra, 0.092, delta=0.008)
        self.assertAlmostEqual(rb, 0.052, delta=0.008)

    def test_3vertex_binding_pack_roundtrip_and_slit_isolation(self):
        # Two triangles separated by a coincident slit at x=0 (vertices 0,1 vs 3,4).
        pts = np.array([
            [0.0, 0.0, 0.0], [0.0, 1.0, 0.0], [-1.0, 0.0, 0.0],
            [0.0, 0.0, 0.0], [0.0, 1.0, 0.0], [1.0, 0.0, 0.0],
        ])
        faces = np.array([[0, 1, 2], [3, 5, 4]])
        render_pts = np.array([[-0.01, 0.5, 0.002], [0.01, 0.5, 0.002]])
        rows = bind_vertex_frames(pts, faces, render_pts, 0.05)
        # Left point binds exclusively to vertices {0,1,2}; right exclusively to {3,4,5}.
        self.assertTrue(set(rows[0][:3]).issubset({0, 1, 2}))
        self.assertTrue(set(rows[1][:3]).issubset({3, 4, 5}))
        # Pack/unpack roundtrip with 1-based indices and partial seam weight 0.6.
        v0, v1, v2, w0, w1, w2 = rows[0]
        packed = pack_vertex_binding_u16x4(v0 + 1, v1 + 1, v2 + 1, w0, w1, w2, amount=0.6)
        is_v, vids, ws = unpack_vertex_binding_u16x4(packed)
        self.assertTrue(is_v)
        self.assertAlmostEqual(sum(ws), 0.6, delta=0.002)
        for vid, w_orig, w_got in zip((v0 + 1, v1 + 1, v2 + 1), (w0, w1, w2), ws):
            if w_got > 0:
                self.assertIn(vid, vids)
                self.assertAlmostEqual(w_got, w_orig * 0.6, delta=0.003)

    def test_subdivide_long_edges_preserves_manifold_and_bounds_length(self):
        pts = np.array([[0.0, 0.0, 0.0], [0.20, 0.0, 0.0], [0.20, 0.20, 0.0], [0.0, 0.20, 0.0]])
        faces = np.array([[0, 1, 2], [0, 2, 3]])
        rp, rf = subdivide_long_edges(pts, faces, max_edge_m=0.08, max_vertices=64)
        rep = validate_proxy(rp, rf, pins=[0, 1], max_edge_m=0.08)
        self.assertTrue(rep["edge_limit_ok"])
        self.assertGreater(len(rp), len(pts))
        # Every edge must belong to 1 (boundary) or 2 (interior) faces.
        edges = np.sort(np.concatenate([rf[:, [0, 1]], rf[:, [1, 2]], rf[:, [2, 0]]], axis=0), axis=1)
        _, counts = np.unique(edges, axis=0, return_counts=True)
        self.assertLessEqual(int(counts.max()), 2)


if __name__ == "__main__":
    unittest.main()
