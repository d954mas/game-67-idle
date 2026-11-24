"""The offline host on a synthetic sheet over a cylinder.

`--emit <blob>` writes the outfit test_host.c steps through the host and through the solver directly. With the
garment_host library as the argument it runs the Python side: a ramp reaches its key pose, and an injected reference
is the reference the solver sees."""
import sys
import unittest
from pathlib import Path

import numpy as np

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
from garment_blob import decode, encode  # noqa: E402
from garment.cook import cook_mesh  # noqa: E402
from garment.host import Host, Skeleton, ramp  # noqa: E402

LIBRARY = None
THICKNESS = 0.004
MATERIAL = dict(density=0.33, stretch=8e-6, bend=450.0, drag=0.4, thickness=THICKNESS, iterations=1, max_edge_strain=0.12,
                strain_passes=2, aero_drag=0.7, weft_stretch_scale=2.2, diag_stretch_scale=2.8, weft_bend_scale=1.3,
                weft_max_strain=0.18, friction=0.4, compression_limit=0.25)


def sheet_over_cylinder():
    """A 0.4 m sheet 6 mm above a cylinder of radius 0.1 along Z, pinned along the cylinder's top line, on one joint."""
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
                 linear_inertia=0.45, angular_inertia=0.35)
    identity = np.eye(4)[:3].ravel()
    return encode("cylinder", [piece], bones=[("root", identity)],
                  spheres=[(0, (0, 0, -0.3), 0.1), (0, (0, 0, 0.3), 0.1)], cones=[(0, 1)])


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

    def test_an_injected_reference_is_the_reference_the_solver_sees(self):
        piece = decode(self.blob)["pieces"][0]
        rest = piece["mesh"]["rest"] + [0.0, 0.05, 0.0]
        direction = piece["direction"] * 3.0
        pose = turned(0.3)
        with Host(self.blob, self.skeleton, LIBRARY) as host:
            host.override(0, rest, direction)
            host.step(pose, 0.0, reset=True)
            x, b = host.reference(0)
            np.testing.assert_allclose(x, rest @ pose[0, :3, :3].T + pose[0, :3, 3], atol=1e-6)
            np.testing.assert_allclose(b, piece["direction"] @ pose[0, :3, :3].T, atol=1e-6)
            # Lifted clear of the cylinder, the reset puts the cloth on that reference.
            np.testing.assert_allclose(host.positions(0), x, atol=1e-6)
            host.override(0)
            host.step(pose, 0.0, reset=True)
            np.testing.assert_allclose(host.reference(0)[0], piece["mesh"]["rest"] @ pose[0, :3, :3].T, atol=1e-6)

    def test_a_skeleton_without_the_blob_joints_is_refused(self):
        with self.assertRaisesRegex(ValueError, "missing joints"):
            Host(self.blob, Skeleton(["other"], [-1]), LIBRARY)


if __name__ == "__main__":
    if sys.argv[1:2] == ["--emit"]:
        Path(sys.argv[2]).write_bytes(sheet_over_cylinder())
    else:
        LIBRARY = sys.argv.pop(1) if len(sys.argv) > 1 else None
        unittest.main()
