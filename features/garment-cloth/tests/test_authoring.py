"""The authoring library on synthetic shapes: a sheet, a tube over a cylinder body, and a split sheet.

Each operation keeps its invariant, each map rule gives its meaning on a shape whose answer is known, the derived maps
do not depend on the order a script declares them in, every gate fails on a known failure and refuses to pass with
nothing measured, the build record names each input that changed, and the sidecar reads back byte for byte."""
import json
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE.parent / "tools"), str(HERE)]
from synthetic import glb, grid, tube  # noqa: E402
from garment import body as B  # noqa: E402
from garment import build as Bd  # noqa: E402
from garment import gates  # noqa: E402
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


def closed_cylinder(radius, y0, y1, around=32, rows=41):
    """A tube capped at both ends, faces out: a closed body."""
    points, faces = tube(0.0, radius, y0, y1, around=around, rows=rows)
    n = len(points)
    points = np.vstack([points, [[0, y0, 0], [0, y1, 0]]])
    caps = []
    for i in range(around):
        caps.append((n, i, (i + 1) % around))
        top = (rows - 1) * around
        caps.append((n + 1, top + (i + 1) % around, top + i))
    return points, np.vstack([faces, caps])


def cylinder_scene(hole=None, split=False):
    """A body cylinder (radius 0.10, y 0..1, 32 x 41) and a skirt tube (radius 0.13, y 0.40..0.90) whose top row is
    the unlabelled waistband it is sewn to; the skirt's lower half is weighted to `left`. hole: (y0, y1) where the
    body's faces are deleted, as an outfit generator deletes skin under cloth. split: the waistband is a primitive of
    its own and the skirt is labelled to its top row, joined to the band only by coincident positions."""
    body_p, body_f = tube(0.0, BODY_R, 0.0, 1.0, around=32, rows=41)
    if hole:
        centre = body_p[body_f].mean(1)[:, 1]
        body_f = body_f[(centre < hole[0]) | (centre > hole[1])]
    cloth_p, cloth_f = tube(0.0, CLOTH_R, 0.40, 0.90, around=24, rows=12)
    label = np.where(cloth_p[:, 1] < 0.899, 1.0, 0.0)
    ids, w = lanes(len(cloth_p), 0)
    ids[cloth_p[:, 1] < 0.65, 0] = 1
    prims = [("skin", body_p, body_f, *lanes(len(body_p), 0), {})]
    if split:
        band_p, band_f = tube(0.0, CLOTH_R, 0.90, 0.95, around=24, rows=2)
        prims.append(("band", band_p, band_f, *lanes(len(band_p), 0), {}))
        label = np.ones(len(cloth_p))
    prims.append(("cloth", cloth_p, cloth_f, ids, w, {"_CLOTH_PIECE": label}))
    return glb(prims, JOINTS)


def body_scene():
    """The skirt over a closed body cylinder declared as a complete body file."""
    data = cylinder_scene()
    points, faces = closed_cylinder(BODY_R, 0.0, 1.0)
    body = glb([("skin", points, faces, *lanes(len(points), 0), {})], JOINTS)
    scene = Scene(data)
    (scene.root / "body.glb").write_bytes(body)
    (scene.root / "body.provenance.json").write_text(json.dumps(
        {"id": "body", "origin": "mine", "license": "Studio-made test body"}), encoding="utf-8")
    return scene


MODEL_RECORD = {"id": "model", "origin": "mine", "license": "Studio-made test model"}


class Scene:
    def __init__(self, data):
        self.dir = tempfile.TemporaryDirectory()
        self.root = Path(self.dir.name)
        (self.root / "model.glb").write_bytes(data)
        (self.root / "model.provenance.json").write_text(json.dumps(MODEL_RECORD), encoding="utf-8")

    def outfit(self, body=("skin",), **kwargs):
        return Outfit(None, self.root, "model.glb", body=list(body) if isinstance(body, tuple) else body,
                      fabrics={"plain": FABRIC}, **kwargs)

    def close(self):
        self.dir.cleanup()


def skirt(outfit, vertices=200):
    p = outfit.piece("skirt", 1, "plain")
    p.create("cloth", vertices)
    p.pin(p.from_render(~p.render.labelled | (p.render.height <= 0.1)))
    p.weights_transfer(p.render.surface(), nearest=4)
    p.gradient(rate=0.5, cap=0.2)
    return p


def split_sheet(n=9):
    """A 0.4 m sheet facing +Z with a slit up the middle column from the bottom edge to its centre."""
    points, faces = grid(-0.2, 0.2, 0.0, 0.4, 0.0, n, n)
    mid = n // 2
    path = [j * n + mid for j in range(mid + 1)]
    return sim.cut(points, faces, path), path


def gate_data(outfit, data=None, stage=2, shapes=None, seats=None, complete=None):
    data = sc.write(outfit.built()) if data is None else data
    body = outfit.body
    return gates.GateData(sidecar=data, model=str(outfit.model_path),
                          body=(body.points, body.faces, body.weights, body.complete if complete is None else complete,
                                body.rim_faces),
                          shapes=shapes or dict(spheres=[], cones=[]), fabrics={p.id: FABRIC for p in outfit.pieces},
                          clips=[], seats=seats or {}, stage=stage)


def posed(outfit, matrices):
    """Sidecar bytes of the outfit carrying a zero corrective keyed on a pose, and the clip spec that poses it: the
    per-frame gates then see the reference in that pose."""
    built = outfit.built()
    for p in built.pieces:
        p.correctives["pose"] = (np.zeros_like(p.points), np.zeros_like(p.points), np.column_stack([p.d, p.R]))
    record = dict(name="pose", pose=dict(joints=JOINTS, matrices=np.asarray(matrices, float).tolist()))
    built.correctives = [record]
    return sc.write(built), (gates.CORRECTIVE + "pose", None, record)


class TestSidecar(unittest.TestCase):
    def test_round_trip_is_byte_for_byte_with_correctives_and_the_plane_form(self):
        scene = Scene(cylinder_scene())
        try:
            o = scene.outfit()
            p = skirt(o)
            plane = np.zeros(len(p.x), bool)
            plane[np.flatnonzero(~p.pins)[:3]] = True
            p.paint("R", plane, np.inf)
            built = o.built()
            piece = built.piece("skirt")
            key = np.column_stack([piece.d * 0.5, np.where(piece.R > 0, np.inf, 0.0)]).astype(np.float32)
            piece.correctives["sit"] = (np.where(piece.r[:, None] == 0, 0.0, np.full_like(piece.points, 0.01)),
                                        np.zeros_like(piece.points), key)
            built.correctives = [dict(name="sit", clip="sit", t=1.5)]
            data = sc.write(built)
            back = sc.read(data)
            self.assertEqual(sc.write(back), data)
            piece = back.piece("skirt")
            self.assertTrue(np.isinf(piece.R).sum() == 3)
            self.assertEqual(list(piece.correctives), ["sit"])
            np.testing.assert_array_equal(piece.correctives["sit"][2], key)
            self.assertEqual(back.correctives[0]["clip"], "sit")
            self.assertEqual(back.joint_names, JOINTS)
            np.testing.assert_array_equal(back.inverse_bind_matrices()[1], np.eye(4))
            np.testing.assert_allclose(piece.weights.sum(1), 1, atol=1e-6)
            self.assertEqual(back.build["inputs"].keys(), {"model.glb"})
            path = o.write()
            self.assertEqual(sc.read(path.read_bytes()).piece("skirt").points.tobytes(), piece.points.tobytes())
            self.assertEqual(path.name, "model.cloth.glb")
            record = json.loads(path.with_name("model.cloth.provenance.json").read_text(encoding="utf-8"))
            # A derived file carries its model's origin and license and names the record it took them from.
            self.assertEqual((record["origin"], record["license"]), (MODEL_RECORD["origin"], MODEL_RECORD["license"]))
            self.assertEqual((record["source"], record["source_record"]), ("model.glb", "model.provenance.json"))
            (scene.root / "model.provenance.json").unlink()
            with self.assertRaisesRegex(ValueError, "no asset record"):
                o.write()
            (scene.root / "model.provenance.json").write_text(json.dumps(MODEL_RECORD), encoding="utf-8")
            self.assertEqual(record["sha256"], Bd.sha256(path))
            self.assertEqual(record["integrity"], dict(sha256=Bd.sha256(path), bytes=path.stat().st_size))
            self.assertTrue(record["license"] and record["origin"])
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
            self.assertTrue(np.isnan(p.r[~p.pins]).all())
            # The pins kept their places: the edit was below them.
            np.testing.assert_allclose(np.sort(p.x[p.pins], axis=0), np.sort(before, axis=0))
        finally:
            scene.close()

    def test_density_refuses_to_merge_a_pin_with_a_free_vertex(self):
        scene = Scene(cylinder_scene())
        try:
            p = skirt(scene.outfit(), vertices=150)
            with self.assertRaisesRegex(ValueError, "merge pins"):
                p.density(p.f.all, edge_mm=120)
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
        # Tight to a line, then opening below it: past 0.2 m of fabric r grows on from 0.02 at 0.5 m per metre.
        p.gradient(rate=0.1, cap=0.04)
        p.gradient(rate=0.5, cap=0.15, start=0.2, base=0.02)
        drop = 0.5 - p.f.y[column]
        expect = np.where(drop < 0.2, 0.1 * drop, np.minimum(0.02 + 0.5 * (drop - 0.2), 0.15))
        np.testing.assert_allclose(p.r[column], expect, atol=1e-12)
        p.unpin(p.f.y > 0.499)
        self.assertTrue(np.isnan(p.r).any() and not p.pins.any())

    def test_backstop_on_a_cylinder_points_in_and_measures_the_limb(self):
        o = self.scene.outfit()
        skirt(o)
        p = o.resolved_piece("skirt")
        free = ~p.pins
        radial = p.x * [1, 0, 1]
        radial /= np.linalg.norm(radial, axis=1, keepdims=True)
        self.assertGreater(np.min(-np.einsum("ij,ij->i", p.b, radial)), 0.97)
        gap = np.linalg.norm(p.x[:, [0, 2]], axis=1) - BODY_R
        np.testing.assert_allclose(p.d[free], np.maximum(gap[free] - FABRIC["thickness"], 0), atol=2e-3)
        # The body radius is known behind every free vertex, also where the authored r is shorter than d: the runtime
        # may grow r, and a zero R would leave the backstop off there.
        self.assertTrue((free & (p.d >= p.r)).any())
        np.testing.assert_allclose(p.R[free], BODY_R, rtol=0.06)
        self.assertTrue(np.all(p.R[p.pins] == 0))

    def test_the_backstop_does_not_depend_on_the_order_the_script_wrote_r(self):
        # A backstop measured while r is still small would keep R = 0 where a later gradient lets the cloth reach the
        # body; the build derives it from the final state, so the order of the script's writes cannot matter.
        def build(open_after):
            scene = Scene(cylinder_scene())
            try:
                o = scene.outfit()
                p = o.piece("skirt", 1, "plain")
                p.create("cloth", 200)
                p.pin(p.from_render(~p.render.labelled | (p.render.height <= 0.1)))
                p.weights_transfer(p.render.surface(), nearest=4)
                p.gradient(rate=0.02, cap=0.004)
                p.paint("R", p.f.y > 0.85, 0.5)
                if open_after:
                    p.gradient(rate=0.5, cap=0.08)
                return sc.read(sc.write(o.built()))
            finally:
                scene.close()
        late = build(True).piece("skirt")
        reach = (late.r > 0) & (late.d < late.r)
        self.assertTrue(reach.any())
        self.assertTrue(np.all(late.R[reach & (late.points[:, 1] <= 0.85)] > 0))
        # The override comes after the derivation whatever line wrote it.
        np.testing.assert_allclose(late.R[(late.points[:, 1] > 0.85) & (late.r > 0)], 0.5)
        early = build(False).piece("skirt")
        np.testing.assert_array_equal(early.R, late.R)
        np.testing.assert_array_equal(early.d, late.d)

    def test_backstop_is_off_over_a_hole_of_the_body(self):
        scene = Scene(cylinder_scene(hole=(0.45, 0.75)))
        try:
            o = scene.outfit()
            skirt(o)
            p = o.resolved_piece("skirt")
            over = (p.f.y > 0.47) & (p.f.y < 0.73) & ~p.pins
            self.assertTrue(over.any())
            self.assertTrue(np.all(p.R[over] == 0))
            covered = (p.f.y < 0.42) & ~p.pins
            self.assertTrue(np.all(p.R[covered] > 0))
            holes, = gates.holes(sc.read(sc.write(o.built())), o.body)
            self.assertGreater(holes["counts"]["free_over_hole"], 0)
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
        r_before = inner.r.copy()
        # Declared before the weights it reads: the build applies it after them.
        o.layer("over", "skirt")
        outer.weights_set(outer.f.all, "right", 1.0)
        outer, inner = o.resolved_piece("over"), o.resolved_piece("skirt")
        t = 2 * FABRIC["thickness"]
        hit = ~outer.pins & (outer.f.y > 0.6)
        g = np.linalg.norm(outer.x[hit][:, [0, 2]], axis=1) - CLOTH_R
        # The inner mesh is a polygon inside the circle, so the gap along b is a little more than the radii differ.
        np.testing.assert_allclose(outer.d[hit], (g - t) / 2, atol=3e-3)
        under = ~inner.pins & (inner.f.y > 0.6) & (inner.f.y < 0.85)
        self.assertTrue(np.all(inner.r[under] <= (0.16 - CLOTH_R - t) / 2 + 3e-3))
        self.assertTrue(np.all(inner.r <= r_before + 1e-15))
        self.assertEqual(o.layers, [("over", "skirt")])
        # Under the outer the inner rides on the outer's weights.
        np.testing.assert_allclose(inner.f.w["right"][under], 1.0)

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


class TestWeightsFollowBody(unittest.TestCase):
    def test_average_within_r_is_a_mean_over_each_vertex_own_disc(self):
        scene = Scene(cylinder_scene())
        try:
            o = scene.outfit()
            p = o.piece("sheet", 1, "plain")
            points, faces = grid(-0.2, 0.2, 0.0, 0.4, 0.2, 9, 9)
            p.set_mesh(points, faces)
            p.w[:, 1] = p.f.x < 0
            p.w[:, 2] = p.f.x >= 0
            before = p.w.copy()
            p.weights_average(p.f.all, 0.0)
            np.testing.assert_array_equal(p.w, before)
            p.weights_average(p.f.all, 10.0)
            np.testing.assert_allclose(p.w, np.tile(before.mean(0), (len(p.w), 1)))
            p.w[:] = before
            p.weights_average(p.f.all, np.where(np.isclose(p.f.x, 0), 0.06, 0.0))
            middle = np.isclose(p.f.x, 0) & (p.f.y > 0.05) & (p.f.y < 0.35)
            # Within 6 cm along the mesh: the vertex and its four 5 cm neighbours (the diagonals are 7 cm), one of them on
            # the left.
            np.testing.assert_allclose(p.w[middle, 1], 1 / 5, atol=1e-12)
        finally:
            scene.close()

    def test_follow_body_takes_the_body_weights_and_keeps_the_final_pins(self):
        scene = Scene(cylinder_scene())
        try:
            o = scene.outfit()
            p = skirt(o)
            own = p.w.copy()
            p.weights_follow_body()
            with self.assertRaisesRegex(ValueError, "follows the body"):
                p.weights_set(p.f.all, "left", 1.0)
            # A pin added after the declaration keeps its own weights too: the build reads the final pins.
            late = (~p.pins) & (p.f.y < 0.45)
            p.pin(late)
            got = o.resolved_piece("skirt")
            # The body is weighted to `root` alone, so every free vertex follows it; the pins keep their weights.
            np.testing.assert_allclose(got.f.w["root"][~got.pins], 1)
            np.testing.assert_array_equal(got.w[got.pins], own[got.pins])
            self.assertTrue(np.any(own[late, 1] > 0.5))
        finally:
            scene.close()


class TestGates(unittest.TestCase):
    def setUp(self):
        self.scene = Scene(cylinder_scene())
        self.o = self.scene.outfit()

    def tearDown(self):
        self.scene.close()

    def test_a_reference_past_the_limb_axis_fails_and_one_turning_with_it_passes(self):
        from garment.gates import Limb
        rest = np.eye(4)[None]
        ends = ((0, np.array([0.0, 0.2, 0.0]), 0.08), (0, np.array([0.0, 0.8, 0.0]), 0.06))
        points = np.array([[0.12, 0.5, 0.0], [0.5, 0.5, 0.0]])
        r = np.array([0.08, 0.08])
        limb = Limb(ends, rest, points, r, 0.004)
        self.assertEqual(limb.near.tolist(), [True, False])
        crossed = limb.crossing(np.array([[-0.10, 0.5, 0.0], [-0.5, 0.5, 0.0]]), rest, r, 0.004)
        np.testing.assert_allclose(crossed, [0.10, 0.0])
        turn = np.eye(4)
        turn[:3, :3] = [[0, 0, 1], [0, 1, 0], [-1, 0, 0]]
        np.testing.assert_allclose(limb.crossing(points @ turn[:3, :3].T, turn[None], r, 0.004), 0)

    def test_g5_fails_on_a_ninety_degree_blend_of_two_joints(self):
        p = skirt(self.o)
        p.gradient(rate=0.05, cap=0.01)
        quarter = np.eye(4)
        # `left` (the skirt's lower half) turned 90 degrees about the x axis through the joint line y = 0.65.
        quarter[:3, :3] = [[1, 0, 0], [0, 0, -1], [0, 1, 0]]
        quarter[:3, 3] = [0, 0.65, 0.65]
        data, spec = posed(self.o, [np.eye(4), quarter, np.eye(4)])
        still = gates.clip_gates(gate_data(self.o, data), (gates.REST, None), ("G5",))[("G5", "skirt")]
        turned = gates.clip_gates(gate_data(self.o, data), spec, ("G5",))[("G5", "skirt", "pose")]
        self.assertFalse(still["failed"])
        self.assertTrue(turned["failed"])
        self.assertGreater(turned["worst"]["value_mm"], 10)

    def test_g4_and_g7_fail_on_a_reference_held_inside_the_body_and_a_shape(self):
        skirt(self.o).gradient(rate=0.05, cap=0.01)
        # The joint of the lower half pulled 9 cm toward the body axis: those references go deep into the body.
        inward = np.eye(4)
        inward[:3, 3] = [-0.09, 0, 0]
        data, spec = posed(self.o, [np.eye(4), inward, np.eye(4)])
        shapes = dict(spheres=[("root", (0.0, 0.2, 0.0), BODY_R), ("root", (0.0, 0.9, 0.0), BODY_R)], cones=[(0, 1)])
        out = gates.clip_gates(gate_data(self.o, data, complete=True, shapes=shapes), spec, ("G4", "G7"))
        self.assertTrue(out[("G4", "skirt", "pose")]["failed"])
        # Where G4 decides (a complete body at an enforced stage) a sample deeper in the skin than r is G4's alone.
        self.assertGreater(out[("G7", "skirt", "pose")]["masked_by_g4"], 0)
        reported = gates.clip_gates(gate_data(self.o, data, stage=1, complete=False, shapes=shapes), spec, ("G4", "G7"))
        self.assertEqual(reported[("G7", "skirt", "pose")]["masked_by_g4"], 0)
        self.assertTrue(reported[("G7", "skirt", "pose")]["failed"])

    def test_g10_needs_twice_the_covering_fabric_between_body_and_seat(self):
        skirt(self.o)
        built = self.o.built()
        gap = gates.seat_gap(self.o.body, built, {p.id: FABRIC for p in built.pieces})
        covered = np.isfinite(gap)
        y = self.o.body.points[:, 1]
        self.assertTrue(np.all(covered[(y > 0.45) & (y < 0.85)]) and not covered[y < 0.3].any())
        np.testing.assert_allclose(gap[covered], 2 * FABRIC["thickness"])

        def worst(top):
            # A seat whose top is the plane y = top, under the body from below.
            seats = {gates.REST: np.array([[0, 1.0, 0, -top], [0, -1.0, 0, top - 1.0]])}
            gd = gate_data(self.o, sc.write(built), seats=seats)
            gd.seat_gap = gap
            return gates.clip_gates(gd, (gates.REST, None), ("G10",))[("G10", "body")]
        # The body column stands in the seat up to y = 0.5: the covered body under that top is inside it.
        self.assertTrue(worst(0.5)["failed"])
        self.assertFalse(worst(0.40 - 0.01)["failed"])

    def test_a_gate_with_nothing_to_measure_is_not_run_and_fails_where_enforced(self):
        skirt(self.o)
        over = self.o.piece("over", 2, "plain")
        x, f = tube(0.0, 0.15, 0.1, 0.2, around=12, rows=3)
        over.set_mesh(x, f)
        over.pin(over.f.y > 0.199)
        over.weights_transfer(Surface(x, f, np.tile([1.0, 0, 0], (len(x), 1)), JOINTS))
        over.gradient(rate=0.5, cap=0.05)
        # A layer pair whose outer never lies over the inner, and no body shapes at all.
        self.o.over("over", "skirt")
        out = gates.merge([gates.clip_gates(gate_data(self.o), (gates.REST, None), ("G6", "G7", "G8"))])
        findings = gates.decide(list(out.values()), stage=2, complete=True)
        for f in findings:
            self.assertTrue(f.get("not_run"), f)
            self.assertTrue(f["failed"] and f["enforced"], f)
        reported = gates.decide([dict(f, failed=False) for f in out.values()], stage=1, complete=True)
        self.assertTrue(all(f["not_run"] and not f["failed"] and not f["enforced"] for f in reported))
        # A reach gate on the fallback body cannot be decided: not run, failed where enforced.
        g4 = gates.decide([gates.clip_gates(gate_data(self.o), (gates.REST, None), ("G4",))[("G4", "skirt")]], 2, False)
        self.assertTrue(g4[0]["failed"] and "fallback" in g4[0]["undecided"])

    def test_unknown_clips_and_seats_are_refused(self):
        skirt(self.o)
        gd = gate_data(self.o)
        built = sc.read(gd.sidecar)
        with self.assertRaisesRegex(ValueError, "unknown clips"):
            gates.run(gd, self.o.model, self.o.body, built, {"G4"}, clip_names={"wlak"})
        gd.seats = {"sit": np.zeros((1, 4))}
        with self.assertRaisesRegex(ValueError, "seats declared for clips"):
            gates.run(gd, self.o.model, self.o.body, built, {"G10"})

    def test_bake_residual_fails_and_is_enforced_at_every_stage(self):
        skirt(self.o)
        built = self.o.built()
        built.correctives = [dict(name="in", pose=dict(joints=JOINTS, matrices=np.tile(np.eye(4), (3, 1, 1)).tolist()),
                                  drift_mm={"skirt": 9.0}, thickness_mm={"skirt": 4.0}, passes=24, converged=False)]
        bake, = gates.decide(gates.bake_findings(built), stage=1, complete=False)
        self.assertEqual((bake["gate"], bake["failed"], bake["enforced"]), ("bake", True, True))

    def test_g2_sees_a_seam_joined_only_by_coincident_positions(self):
        scene = Scene(cylinder_scene(split=True))
        try:
            o = scene.outfit()
            p = o.piece("skirt", 1, "plain")
            p.create("cloth", 200)
            marks = o.model.marks(1)[o.model.primitive("cloth")]
            # The cloth primitive has no unlabelled corner: its seam is the split row alone.
            self.assertFalse(marks["seam"].any())
            self.assertTrue(marks["split"].any())
            # Held below the seam row, so the row binds to triangles with free corners.
            p.pin((p.f.y > 0.80) & (p.f.y < 0.86))
            p.weights_transfer(p.render.surface(), nearest=4)
            p.gradient(rate=0.5, cap=0.2)
            g2 = [f for f in gates.g2_g3(o.model, sc.read(sc.write(o.built()))) if f["gate"] == "G2"]
            self.assertTrue(g2[0]["failed"] and g2[0]["counts"]["render_vertices"] > 0)
            p.pin_bound(-1.0)
            g2 = [f for f in gates.g2_g3(o.model, sc.read(sc.write(o.built()))) if f["gate"] == "G2"]
            self.assertFalse(g2[0]["failed"])
        finally:
            scene.close()

    def test_the_bind_gate_measures_a_render_vertex_far_from_its_triangle(self):
        p = skirt(self.o)
        model = self.o.model
        k = model.primitive("cloth")
        built = sc.read(sc.write(self.o.built()))
        near = [f for f in gates.g2_g3(model, built) if f["gate"] == "bind"][0]
        self.assertFalse(near["failed"])
        self.assertGreater(near["samples"], 0)
        # The same render vertices bound to a sim mesh shrunk to half its radius lie far outside their triangles.
        piece = built.piece("skirt")
        piece.points = (piece.points * [0.5, 1, 0.5]).astype(piece.points.dtype)
        far = [f for f in gates.g2_g3(model, built) if f["gate"] == "bind"][0]
        self.assertTrue(far["failed"])
        self.assertGreater(far["counts"]["farthest_mm"], 40)
        self.assertTrue(np.isin(far["worst"]["render_vertices"], np.flatnonzero(model.labels[k] == 1)).all())
        del p

    def test_g1_includes_the_cooks_structural_checks(self):
        skirt(self.o)
        built = sc.read(sc.write(self.o.built()))
        self.assertFalse(gates.g1_maps(built, {"skirt": FABRIC})[0]["failed"])
        piece = built.piece("skirt")
        piece.faces = np.vstack([piece.faces, piece.faces[:1]])
        bad = gates.g1_maps(built, {"skirt": FABRIC})[0]
        self.assertTrue(bad["failed"] and "duplicate or degenerate" in bad["structure"])


class TestCook(unittest.TestCase):
    def setUp(self):
        self.scene = Scene(cylinder_scene())
        self.o = self.scene.outfit()

    def tearDown(self):
        self.scene.close()

    def test_blob_carries_the_sidecar_and_the_binding_follows_the_pins(self):
        from garment import cook as C
        from garment_blob import decode
        skirt(self.o)
        data = sc.write(self.o.built())
        blob, streams, report = C.cook("synthetic", data, self.o.model, {"plain": FABRIC}, None, (0.45, 0.35))
        piece = decode(blob)["pieces"][0]
        stored = sc.read(data).piece("skirt")
        np.testing.assert_array_equal(piece["mesh"]["rest"], stored.points)
        np.testing.assert_array_equal(piece["mesh"]["max_distance"], stored.r)
        np.testing.assert_array_equal(piece["direction"], stored.b)
        self.assertEqual(report[0]["pins"], int((stored.r == 0).sum()))
        k = self.o.model.primitive("cloth")
        labelled = self.o.model.labels[k] == 1
        self.assertTrue(streams[k][labelled].any(1).any())
        self.assertFalse(streams[k][~labelled].any())
        staged = C.stage_model(self.scene.root / "model.glb", streams)
        self.assertIn(b"_GARMENT", staged[:4096])

    def test_pin_bound_makes_the_seam_follow_skinning(self):
        p = self.o.piece("skirt", 1, "plain")
        p.create("cloth", 200)
        # Held below the seam row, so the row binds to triangles with free corners.
        p.pin((p.f.y > 0.80) & (p.f.y < 0.86))
        p.weights_transfer(p.render.surface(), nearest=4)
        p.gradient(rate=0.5, cap=0.2)
        before = [f for f in gates.g2_g3(self.o.model, sc.read(sc.write(self.o.built()))) if f["gate"] == "G2"]
        self.assertTrue(before[0]["failed"])
        added = p.pin_bound(-1.0)
        self.assertGreater(added, 0)
        after = gates.g2_g3(self.o.model, sc.read(sc.write(self.o.built())))
        self.assertTrue(all(not f["failed"] for f in after if f["gate"] in ("G2", "G3")))

    def test_layer_keeps_the_order_at_rest(self):
        inner = skirt(self.o)
        outer = self.o.piece("over", 2, "plain")
        x, f = tube(0.0, 0.16, 0.55, 0.88, around=24, rows=8)
        outer.set_mesh(x, f)
        outer.pin(outer.f.y > 0.879)
        outer.weights_transfer(Surface(inner.x, inner.faces, inner.w, inner.joints))
        outer.gradient(rate=0.5, cap=0.2)
        self.o.over("over", "skirt")
        before = gates.clip_gates(gate_data(self.o), (gates.REST, None), ("G8",))
        self.assertTrue(before[("G8", "over>skirt")]["failed"])
        self.o.layer("over", "skirt")
        after = gates.clip_gates(gate_data(self.o), (gates.REST, None), ("G8",))
        self.assertFalse(after[("G8", "over>skirt")]["failed"], after[("G8", "over>skirt")]["worst"])

    def test_a_corrective_is_gated_at_its_key_with_the_reference_it_gives(self):
        skirt(self.o)
        built = self.o.built()
        # A corrective that draws the free cloth halfway into the body: the plain reference passes the reach gate, the
        # corrected one at its key does not, and its record names the corrective.
        piece = built.piece("skirt")
        free = piece.r[:, None] > 0
        inward = np.where(free, (piece.points * [1, 0, 1]) * -(CLOTH_R - 0.5 * BODY_R) / CLOTH_R, 0.0)
        piece.correctives["in"] = (inward.astype(np.float32), np.zeros_like(piece.points), np.column_stack([piece.d, piece.R]))
        built.correctives = [dict(name="in", pose=dict(joints=JOINTS, matrices=np.tile(np.eye(4), (3, 1, 1)).tolist()))]
        data = sc.write(built)
        gd = gate_data(self.o, data, complete=True)
        plain = gates.clip_gates(gd, (gates.REST, None), ("G4",))
        self.assertFalse(plain[("G4", "skirt")]["failed"])
        spec, = gates.corrective_specs(sc.read(data), [])
        corrected = gates.clip_gates(gd, spec, ("G4", "G10"))
        self.assertEqual(set(corrected), {("G4", "skirt", "in")})
        self.assertTrue(corrected[("G4", "skirt", "in")]["failed"])
        self.assertEqual(corrected[("G4", "skirt", "in")]["corrective"], "in")

    def test_a_clip_the_game_plays_with_a_corrective_is_gated_on_the_corrected_reference(self):
        # A held clip (three keys, no motion) whose manifest corrective draws the free cloth halfway into the body:
        # gated plain it passes; gated as the game plays it, on the corrected reference, every frame fails, and the
        # plain reference of that clip is not gated at all.
        from synthetic import glb as write_glb
        cloth_p, cloth_f = tube(0.0, CLOTH_R, 0.40, 0.90, around=24, rows=12)
        clip = write_glb([("cloth", cloth_p, cloth_f, *lanes(len(cloth_p), 0), {})], JOINTS,
                         animation={"left": ([0.0, 0.5, 1.0], np.zeros((3, 3)))})
        (self.scene.root / "held.glb").write_bytes(clip)
        skirt(self.o)
        built = self.o.built()
        piece = built.piece("skirt")
        free = piece.r[:, None] > 0
        inward = np.where(free, (piece.points * [1, 0, 1]) * -(CLOTH_R - 0.5 * BODY_R) / CLOTH_R, 0.0)
        piece.correctives["in"] = (inward.astype(np.float32), np.zeros_like(piece.points), np.column_stack([piece.d, piece.R]))
        built.correctives = [dict(name="in", clip="held", t=0.5)]
        data = sc.write(built)
        gd = gate_data(self.o, data, complete=True)
        spec = ("held", self.scene.root / "held.glb")
        plain = gates.clip_gates(gd, spec, ("G4",))
        self.assertFalse(plain[("G4", "skirt")]["failed"])
        self.assertEqual(plain[("G4", "skirt")]["frames"], 3)
        gd.clip_correctives = {"held": "in"}
        shown = gates.clip_gates(gd, spec, ("G4",))
        record = shown[("G4", "skirt")]
        self.assertEqual((record["frames"], record["frames_failed"]), (3, 3))
        self.assertEqual((record["worst"]["clip"], record["worst"]["corrective"]), ("held", "in"))
        self.assertEqual(list(record["clips"]), ["held with in"])
        # The manifest names the clips: only correctives the sidecar has are mapped.
        (self.scene.root / "clips.json").write_text(json.dumps({"animations": [
            {"name": "held", "resource": "held.glb", "game": {"corrective": "in"}},
            {"name": "other", "resource": "held.glb", "game": {"corrective": "missing"}}]}), encoding="utf-8")
        self.assertEqual(gates.clip_correctives(sc.read(data), self.scene.root / "clips.json"), {"held": "in"})

    def test_cross_fades_between_seated_clips_on_one_seat_are_reported_at_their_weights(self):
        # Two held seated clips on one seat, one playing a corrective that draws the free cloth halfway into the body:
        # fading out of it shows a share of the corrective on a blend of the poses, measured under its own report.
        from synthetic import glb as write_glb
        cloth_p, cloth_f = tube(0.0, CLOTH_R, 0.40, 0.90, around=24, rows=12)
        clip = write_glb([("cloth", cloth_p, cloth_f, *lanes(len(cloth_p), 0), {})], JOINTS,
                         animation={"left": ([0.0, 0.5, 1.0], np.zeros((3, 3)))})
        (self.scene.root / "held.glb").write_bytes(clip)
        skirt(self.o)
        built = self.o.built()
        piece = built.piece("skirt")
        free = piece.r[:, None] > 0
        inward = np.where(free, (piece.points * [1, 0, 1]) * -(CLOTH_R - 0.5 * BODY_R) / CLOTH_R, 0.0)
        piece.correctives["in"] = (inward.astype(np.float32), np.zeros_like(piece.points), np.column_stack([piece.d, piece.R]))
        built.correctives = [dict(name="in", clip="sit", t=0.5)]
        data = sc.write(built)
        seat = [[0.0, 1.0, 0.0, -0.2]]
        gd = gate_data(self.o, data, complete=True, seats={"sit": seat, "perch": seat, "stand": [[0, 1, 0, 0]]})
        path = self.scene.root / "held.glb"
        gd.clips = [(gates.REST, None), ("sit", path), ("perch", path), ("stand", path)]
        gd.clip_correctives = {"sit": "in"}
        specs = gates.crossfade_specs(gd)
        self.assertEqual(sorted({s[0].split("@")[0] for s in specs}), ["crossfade:perch>sit", "crossfade:sit>perch"])
        self.assertEqual(len(specs), 2 * len(gates.CROSSFADE_U))
        early = next(s for s in specs if s[0] == f"{gates.CROSSFADE}sit>perch@0.25")
        late = next(s for s in specs if s[0] == f"{gates.CROSSFADE}sit>perch@0.75")
        deep = gates.clip_gates(gd, early, ("G4",))[("crossfade", "G4", "skirt", "sit>perch")]
        shallow = gates.clip_gates(gd, late, ("G4",))[("crossfade", "G4", "skirt", "sit>perch")]
        self.assertTrue(deep["failed"] and not deep["enforced"])
        self.assertEqual((deep["measure"], deep["frames"]), ("G4", 3))
        self.assertGreater(deep["worst"]["value_mm"], shallow["worst"]["value_mm"] if shallow["worst"] else 0.0)
        # The blend's ends are the configurations themselves.
        p = sc.read(data).piece("skirt")
        for got, want in zip(gates.blend(p, {"in": 1.0}), gates.configuration(p, "in")):
            np.testing.assert_array_equal(got, want)
        for got, want in zip(gates.blend(p, {}), gates.configuration(p, None)):
            np.testing.assert_array_equal(got, want)

    def test_the_gates_blend_backstops_and_directions_by_the_runtime_rule(self):
        skirt(self.o)
        p = sc.read(sc.write(self.o.built())).piece("skirt")
        free = np.flatnonzero((p.r > 0) & (p.R > 0))[:2]
        rest = p.b[free].astype(float)
        rest /= np.linalg.norm(rest, axis=1, keepdims=True)
        side = np.cross(rest, [0.0, 1.0, 0.0])
        side /= np.linalg.norm(side, axis=1, keepdims=True)
        turn = np.zeros(p.points.shape)
        turn[free] = side - p.b[free]
        key = np.column_stack([p.d, p.R]).astype(float)
        key[free] = [0.0, 0.0]
        p.correctives["side"] = (np.zeros(p.points.shape, np.float32), turn.astype(np.float32), key)
        for w in (0.25, 0.5):
            _, b, d, R = gates.blend(p, {"side": w})
            angle = np.arccos(np.clip(np.einsum("ij,ij->i", b[free], rest), -1, 1))
            np.testing.assert_allclose(angle, w * np.arccos(np.clip(np.einsum("ij,ij->i", side, rest), -1, 1)), atol=1e-5)
            # An off key is a flat body at the reach: d moves toward r, the curvature toward 0.
            np.testing.assert_allclose(d[free], (1 - w) * p.d[free] + w * p.r[free], rtol=1e-5)
            self.assertTrue(np.all(R[free] > 0))

    def test_the_layer_gate_reads_the_backstop_of_the_corrected_reference(self):
        inner = skirt(self.o)
        outer = self.o.piece("over", 2, "plain")
        x, f = tube(0.0, 0.16, 0.55, 0.88, around=24, rows=8)
        outer.set_mesh(x, f)
        outer.pin(outer.f.y > 0.879)
        outer.weights_transfer(Surface(inner.x, inner.faces, inner.w, inner.joints))
        outer.gradient(rate=0.5, cap=0.2)
        self.o.layer("over", "skirt")
        built = self.o.built()
        # A corrective that moves nothing but lets the outer reach toward the inner by 5 cm at its key: the layer
        # inequality that holds on the rest backstop breaks on the key's.
        for piece in built.pieces:
            key = np.column_stack([piece.d, piece.R])
            if piece.id == "over":
                key[piece.r > 0, 0] = 0.05
            piece.correctives["reach"] = (np.zeros_like(piece.points), np.zeros_like(piece.points), key)
        built.correctives = [dict(name="reach", pose=dict(joints=JOINTS, matrices=np.tile(np.eye(4), (3, 1, 1)).tolist()))]
        data = sc.write(built)
        gd = gate_data(self.o, data)
        self.assertFalse(gates.clip_gates(gd, (gates.REST, None), ("G8",))[("G8", "over>skirt")]["failed"])
        spec, = gates.corrective_specs(sc.read(data), [])
        self.assertTrue(gates.clip_gates(gd, spec, ("G8",))[("G8", "over>skirt", "reach")]["failed"])

    def test_the_backstop_rule_at_a_key_is_the_build_rule(self):
        # Measured on the rest configuration against the rest body, the key rule gives the maps the build wrote,
        # layer rule and overrides included.
        inner = skirt(self.o)
        outer = self.o.piece("over", 2, "plain")
        x, f = tube(0.0, 0.16, 0.55, 0.88, around=24, rows=8)
        outer.set_mesh(x, f)
        outer.pin(outer.f.y > 0.879)
        outer.weights_transfer(Surface(inner.x, inner.faces, inner.w, inner.joints))
        outer.gradient(rate=0.5, cap=0.2)
        outer.paint("R", outer.f.y < 0.6, 0.0)
        self.o.layer("over", "skirt")
        for p in self.o.resolve()[0]:
            b, d, R = self.o.backstop_at(p, p.x, self.o.body)
            np.testing.assert_array_equal(d, p.d)
            np.testing.assert_array_equal(R, p.R)
            np.testing.assert_array_equal(b, p.b)

    def test_seats_given_as_a_function_are_build_inputs(self):
        (self.scene.root / "seat.json").write_text(json.dumps({"planes": [[0, 1, 0, -0.4]]}), encoding="utf-8")

        def seats(read):
            return {"sit": json.loads(read("seat.json").read_text(encoding="utf-8"))["planes"]}
        o = self.scene.outfit(seats=seats)
        self.assertIn("seat.json", o.inputs)
        self.assertEqual(o.seats, {"sit": [[0, 1, 0, -0.4]]})

    def test_a_seated_clip_without_a_seat_is_refused(self):
        (self.scene.root / "clips.json").write_text(json.dumps({"animations": [
            {"name": "sit", "resource": "model.glb", "game": {"state": "seated"}},
            {"name": "walk", "resource": "model.glb"}]}), encoding="utf-8")
        (self.scene.root / "colliders.json").write_text("{}", encoding="utf-8")
        o = self.scene.outfit(colliders="colliders.json", clips="clips.json")
        skirt(o)
        with self.assertRaisesRegex(ValueError, "seated clip"):
            o.corrective("sit", clip="sit", t=1.0)
        with self.assertRaisesRegex(ValueError, "no clip"):
            o.corrective("run", clip="run", t=1.0)
        o.corrective("sit", clip="sit", t=1.0, convexes=[])
        o.corrective("walk", clip="walk", t=0.5)
        self.assertEqual([r["convexes"] for r in o.corrective_requests], [[], []])


COLLIDERS = dict(schema="garment.colliders.v1", body_sha256="", cones=[[0, 1]],
                 spheres=[dict(name="a", bone="root", center=[0, 0, 0], radius=0.1),
                          dict(name="b", bone="root", center=[0, 1, 0], radius=0.1)])


class TestReceipt(unittest.TestCase):
    """check.py writes the receipt the cook accepts instead of running the per-frame gates: one that certifies exactly
    what the gates read, that no hand edit or change during the check gets past, on colliders the declaration names."""

    SCRIPT = """import sys
sys.path.insert(0, {tools!r})
from garment.outfit import Outfit
FABRIC = {fabric!r}
o = Outfit(__file__, {root!r}, "model.glb", body=["skin"], fabrics={{"plain": FABRIC}}{extra})
p = o.piece("skirt", 1, "plain")
p.create("cloth", 120)
p.pin(p.from_render(~p.render.labelled | (p.render.height <= 0.1)))
p.weights_transfer(p.render.surface(), nearest=4)
p.gradient(rate=0.5, cap=0.2)
o.write()
"""
    DECLARATION = {"script": "outfit.py", "stage": 1, "sidecar": "model.cloth.glb", "colliders": "colliders.json"}

    def setUp(self):
        import runpy
        self.scene = Scene(cylinder_scene())
        self.root = self.scene.root
        self.script = self.root / "outfit.py"
        self.declaration = self.root / "outfit.json"
        for name in ("colliders.json", "other-colliders.json"):
            (self.root / name).write_text(json.dumps(COLLIDERS), encoding="utf-8")
        self.write_script()
        self.declaration.write_text(json.dumps(self.DECLARATION), encoding="utf-8")
        runpy.run_path(str(self.script), run_name="__main__")

    def tearDown(self):
        self.scene.close()

    def write_script(self, extra=""):
        self.script.write_text(self.SCRIPT.format(tools=str(HERE.parent / "tools"), fabric=FABRIC, root=str(self.root),
                                                  extra=extra), encoding="utf-8")

    def check(self, *args):
        from garment import check
        return check.main([str(self.script), "--workers", "1", *args])

    def test_a_check_writes_a_receipt_the_cook_accepts(self):
        from garment import receipt as R
        self.assertEqual(self.check(), 0)
        path = R.path_for(self.script)
        first = path.read_bytes()
        findings = R.accept(self.root, self.declaration)
        self.assertIn("G1", {f["gate"] for f in findings})
        doc = json.loads(first)
        self.assertTrue({"outfit.py", "outfit.json", "colliders.json", "model.glb", "model.cloth.glb"} <= set(doc["files"]))
        self.assertIn("features/garment-cloth/tools/garment/gates.py", doc["modules"])
        self.assertIn("python", doc["versions"])
        # The same check writes the same bytes.
        self.assertEqual(self.check(), 0)
        self.assertEqual(path.read_bytes(), first)
        # A changed input makes the receipt stale, and the refusal names it.
        sidecar = self.root / "model.cloth.glb"
        kept = sidecar.read_bytes()
        sidecar.write_bytes(kept + b"\0")
        with self.assertRaisesRegex(ValueError, "stale .*file model.cloth.glb"):
            R.verify(self.root, self.declaration)
        sidecar.write_bytes(kept)
        path.unlink()
        with self.assertRaisesRegex(ValueError, "no check receipt"):
            R.verify(self.root, self.declaration)
        # A partial check writes none.
        self.assertEqual(self.check("--gates", "G1"), 0)
        self.assertFalse(path.exists())

    def test_a_hand_edited_receipt_is_refused(self):
        from garment import receipt as R
        self.assertEqual(self.check(), 0)
        path = R.path_for(self.script)
        original = json.loads(path.read_text(encoding="utf-8"))

        def edited(change, digest):
            doc = json.loads(json.dumps(original))
            change(doc)
            if digest:
                doc["digest"] = R._digest(doc)
            path.write_text(json.dumps(doc), encoding="utf-8")

        # Any edit without check.py's digest: a deleted file, a pasted hash, a deleted failing finding.
        for change in (lambda d: d["files"].pop("colliders.json"),
                       lambda d: d["files"].__setitem__("model.glb", "0" * 64),
                       lambda d: d["findings"].pop()):
            edited(change, digest=False)
            with self.assertRaisesRegex(ValueError, "not what check.py wrote"):
                R.verify(self.root, self.declaration)
        # Even with the digest redone, the files and modules must be exactly those a check of the outfit reads.
        edited(lambda d: d["files"].pop("colliders.json"), digest=True)
        with self.assertRaisesRegex(ValueError, "file colliders.json not checked"):
            R.verify(self.root, self.declaration)
        edited(lambda d: d["modules"].__setitem__("features/garment-cloth/tools/garment/extra.py", "0" * 64), digest=True)
        with self.assertRaisesRegex(ValueError, "extra.py not read by this outfit's check"):
            R.verify(self.root, self.declaration)
        # A recorded failure of a gate the stage enforces is refused; a reported one is not.
        edited(lambda d: d["findings"].append(dict(gate="G1", piece="skirt", failed=True, enforced=True)), digest=True)
        with self.assertRaisesRegex(ValueError, "records enforced failures: G1 skirt"):
            R.verify(self.root, self.declaration)
        edited(lambda d: d["findings"].append(dict(gate="G4", piece="skirt", failed=True, enforced=False)), digest=True)
        R.accept(self.root, self.declaration)
        # The receipt must be of the declared script, sidecar and stage.
        edited(lambda d: d.__setitem__("stage", 2), digest=True)
        with self.assertRaisesRegex(ValueError, "checked stage 2, the declaration ships 1"):
            R.verify(self.root, self.declaration)

    def test_an_input_changed_during_the_check_writes_no_receipt(self):
        from garment import check, receipt as R
        path = R.path_for(self.script)
        colliders = self.root / "colliders.json"
        gates_run = check.run_gates

        def changing(*args, **kwargs):
            out = gates_run(*args, **kwargs)
            colliders.write_text(json.dumps(dict(COLLIDERS, cones=[])), encoding="utf-8")
            return out
        check.run_gates = changing
        try:
            self.assertEqual(self.check(), 1)
        finally:
            check.run_gates = gates_run
        self.assertFalse(path.exists())
        colliders.write_text(json.dumps(COLLIDERS), encoding="utf-8")
        # Between the build and the first gate: the declaration, a build input, differs from what the build hashed.
        built = check.build

        def edited_after_build(script):
            out = built(script)
            self.declaration.write_text(json.dumps(self.DECLARATION, indent=2), encoding="utf-8")
            return out
        check.build = edited_after_build
        try:
            self.assertEqual(self.check(), 1)
        finally:
            check.build = built
        self.assertFalse(path.exists())

    def test_the_declaration_is_the_one_source_of_the_colliders(self):
        import runpy
        from garment import cook as C, receipt as R
        from garment.sidecar import read
        # The script may name the declared colliders, not others.
        self.write_script(', colliders="other-colliders.json"')
        with self.assertRaisesRegex(ValueError, "one source of the outfit's body shapes"):
            runpy.run_path(str(self.script), run_name="__main__")
        self.write_script(', colliders="colliders.json"')
        runpy.run_path(str(self.script), run_name="__main__")
        self.assertEqual(self.check(), 0)
        sidecar = read((self.root / "model.cloth.glb").read_bytes())
        self.assertEqual(sidecar.check["colliders"], "colliders.json")
        # Declaring other colliders after the check makes the sidecar and its receipt stale: the declaration is a
        # build and check input.
        self.declaration.write_text(json.dumps(dict(self.DECLARATION, colliders="other-colliders.json")), encoding="utf-8")
        self.assertIn("input outfit.json", C.stale(sidecar, self.root, self.script))
        with self.assertRaisesRegex(ValueError, "stale .*file outfit.json"):
            R.accept(self.root, self.declaration)


class TestRecord(unittest.TestCase):
    """A built file's record lists every source that fed it; one derived from a purchased export needs the project
    owner's grant, and the build never writes it otherwise."""

    def test_a_sidecar_derived_from_a_purchased_body_needs_the_grant(self):
        scene = body_scene()
        try:
            o = Outfit(None, scene.root, "model.glb", body="body.glb", fabrics={"plain": FABRIC})
            skirt(o)
            o.write()
            record = json.loads((scene.root / "model.cloth.provenance.json").read_text(encoding="utf-8"))
            self.assertEqual([(s["file"], s["record"], s["purchased"]) for s in record["sources"]],
                             [("model.glb", "model.provenance.json", False),
                              ("body.glb", "body.provenance.json", False)])
            (scene.root / "model.cloth.glb").unlink()
            grant = scene.root / Bd.GRANT
            grant.parent.mkdir(parents=True)
            grant.write_text("# Grant\n\n## A pack\n\n| Tracked export | Source |\n| --- | --- |\n"
                             "| `body.glb` | pack/body.fbx |\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, r"model.cloth.glb is derived from .*\(body.glb\).*PURCHASED_EXPORTS"):
                o.write()
            self.assertFalse((scene.root / "model.cloth.glb").exists())
            grant.write_text(grant.read_text(encoding="utf-8") + "| `model.cloth.glb` | body.glb |\n", encoding="utf-8")
            o.write()
            record = json.loads((scene.root / "model.cloth.provenance.json").read_text(encoding="utf-8"))
            self.assertEqual([s["purchased"] for s in record["sources"]], [False, True])
        finally:
            scene.close()

    def test_a_clip_is_found_by_its_manifest_row_and_listed_by_folder_and_name(self):
        scene = Scene(cylinder_scene())
        try:
            clips = scene.root / "assets/clips"
            clips.mkdir(parents=True)
            (clips / "walk.glb").write_bytes(b"clip")
            (clips / "animations.manifest.json").write_text(json.dumps({"animations": [
                {"name": "walk", "resource": "assets/clips/walk.glb", "origin": "sourced", "license": "Mixamo terms"}]}),
                encoding="utf-8")
            grant = scene.root / Bd.GRANT
            grant.parent.mkdir(parents=True, exist_ok=True)
            grant.write_text("## Mixamo\n\n| Tracked export (`assets/clips/`) |\n| --- |\n| `walk.glb` |\n", encoding="utf-8")
            entry, = Bd.derived_sources(scene.root, [clips / "walk.glb"], clips / "walk.glb")
            self.assertEqual((entry["record"], entry["row"], entry["license"], entry["purchased"]),
                             ("assets/clips/animations.manifest.json", "walk", "Mixamo terms", True))
            with self.assertRaisesRegex(ValueError, "needs its own entry"):
                Bd.derived_sources(scene.root, [scene.root / "model.glb", clips / "walk.glb"], scene.root / "model.cloth.glb")
            self.assertFalse(Bd.granted(scene.root, clips / "run.glb"))
        finally:
            scene.close()


class TestStale(unittest.TestCase):
    """The build record names each input that changed; the cook refuses on any of them."""

    def test_every_input_of_the_record_is_compared_and_named(self):
        from garment import cook as C
        scene = Scene(cylinder_scene())
        try:
            root = scene.root
            (root / "helper.py").write_text("SCALE = 0.5\n", encoding="utf-8")
            (root / "fabrics.json").write_text(json.dumps({"fabrics": {"plain": FABRIC}}), encoding="utf-8")
            script = root / "outfit.py"
            script.write_text(
                "import sys\nfrom pathlib import Path\nsys.path[:0] = [str(Path(__file__).parent)]\nimport helper\n"
                "from garment.outfit import Outfit\n"
                "o = Outfit(__file__, Path(__file__).parent, 'model.glb', body=['skin'], fabrics='fabrics.json')\n"
                "p = o.piece('skirt', 1, 'plain')\np.create('cloth', 120)\n"
                "p.pin(p.from_render(~p.render.labelled | (p.render.height <= 0.1)))\n"
                "p.weights_transfer(p.render.surface(), nearest=4)\np.gradient(rate=helper.SCALE, cap=0.2)\no.write()\n",
                encoding="utf-8")
            import runpy
            runpy.run_path(str(script), run_name="__main__")
            built = sc.read((root / "model.cloth.glb").read_bytes())
            self.assertTrue(any(name.endswith("helper.py") for name in built.build["modules"]))
            self.assertEqual(C.stale(built, root, script), [])
            self.assertEqual(set(built.build["versions"]), {"numpy", "scipy", "python"})
            colliders = root / "colliders.json"
            colliders.write_text(json.dumps(dict(body_sha256=Bd.sha256(root / "model.glb"))), encoding="utf-8")
            self.assertEqual(C.stale(built, root, script, "colliders.json"), [])
            colliders.write_text(json.dumps(dict(body_sha256="0" * 64)), encoding="utf-8")
            self.assertTrue(any("fitted on body" in s for s in C.stale(built, root, script, "colliders.json")))
            (root / "fabrics.json").write_text(json.dumps({"fabrics": {"plain": dict(FABRIC, thickness=0.005)}}),
                                               encoding="utf-8")
            self.assertEqual(C.stale(built, root, script), ["input fabrics.json"])
            old = dict(built.build, versions=dict(built.build["versions"], numpy="0.0"), library="0.0.0")
            built.build = old
            named = C.stale(built, root, script)
            self.assertIn("version numpy 0.0 -> " + np.__version__, named)
            self.assertTrue(any(s.startswith("library 0.0.0") for s in named))
            script.write_text(script.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            self.assertTrue(any(s.startswith("script") for s in C.stale(built, root, script)))
        finally:
            scene.close()

    def test_imported_modules_are_found_from_the_import_statements(self):
        found = Bd.imported_modules(HERE.parent / "tools/garment/check.py", [HERE.parent])
        names = {f.name for f in found}
        self.assertTrue({"outfit.py", "gates.py", "cards.py", "build.py", "garment_blob.py"} <= names)
        self.assertNotIn("check.py", names)


class TestBody(unittest.TestCase):
    def test_winding_number_is_one_inside_a_closed_box_and_zero_outside(self):
        corners = np.array([[x, y, z] for x in (0, 1) for y in (0, 1) for z in (0, 1)], float)
        quads = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
        faces = np.array([t for a, b, c, d in quads for t in ((a, b, c), (a, c, d))])
        w = B.winding([[0.5, 0.5, 0.5], [1.5, 0.5, 0.5], [0.9, 0.1, 0.5]], corners[faces])
        np.testing.assert_allclose(w, [1, 0, 1], atol=1e-9)
        sd = B.signed_distance([[0.5, 0.5, 0.2], [0.5, 0.5, 1.3]], corners, faces)
        np.testing.assert_allclose(sd, [-0.2, 0.3], atol=1e-9)

    def test_the_proof_catches_what_corrupts_the_winding_number(self):
        points, faces = closed_cylinder(0.1, 0.0, 1.0, around=16, rows=6)
        self.assertEqual(B.prove(points, faces), [])
        flipped = faces.copy()
        flipped[3] = flipped[3][::-1]
        self.assertTrue(any("orientation" in f for f in B.prove(points, flipped)))
        self.assertTrue(any("negative volume" in f for f in B.prove(points, faces[:, ::-1])))
        self.assertTrue(any("open edges" in f for f in B.prove(points, faces[:-1])))
        # A second copy of the surface turned through it: one part, triangles crossing.
        turned = points @ np.array([[1, 0, 0], [0, 0, 1], [0, -1, 0]]).T + [0, 0.5, 0.5]
        both = np.vstack([points, turned])
        crossing = B.prove(both, np.vstack([faces, faces + len(points)]))
        self.assertFalse(any("cross" in f for f in crossing))  # two closed parts may overlap
        wedge = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0], [0.2, 0.2, -0.5], [0.2, 0.2, 0.5], [0.9, 0.9, 0.0]])
        tris = np.array([[0, 1, 2], [3, 4, 5]])
        self.assertTrue(any("cross" in f for f in B.prove(wedge, np.vstack([tris, [[0, 2, 3], [2, 3, 5]]]))))

    def test_an_outfit_refuses_a_body_file_that_is_not_proven_complete(self):
        scene = body_scene()
        try:
            o = scene.outfit(body="body.glb")
            self.assertTrue(o.body.complete)
            points, faces = closed_cylinder(BODY_R, 0.0, 1.0)
            faces = faces.copy()
            faces[5] = faces[5][::-1]
            (scene.root / "body.glb").write_bytes(glb([("skin", points, faces, *lanes(len(points), 0), {})], JOINTS))
            with self.assertRaisesRegex(ValueError, "not a proven complete body"):
                scene.outfit(body="body.glb")
        finally:
            scene.close()


if __name__ == "__main__":
    unittest.main()
