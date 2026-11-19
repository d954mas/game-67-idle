import unittest

import numpy as np

from ai_studio.assets.tools.blender import contact_check
from ai_studio.assets.tools.blender.lib import glb as G

UNIT = np.array([[x, y, z] for x in (0, 1) for y in (0, 1) for z in (0, 1)], float)
FACES = np.array([[1, 2, 0], [3, 2, 1], [6, 5, 4], [6, 7, 5], [4, 1, 0], [4, 5, 1],
                  [3, 6, 2], [7, 6, 3], [2, 4, 0], [6, 4, 2], [5, 3, 1], [5, 7, 3]])


def box(lo, hi):
    return np.asarray(lo, float) + UNIT * (np.asarray(hi, float) - lo)


def scene(**boxes):
    b = G.GlbBuilder()
    for name, (lo, hi) in boxes.items():
        b.add_node(name, mesh=b.add_mesh(name, box(lo, hi), FACES))
    return b.to_glb()


def found(report, severity=None):
    return {f["check"] for f in report["findings"] if severity is None or f["severity"] == severity}


SLAB = ((-1, -0.2, -1), (1, 0, 1))
PAIR = {"pairs": [{"moving": "Crate", "receiver": "Slab", "direction": [0, -1, 0], "window": [-0.08, 0.001]}]}


class Contract(unittest.TestCase):
    def test_resting_part_passes_and_controls_fail(self):
        report = contact_check.check(scene(Slab=SLAB, Crate=((-0.2, 0, -0.2), (0.2, 0.4, 0.2))), contract=PAIR)
        self.assertEqual(found(report), set())
        row = report["metrics"]["contacts"][0]
        self.assertAlmostEqual(row["signed_gap"], 0.0, places=6)
        self.assertTrue(all(row["controls"].values()))

    def test_floating_and_sunk_parts(self):
        lifted = contact_check.check(scene(Slab=SLAB, Crate=((-0.2, 0.15, -0.2), (0.2, 0.5, 0.2))), contract=PAIR)
        self.assertIn("contact", found(lifted, "high"))
        self.assertAlmostEqual(lifted["metrics"]["contacts"][0]["signed_gap"], 0.15, places=5)
        sunk = contact_check.check(scene(Slab=SLAB, Crate=((-0.2, -0.15, -0.2), (0.2, 0.3, 0.2))), contract=PAIR)
        self.assertAlmostEqual(sunk["metrics"]["contacts"][0]["signed_gap"], -0.15, places=5)
        self.assertIn("contact", found(sunk, "high"))

    def test_window_too_wide_is_reported_as_testing_nothing(self):
        wide = {"pairs": [dict(PAIR["pairs"][0], window=[-0.08, 1e6])]}
        report = contact_check.check(scene(Slab=SLAB, Crate=((-0.2, 0, -0.2), (0.2, 0.4, 0.2))), contract=wide)
        self.assertIn("negative_control", found(report, "medium"))

    def test_ground_and_missing_nodes(self):
        contract = {"ground": [{"node": "Crate", "window": [-0.01, 0.001]}], "floor_y": 0,
                    "pairs": [{"moving": "Ghost", "receiver": "Slab"}]}
        report = contact_check.check(scene(Slab=SLAB, Crate=((-0.2, 0.05, -0.2), (0.2, 0.4, 0.2))), contract=contract)
        messages = " ".join(f["message"] for f in report["findings"])
        self.assertIn("Crate -> floor: hangs", messages)
        self.assertIn("Ghost", messages)


class Floating(unittest.TestCase):
    def test_stack_on_floor_is_held(self):
        report = contact_check.check(scene(Table=((-0.5, 0, -0.5), (0.5, 0.7, 0.5)),
                                           Cup=((-0.05, 0.7, -0.05), (0.05, 0.8, 0.05))), floating=True)
        self.assertEqual(found(report), set())

    def test_floating_box_is_found(self):
        report = contact_check.check(scene(Table=((-0.5, 0, -0.5), (0.5, 0.7, 0.5)),
                                           Cup=((-0.05, 0.85, -0.05), (0.05, 0.95, 0.05))), floating=True)
        hits = [f for f in report["findings"] if f["check"] == "floating"]
        self.assertEqual([f["where"]["part"] for f in hits], ["Cup"])
        self.assertAlmostEqual(hits[0]["value"], 0.85, places=4)
        rows = {r["part"]: r for r in report["metrics"]["floating"]}
        self.assertAlmostEqual(rows["Cup"]["gap_below"], 0.15, places=4)

    def test_wall_mount_and_embedded_parts_are_held(self):
        report = contact_check.check(scene(Wall=((0, 0, 0), (0.1, 3, 2)),
                                           Shelf=((0.1, 1.5, 0.5), (0.4, 1.55, 1.5)),
                                           Peg=((0.02, 1.0, 0.9), (0.3, 1.05, 1.0))), floating=True)
        self.assertEqual(found(report), set())

    def test_split_components_finds_floating_shell_inside_one_mesh(self):
        b = G.GlbBuilder()
        points = np.vstack([box((-0.5, 0, -0.5), (0.5, 0.7, 0.5)), box((-0.05, 0.9, -0.05), (0.05, 1.0, 0.05))])
        b.add_node("merged", mesh=b.add_mesh("merged", points, np.vstack([FACES, FACES + 8])))
        self.assertEqual(found(contact_check.check(b.to_glb(), floating=True)), set())
        self.assertIn("floating", found(contact_check.check(b.to_glb(), floating=True, split_components=True)))


class FloatingAssemblies(unittest.TestCase):
    def test_table_hovering_with_legs_sunk_into_its_top(self):
        legs = {f"Leg{k}": ((x - 0.04, 1.0, z - 0.04), (x + 0.04, 1.72, z + 0.04))
                for k, (x, z) in enumerate([(-0.4, -0.4), (0.4, -0.4), (-0.4, 0.4), (0.4, 0.4)])}
        report = contact_check.check(scene(Top=((-0.5, 1.7, -0.5), (0.5, 1.8, 0.5)), **legs), floating=True)
        flagged = {f["where"]["part"] for f in report["findings"] if f["check"] == "floating"}
        self.assertEqual(flagged, {"Top", *legs})
        grounded = {k: ((lo[0], lo[1] - 1.0, lo[2]), (hi[0], hi[1] - 1.0, hi[2])) for k, (lo, hi) in legs.items()}
        report = contact_check.check(scene(Top=((-0.5, 0.7, -0.5), (0.5, 0.8, 0.5)), **grounded), floating=True)
        self.assertEqual(found(report), set())

    def test_malformed_node_is_a_finding(self):
        glb = scene(Slab=SLAB)
        glb.json["nodes"][0]["mesh"] = 5
        self.assertIn("check_error", found(contact_check.check(glb, floating=True), "high"))


if __name__ == "__main__":
    unittest.main()
