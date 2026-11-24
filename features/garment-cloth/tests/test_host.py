"""The offline host on a synthetic sheet over a cylinder.

`--emit <blob>` writes the outfit test_host.c steps through the host and through the solver directly. With the
garment_host library as the argument it runs the Python side: a ramp reaches its key pose, a stored corrective is the
reference the solver sees, and a bake converges to a drape that is an equilibrium of the runtime."""
import os
import sys
import unittest
from pathlib import Path

import numpy as np

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path[:0] = [str(TOOLS), str(Path(__file__).resolve().parent)]
from garment_blob import decode, encode  # noqa: E402
from garment.cook import cook_mesh  # noqa: E402
from garment import corrective as C  # noqa: E402
from garment.body import Body  # noqa: E402
from garment.host import Host, Skeleton, bake, deltas, ramp  # noqa: E402
from garment.outfit import Outfit  # noqa: E402

LIBRARY = None
THICKNESS = 0.004
MATERIAL = dict(density=0.33, stretch=8e-6, bend=450.0, drag=0.4, thickness=THICKNESS, iterations=1, max_edge_strain=0.12,
                strain_passes=2, aero_drag=0.7, weft_stretch_scale=2.2, diag_stretch_scale=2.8, weft_bend_scale=1.3,
                weft_max_strain=0.18, friction=0.4, compression_limit=0.25)


def sheet_over_cylinder(correctives=()):
    """A 0.4 m sheet 6 mm above a cylinder of radius 0.1 along Z, pinned along the cylinder's top line, on one joint;
    with corrective names, each a zero shape to store a bake in."""
    n = 9
    xs = np.linspace(-0.2, 0.2, n)
    points = np.array([(x, 0.11, z) for z in xs for x in xs])
    faces = [t for j in range(n - 1) for i in range(n - 1)
             for a, b, c, d in [(j * n + i, j * n + i + 1, (j + 1) * n + i, (j + 1) * n + i + 1)]
             for t in ((a, c, d), (a, d, b))]
    pins = [j * n + n // 2 for j in range(n)]
    r = 0.5 * np.abs(points[:, 0])
    toward = -points * [1, 1, 0]
    direction = toward / np.linalg.norm(toward, axis=1, keepdims=True)
    # The backstop is the cylinder: d is the gap to its surface, R its radius.
    gap = np.maximum(np.linalg.norm(toward, axis=1) - 0.1 - THICKNESS, 0.0)
    backstop = np.where((r > 0)[:, None], np.column_stack([gap, np.full(len(r), 0.1)]), 0.0)
    mesh = cook_mesh("sheet", points, faces, pins, wearable=True, tether_slack=1.12, max_distance=r, backstop=backstop)
    skin = dict(bone=np.zeros((len(points), 4), int), weight=np.tile([1.0, 0, 0, 0], (len(points), 1)))
    piece = dict(id="sheet", mesh=mesh, skin=skin, direction=direction, material=MATERIAL, render_vertices=len(points),
                 linear_inertia=0.45, angular_inertia=0.35, correctives=np.zeros((len(correctives), len(points), 6)))
    identity = np.eye(4)[:3].ravel()
    return encode("cylinder", [piece], bones=[("root", identity)],
                  spheres=[(0, (0, 0, -0.3), 0.1), (0, (0, 0, 0.3), 0.1)], cones=[(0, 1)], correctives=correctives)


def cylinder_body(pose):
    """The cylinder as a closed body surface on the one joint, posed: what the body direction rule reads."""
    angles = np.linspace(0, 2 * np.pi, 33)[:-1]
    zs = np.linspace(-0.4, 0.4, 9)
    ring = np.column_stack([0.1 * np.cos(angles), 0.1 * np.sin(angles)])
    points = np.array([(x, y, z) for z in zs for x, y in ring])
    normals = np.array([(x / 0.1, y / 0.1, 0.0) for z in zs for x, y in ring])
    n = len(angles)
    faces = [t for j in range(len(zs) - 1) for i in range(n)
             for a, b, c, d in [(j * n + i, j * n + (i + 1) % n, (j + 1) * n + i, (j + 1) * n + (i + 1) % n)]
             for t in ((a, b, d), (a, d, c))]
    m = pose[0]
    return Body(points @ m[:3, :3].T + m[:3, 3], faces, normals @ m[:3, :3].T, np.ones((len(points), 1)), ["root"],
                complete=True)


def turned(angle):
    """A one-joint pose turned about +Z, (1, 4, 4)."""
    c, s = np.cos(angle), np.sin(angle)
    m = np.eye(4)
    m[:2, :2] = [[c, -s], [s, c]]
    return m[None]


class TestHost(unittest.TestCase):
    def setUp(self):
        self.blob = sheet_over_cylinder()
        self.skeleton = Skeleton(["root"], [-1])

    def test_a_ramp_reaches_the_key_pose(self):
        chain = Skeleton(["a", "b", "c"], [-1, 0, 1])
        bone = np.eye(4)
        bone[1, 3] = 1.0
        rest_locals = [np.eye(4), bone, bone]

        def chained(locals_):
            out = [locals_[0]]
            for local in locals_[1:]:
                out.append(out[-1] @ local)
            return np.stack(out)
        start = chained(rest_locals)
        key = chained([local @ turned(0.4 * (j + 1))[0] for j, local in enumerate(rest_locals)])
        np.testing.assert_allclose(ramp(start, key, 0.0, chain), start, atol=1e-12)
        np.testing.assert_allclose(ramp(start, key, 1.0, chain), key, atol=1e-12)
        # Between them each bone keeps its length: the joints turn, they do not cut the chord.
        half = ramp(start, key, 0.5, chain)
        np.testing.assert_allclose(np.linalg.norm(half[1:, :3, 3] - half[:-1, :3, 3], axis=1), 1.0, atol=1e-12)
        # Stepped along the ramp, the host's reference ends on the key pose's reference.
        with Host(self.blob, self.skeleton, LIBRARY) as host:
            host.step(turned(0.0), 0.0, reset=True)
            for k in range(30):
                host.step(ramp(turned(0.0), turned(0.6), (k + 1) / 30, self.skeleton), 1 / 30)
            ramped = host.reference(0)
        with Host(self.blob, self.skeleton, LIBRARY) as host:
            host.step(turned(0.6), 0.0, reset=True)
            direct = host.reference(0)
        for a, b in zip(ramped, direct):
            np.testing.assert_allclose(a, b, atol=1e-6)

    def test_a_stored_corrective_is_the_reference_the_solver_sees(self):
        blob = sheet_over_cylinder(["lift"])
        piece = decode(blob)["pieces"][0]
        free = (piece["mesh"]["max_distance"] > 0)[:, None]
        delta = np.where(free, [0.0, 0.05, 0.0], 0.0)
        pose = turned(0.3)
        with Host(blob, self.skeleton, LIBRARY) as host:
            self.assertEqual(host.corrective_count, 1)
            host.set_corrective(0, 0, delta, np.zeros_like(delta))
            host.step(pose, 0.0, reset=True, weights=[1.0])
            x, b = host.reference(0)
            np.testing.assert_allclose(x, (piece["mesh"]["rest"] + delta) @ pose[0, :3, :3].T, atol=1e-6)
            np.testing.assert_allclose(b, piece["direction"] @ pose[0, :3, :3].T, atol=1e-6)
            # Lifted clear of the cylinder, the reset puts the cloth on that reference.
            np.testing.assert_allclose(host.positions(0), x, atol=1e-6)
            host.step(pose, 0.0, reset=True, weights=[0.0])
            np.testing.assert_allclose(host.reference(0)[0], piece["mesh"]["rest"] @ pose[0, :3, :3].T, atol=1e-6)

    def test_a_drape_stored_as_a_corrective_is_the_reference_at_its_key(self):
        blob = sheet_over_cylinder(["drape"])
        decoded = decode(blob)
        key = turned(0.5)
        with Host(blob, self.skeleton, LIBRARY) as host:
            drape = bake(host, turned(0.0), key, fixed=[[0, 1, 0, 0.3]], settle_s=1.0)
            shapes = deltas(decoded, key, self.skeleton, drape, [C.body_maps.directions(drape[0],
                            decoded["pieces"][0]["mesh"]["faces"], cylinder_body(key))])
            host.set_corrective(0, 0, *shapes[0])
            host.step(key, 0.0, reset=True, weights=[1.0])
            np.testing.assert_allclose(host.reference(0)[0], drape[0], atol=1e-5)
            np.testing.assert_allclose(host.positions(0), drape[0], atol=1e-5)

    def test_a_bake_repeats_until_its_drape_is_an_equilibrium(self):
        blob = sheet_over_cylinder(["drape"])
        key = turned(0.5)
        shapes, record = C.bake(blob, 0, self.skeleton, turned(0.0), key, [], cylinder_body(key), [THICKNESS],
                                floor=[[0, 1, 0, 0.3]], procedure=dict(settle_s=1.0), library=LIBRARY)
        self.assertTrue(record["converged"])
        self.assertLessEqual(record["drift_mm"]["sheet"], THICKNESS * 1000)
        self.assertEqual(record["passes"], len(record["drift_by_pass_mm"]))
        # Reset onto the stored corrective and held, the cloth stays within the fabric's thickness of it.
        with Host(blob, self.skeleton, LIBRARY) as host:
            host.set_corrective(0, 0, *shapes[0])
            host.step(key, 0.0, reset=True, convexes=[[[0, 1, 0, 0.3]]], weights=[1.0])
            for _ in range(60):
                host.step(key, 1 / 30, convexes=[[[0, 1, 0, 0.3]]], weights=[1.0])
            gap = np.linalg.norm(host.positions(0) - host.reference(0)[0], axis=1)
            self.assertLessEqual(float(np.percentile(gap, 99)), THICKNESS)

    def test_an_outfit_bakes_a_corrective_into_its_sidecar(self):
        import json
        import test_authoring as A
        from garment import sidecar as sc
        scene = A.Scene(A.cylinder_scene())
        try:
            (scene.root / "colliders.json").write_text(json.dumps(dict(
                schema="garment.colliders.v1", body_sha256="", cones=[[0, 1]],
                spheres=[dict(name="a", bone="root", center=[0, 0, 0], radius=0.1),
                         dict(name="b", bone="root", center=[0, 1, 0], radius=0.1)])), encoding="utf-8")
            o = Outfit(None, scene.root, "model.glb", body=["skin"], fabrics={"plain": A.FABRIC}, colliders="colliders.json")
            p = A.skirt(o)
            key = np.tile(np.eye(4), (3, 1, 1))
            key[1, 2, 3] = 0.06  # the joint the skirt's lower half follows steps forward into the body's side
            # The outfit bakes through the library GARMENT_HOST names, the one under test here.
            previous = os.environ.get("GARMENT_HOST")
            if LIBRARY:
                os.environ["GARMENT_HOST"] = str(LIBRARY)
            try:
                o.corrective("lean", pose=key, convexes=[])
                # Declared before the last edit: the build bakes on the final state, whatever the order.
                p.paint("r", p.f.all & ~p.pins, 0.05)
                data = sc.read(o.write().read_bytes())
            finally:
                if LIBRARY:
                    os.environ.pop("GARMENT_HOST") if previous is None else os.environ.__setitem__("GARMENT_HOST", previous)
            record, = data.correctives
            self.assertTrue(record["converged"])
            self.assertIn("colliders.json", o.inputs)
            self.assertEqual(data.corrective_names, ["lean"])
            self.assertTrue(np.all(data.piece("skirt").r[data.piece("skirt").r > 0] == np.float32(0.05)))
            delta, _ = data.piece("skirt").correctives["lean"]
            self.assertTrue(np.all(delta[data.piece("skirt").r == 0] == 0))
            self.assertGreater(float(np.abs(delta).max()), 0.01)
            from garment.cook import stale
            self.assertEqual([s for s in stale(data, scene.root, Path(__file__)) if s.startswith("corrective")], [])
            # A solver change makes every baked corrective stale, and stale() says so.
            moved = sc.read(sc.write(data))
            moved.correctives[0]["key"]["runtime_sha256"] = "0" * 64
            self.assertIn("corrective lean: the runtime sources changed since its bake",
                          stale(moved, scene.root, Path(__file__)))
            # Its recorded inputs all match: a second build keeps the committed deltas and needs no host at all.
            os.environ["GARMENT_HOST"] = str(scene.root / "no-such-host.dll")
            try:
                again = Outfit(None, scene.root, "model.glb", body=["skin"], fabrics={"plain": A.FABRIC},
                               colliders="colliders.json")
                q = A.skirt(again)
                q.paint("r", q.f.all & ~q.pins, 0.05)
                again.corrective("lean", pose=key, convexes=[])
                self.assertEqual(sc.write(again.built()), sc.write(data))
                # A changed input makes it stale: the build would bake again (here it cannot: no host).
                q.paint("r", q.f.all & ~q.pins, 0.06)
                with self.assertRaises((OSError, ValueError, FileNotFoundError)):
                    again.built()
            finally:
                os.environ.pop("GARMENT_HOST") if previous is None else os.environ.__setitem__("GARMENT_HOST", previous)
            # A bake that cannot converge in its passes stops the build.
            if LIBRARY:
                os.environ["GARMENT_HOST"] = str(LIBRARY)
            try:
                os.environ["GARMENT_REBAKE"] = "1"
                once = Outfit(None, scene.root, "model.glb", body=["skin"], fabrics={"plain": A.FABRIC},
                              colliders="colliders.json")
                A.skirt(once)
                once.corrective("lean", pose=key, convexes=[], max_passes=1)
                with self.assertRaisesRegex(ValueError, "did not converge"):
                    once.built()
            finally:
                os.environ.pop("GARMENT_REBAKE")
                if LIBRARY:
                    os.environ.pop("GARMENT_HOST") if previous is None else os.environ.__setitem__("GARMENT_HOST", previous)
        finally:
            scene.close()

    def test_a_skeleton_without_the_blob_joints_is_refused(self):
        with self.assertRaisesRegex(ValueError, "missing joints"):
            Host(self.blob, Skeleton(["other"], [-1]), LIBRARY)


if __name__ == "__main__":
    if sys.argv[1:2] == ["--emit"]:
        Path(sys.argv[2]).write_bytes(sheet_over_cylinder())
    else:
        LIBRARY = sys.argv.pop(1) if len(sys.argv) > 1 else None
        unittest.main()
