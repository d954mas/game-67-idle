"""The authoring library on synthetic shapes: a sheet, a tube over a cylinder body, and a split sheet.

Each operation keeps its invariant, each map rule gives its meaning on a shape whose answer is known, and the
sidecar reads back byte for byte."""
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE.parent / "tools"), str(HERE)]
from synthetic import glb, grid, tube  # noqa: E402
from garment import body as B  # noqa: E402
from garment import sidecar as sc  # noqa: E402
from garment import sim  # noqa: E402
from garment.model import Surface  # noqa: E402
from garment.outfit import Outfit  # noqa: E402

FABRIC = dict(density=0.3, stretch=1e-5, bend=200.0, drag=0.4, thickness=0.004, iterations=1, max_edge_strain=0.12,
              strain_passes=2, aero_drag=0.7, weft_stretch_scale=2.0, diag_stretch_scale=2.5, weft_bend_scale=1.3,
              weft_max_strain=0.18, friction=0.4, compression_limit=0.25)
JOINTS = ["root", "left", "right"]
BODY_R, CLOTH_R = 0.10, 0.13


def lanes(n, joint):
    ids = np.zeros((n, 4), int)
    ids[:, 0] = joint
    w = np.zeros((n, 4))
    w[:, 0] = 1
    return ids, w


def cylinder_scene(hole=None):
    """A body cylinder (radius 0.10, y 0..1, 32 x 41) and a skirt tube (radius 0.13, y 0.40..0.90) whose top row is
    the unlabelled waistband it is sewn to; the skirt's lower half is weighted to `left`. hole: (y0, y1) where the
    body's faces are deleted, as an outfit generator deletes skin under cloth."""
    body_p, body_f = tube(0.0, BODY_R, 0.0, 1.0, around=32, rows=41)
    if hole:
        centre = body_p[body_f].mean(1)[:, 1]
        body_f = body_f[(centre < hole[0]) | (centre > hole[1])]
    cloth_p, cloth_f = tube(0.0, CLOTH_R, 0.40, 0.90, around=24, rows=12)
    label = np.where(cloth_p[:, 1] < 0.899, 1.0, 0.0)
    ids, w = lanes(len(cloth_p), 0)
    ids[cloth_p[:, 1] < 0.65, 0] = 1
    return glb([("skin", body_p, body_f, *lanes(len(body_p), 0), {}),
                ("cloth", cloth_p, cloth_f, ids, w, {"_CLOTH_PIECE": label})], JOINTS)


class Scene:
    def __init__(self, data):
        self.dir = tempfile.TemporaryDirectory()
        self.root = Path(self.dir.name)
        (self.root / "model.glb").write_bytes(data)

    def outfit(self):
        return Outfit(None, self.root, "model.glb", body=["skin"], fabrics={"plain": FABRIC})

    def close(self):
        self.dir.cleanup()


def skirt(outfit, vertices=200):
    p = outfit.piece("skirt", 1, "plain")
    p.create("cloth", vertices)
    p.pin(p.from_render(~p.render.labelled | (p.render.height <= 0.1)))
    p.weights_transfer(p.render.surface(), nearest=4)
    p.gradient(rate=0.5, cap=0.2)
    p.backstop_from_body()
    return p


def split_sheet(n=9):
    """A 0.4 m sheet facing +Z with a slit up the middle column from the bottom edge to its centre."""
    points, faces = grid(-0.2, 0.2, 0.0, 0.4, 0.0, n, n)
    mid = n // 2
    path = [j * n + mid for j in range(mid + 1)]
    return sim.cut(points, faces, path), path


class TestSidecar(unittest.TestCase):
    def test_round_trip_is_byte_for_byte_with_correctives_and_the_plane_form(self):
        scene = Scene(cylinder_scene())
        try:
            o = scene.outfit()
            p = skirt(o)
            p.R[np.flatnonzero(~p.pins)[:3]] = np.inf
            p.correctives["sit"] = (np.full_like(p.x, 0.01), np.zeros_like(p.x))
            data = sc.write(o.built())
            back = sc.read(data)
            self.assertEqual(sc.write(back), data)
            piece = back.piece("skirt")
            self.assertTrue(np.isinf(piece.R).sum() == 3)
            self.assertEqual(list(piece.correctives), ["sit"])
            self.assertEqual(back.joint_names, JOINTS)
            np.testing.assert_array_equal(back.inverse_bind_matrices()[1], np.eye(4))
            np.testing.assert_allclose(piece.weights.sum(1), 1, atol=1e-6)
            self.assertEqual(back.build["inputs"].keys(), {"model.glb"})
            path = o.write()
            self.assertEqual(path.read_bytes(), data)
            self.assertEqual(path.name, "model.cloth.glb")
        finally:
            scene.close()

    def test_writer_names_unset_maps(self):
        scene = Scene(cylinder_scene())
        try:
            o = scene.outfit()
            p = o.piece("skirt", 1, "plain")
            p.create("cloth", 200)
            p.pin(p.from_render(~p.render.labelled))
            with self.assertRaisesRegex(ValueError, "r unset"):
                o.built()
        finally:
            scene.close()


class TestOperations(unittest.TestCase):
    def test_cut_opens_the_path_and_keeps_the_tip_whole(self):
        (points, faces, P), path = split_sheet()
        n = 81
        self.assertEqual(len(points), n + len(path) - 1)
        self.assertEqual(len(faces), 2 * 8 * 8)
        np.testing.assert_allclose(np.asarray(P.sum(1)).ravel(), 1)
        # Across the slit the shortest path goes round its tip: two vertices 5 cm apart are far apart on the cloth.
        geo = sim.geodesic(points, faces, [9 * 1 + 3])
        lip = np.flatnonzero(np.all(np.isclose(points, points[9 * 1 + 4]), axis=1))
        self.assertEqual(len(lip), 2)
        self.assertGreater(geo[lip].max(), 0.15)
        self.assertLess(geo[lip].min(), 0.06)

    def test_delete_drops_triangles_and_orphans(self):
        points, faces = grid(0, 1, 0, 1, 0, 5, 5)
        x, f, P = sim.delete(points, faces, points[:, 0] > 0.6)
        self.assertTrue(np.all(x[:, 0] <= 0.6))
        self.assertEqual(len(np.unique(f)), len(x))
        np.testing.assert_allclose(P @ points, x)

    def test_density_brings_masked_edges_into_the_band_and_carries_weights(self):
        scene = Scene(cylinder_scene())
        try:
            o = scene.outfit()
            p = skirt(o, vertices=150)
            before = p.x[p.pins].copy()
            lower = p.f.y < 0.6
            p.density(lower, edge_mm=30)
            e = sim.edges(p.faces)
            inside = p.x[:, 1] < 0.6
            length = np.linalg.norm(p.x[e[:, 0]] - p.x[e[:, 1]], axis=1)
            self.assertLessEqual(length[inside[e].all(1)].max(), 4 / 3 * 0.030 + 1e-9)
            np.testing.assert_allclose(p.w.sum(1), 1)
            self.assertTrue(np.isnan(p.d).all())
            # The pins kept their places: the edit was below them.
            np.testing.assert_allclose(np.sort(p.x[p.pins], axis=0), np.sort(before, axis=0))
        finally:
            scene.close()

    def test_relax_keeps_boundary_and_surface(self):
        points, faces = grid(0, 1, 0, 1, 0, 7, 7)
        rng = np.random.default_rng(1)
        inner = ~sim.boundary(faces, len(points))
        points[inner, :2] += rng.uniform(-0.04, 0.04, (inner.sum(), 2))
        out = sim.relax(points, faces, np.ones(len(points), bool), iterations=6)
        np.testing.assert_array_equal(out[~inner], points[~inner])
        np.testing.assert_allclose(out[:, 2], 0, atol=1e-12)
        spread = lambda x: np.ptp(np.linalg.norm(x[sim.edges(faces)[:, 0]] - x[sim.edges(faces)[:, 1]], axis=1))
        self.assertLess(spread(out), spread(points))

    def test_offset_moves_along_the_front_normal(self):
        points, faces = grid(0, 1, 0, 1, 0, 4, 4)
        mask = points[:, 0] < 0.5
        out = sim.offset(points, faces, mask, 0.005)
        np.testing.assert_allclose(out[mask, 2], 0.005)
        np.testing.assert_array_equal(out[~mask], points[~mask])


class TestMaps(unittest.TestCase):
    def setUp(self):
        self.scene = Scene(cylinder_scene())

    def tearDown(self):
        self.scene.close()

    def test_gradient_is_rate_times_fabric_path_capped(self):
        o = self.scene.outfit()
        p = o.piece("sheet", 1, "plain")
        points, faces = grid(-0.2, 0.2, 0.0, 0.5, 0.2, 5, 11)
        p.set_mesh(points, faces)
        p.pin(p.f.y > 0.499)
        p.gradient(rate=0.5, cap=0.15)
        # Straight down a column the edge path is the drop itself.
        column = np.isclose(p.f.x, 0.0)
        np.testing.assert_allclose(p.r[column], np.minimum(0.5 * (0.5 - p.f.y[column]), 0.15), atol=1e-12)
        self.assertTrue(np.all(p.r[p.pins] == 0))
        p.smooth("r", p.f.all, 3)
        self.assertTrue(np.all(p.r[p.pins] == 0))
        p.unpin(p.f.y > 0.499)
        self.assertTrue(np.isnan(p.r).any() and not p.pins.any())

    def test_backstop_on_a_cylinder_points_in_and_measures_the_limb(self):
        o = self.scene.outfit()
        p = skirt(o)
        free = ~p.pins
        radial = p.x * [1, 0, 1]
        radial /= np.linalg.norm(radial, axis=1, keepdims=True)
        self.assertGreater(np.min(-np.einsum("ij,ij->i", p.b, radial)), 0.97)
        gap = np.linalg.norm(p.x[:, [0, 2]], axis=1) - BODY_R
        acting = free & (p.R > 0)
        np.testing.assert_allclose(p.d[free], np.maximum(gap[free] - FABRIC["thickness"], 0), atol=2e-3)
        self.assertTrue(acting.any())
        np.testing.assert_allclose(p.R[acting], BODY_R, rtol=0.06)
        self.assertTrue(np.all(p.R[free & (p.d >= p.r)] == 0))

    def test_backstop_is_off_over_a_hole_of_the_body(self):
        scene = Scene(cylinder_scene(hole=(0.45, 0.75)))
        try:
            o = scene.outfit()
            p = o.piece("skirt", 1, "plain")
            p.create("cloth", 200)
            p.pin(p.from_render(~p.render.labelled | (p.render.height <= 0.1)))
            p.weights_transfer(p.render.surface(), nearest=4)
            p.gradient(rate=0.5, cap=0.2)
            holes = p.backstop_from_body()
            over = (p.f.y > 0.47) & (p.f.y < 0.73) & ~p.pins
            self.assertGreater(holes, 0)
            self.assertTrue(np.all(p.R[over] == 0))
            covered = (p.f.y < 0.42) & ~p.pins & (p.d < p.r)
            self.assertTrue(np.all(p.R[covered] > 0))
        finally:
            scene.close()

    def test_layer_splits_the_gap_between_two_tubes(self):
        o = self.scene.outfit()
        inner = skirt(o)
        outer = o.piece("over", 2, "plain")
        x, f = tube(0.0, 0.16, 0.55, 0.88, around=24, rows=8)
        outer.set_mesh(x, f)
        outer.pin(outer.f.y > 0.879)
        outer.weights_transfer(Surface(inner.x, inner.faces, inner.w, inner.joints))
        outer.gradient(rate=0.5, cap=0.2)
        outer.backstop_from_body()
        r_before = inner.r.copy()
        counts = o.layer("over", "skirt")
        self.assertGreater(counts["outer"], 0)
        self.assertGreater(counts["inner"], 0)
        t = 2 * FABRIC["thickness"]
        hit = ~outer.pins & (outer.f.y > 0.6)
        g = np.linalg.norm(outer.x[hit][:, [0, 2]], axis=1) - CLOTH_R
        # The inner mesh is a polygon inside the circle, so the gap along b is a little more than the radii differ.
        np.testing.assert_allclose(outer.d[hit], (g - t) / 2, atol=3e-3)
        under = ~inner.pins & (inner.f.y > 0.6) & (inner.f.y < 0.85)
        self.assertTrue(np.all(inner.r[under] <= (0.16 - CLOTH_R - t) / 2 + 3e-3))
        self.assertTrue(np.all(inner.r <= r_before + 1e-15))
        self.assertEqual(o.layers, [("over", "skirt")])

    def test_from_render_selects_where_every_corner_is_selected(self):
        o = self.scene.outfit()
        p = o.piece("skirt", 1, "plain")
        p.create("cloth", 200)
        band = p.from_render(~p.render.labelled | (p.render.height <= 0.1))
        y = p.render.positions[:, 1]
        self.assertTrue(np.all(p.f.y[band] >= y.max() - 0.1 * np.ptp(y[np.unique(p.render.region)]) - 1e-9))
        self.assertGreater(band.sum(), 10)


class TestWeights(unittest.TestCase):
    def setUp(self):
        self.scene = Scene(cylinder_scene())
        self.o = self.scene.outfit()

    def tearDown(self):
        self.scene.close()

    def test_closest_transfer_reads_each_lip_of_a_cut_from_its_own_side(self):
        (points, faces, _), _ = split_sheet(n=17)
        weights = np.zeros((len(points), 3))
        centre = points[:, 1] > 0.2 + 1e-9
        left = points[:, 0] < -1e-9
        weights[:, 1] = left
        weights[:, 2] = ~left
        # The lips coincide; the copies made by the cut are the right-hand lip.
        lip_right = np.arange(len(points)) >= 17 * 17
        weights[lip_right, 1], weights[lip_right, 2] = 0, 1
        on_line = np.isclose(points[:, 0], 0) & ~lip_right & ~centre
        weights[on_line, 1], weights[on_line, 2] = 1, 0
        source = Surface(points, faces, weights, JOINTS)
        p = self.o.piece("sheet", 1, "plain")
        (sx, sf, _), _ = split_sheet(n=9)
        p.set_mesh(sx, sf)
        got = p.weights_from(source)
        lips = np.isclose(sx[:, 0], 0) & (sx[:, 1] < 0.2 - 1e-9)
        copies = np.arange(len(sx)) >= 81
        np.testing.assert_allclose(got[lips & ~copies, 1], 1, atol=1e-9)
        np.testing.assert_allclose(got[lips & copies, 2], 1, atol=1e-9)
        mixed = p.weights_from(source, nearest=4)
        self.assertTrue(np.any((mixed[lips, 1] > 0.05) & (mixed[lips, 2] > 0.05)))

    def test_set_blend_smooth_keep_rows_normalised(self):
        p = skirt(self.o)
        own = p.w.copy()
        p.weights_set(p.f.y < 0.6, "right", 0.25)
        np.testing.assert_allclose(p.f.w["right"][p.f.y < 0.6], 0.25)
        np.testing.assert_allclose(p.w.sum(1), 1)
        p.weights_blend(p.f.all, own, p.w.copy(), by=np.clip((0.9 - p.f.y) / 0.5, 0, 1))
        np.testing.assert_allclose(p.w.sum(1), 1)
        np.testing.assert_allclose(p.w[p.f.y > 0.899], own[p.f.y > 0.899])
        p.weights_smooth(p.f.all, 5)
        np.testing.assert_allclose(p.w.sum(1), 1)
        self.assertIn("test_authoring.py", p.why(int(np.argmin(p.f.y)))["w"])


class TestBody(unittest.TestCase):
    def test_winding_number_is_one_inside_a_closed_box_and_zero_outside(self):
        corners = np.array([[x, y, z] for x in (0, 1) for y in (0, 1) for z in (0, 1)], float)
        quads = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
        faces = np.array([t for a, b, c, d in quads for t in ((a, b, c), (a, c, d))])
        w = B.winding([[0.5, 0.5, 0.5], [1.5, 0.5, 0.5], [0.9, 0.1, 0.5]], corners[faces])
        np.testing.assert_allclose(w, [1, 0, 1], atol=1e-9)
        sd = B.signed_distance([[0.5, 0.5, 0.2], [0.5, 0.5, 1.3]], corners, faces)
        np.testing.assert_allclose(sd, [-0.2, 0.3], atol=1e-9)


if __name__ == "__main__":
    unittest.main()
