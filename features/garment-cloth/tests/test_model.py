"""The cook reads the author contract from a GLB and refuses a model that breaks it."""
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from garment_binding import unpack_vertex_binding_u16x4
from garment_blob import decode
from garment_cook import bind_outfit, cook_outfit, depfile_text, stage_model
from garment_glb import Glb
from garment_paint import baseline_freedom
from synthetic import glb, grid, legs, one_joint

JOINTS = ["Root", "Left", "Right", "Ribbon"]
BODY = set(JOINTS[:3])
LABEL = 7  # any unique positive integer names a piece


def model(label=None, freedom=None, weights=None, cloth_joint=None):
    """Legs on Root/Left/Right and a panel in front of them labelled LABEL; a skin flap below stays unlabelled."""
    body_points, body_faces, body_weights = legs()
    panel, panel_faces = grid(-0.16, 0.16, 0.2, 0.6, 0.07, 9, 7)
    flap, flap_faces = grid(-0.16, 0.16, 0.6, 0.66, 0.07, 9, 2)
    # The flap's bottom row is the panel's top row: the panel is sewn to it there.
    points = np.concatenate([panel, flap[9:]])
    faces = np.concatenate([panel_faces, np.where(flap_faces < 9, flap_faces + 54, flap_faces - 9 + 63)])
    labels = np.zeros(len(points))
    labels[:63] = LABEL
    # The panel's own skin weights: the left leg's share grows from the right leg to the left one.
    left = np.clip((panel[:, 0] + 0.1) / 0.2, 0, 1)
    jids = np.zeros((len(points), 4), int)
    vals = np.zeros((len(points), 4))
    jids[:63, :2], vals[:63, :2] = [1, 2], np.column_stack([left, 1 - left])
    jids[63:, 0], vals[63:, 0] = 0, 1
    if cloth_joint is not None:
        jids[:63], vals[:63] = one_joint(63, cloth_joint)
    if weights is not None:
        jids, vals = weights
    paint = np.zeros(len(points))
    paint[:63] = baseline_freedom(panel, panel_faces, 0.2, (0.8, 0.3, 1.0))
    bj, bw = np.zeros((len(body_points), 4), int), np.zeros((len(body_points), 4))
    bj[:, 0] = np.argmax(body_weights, 1) + 1
    bw[:, 0] = 1
    data = glb([("skin", body_points, body_faces, bj, bw, {}),
                ("cloth", points, faces, jids, vals,
                 {"_CLOTH_PIECE": labels if label is None else label, "_CLOTH": paint if freedom is None else freedom})],
               JOINTS, parents=[None, 0, 0, 0])
    return Glb(data), labels, paint


def piece(**changes):
    base = dict(id="panel", piece=LABEL, source_material="cloth", outer_surface=None, budget=40, drive_hz=2.0, swing=0.5, backstop_per_m=0.1,
                material=dict(density=0.2, iterations=1, thickness=0.004),
                colliders=[("Left", [0.1, 0.0, 0.0], "Left", [0.1, 0.8, 0.0], 0.06, 0.0, 0)],
                linear_inertia=0.45, angular_inertia=0.35, downward_follow=0.85)
    return base | changes


class ModelCook(unittest.TestCase):
    def test_contract_decodes_into_pins_drive_travel_and_a_closed_seam(self):
        m, labels, paint = model()
        blob, sheets, report = cook_outfit("synthetic", m, [piece()], BODY)
        cooked = decode(blob)["pieces"][0]
        pins = cooked["mesh"]["inv_area"] == 0
        drive, travel = cooked["mesh"]["drive"], cooked["vertices"]["travel_m"]
        self.assertTrue(pins.any() and (~pins).any())
        self.assertTrue(np.all(drive[pins] == 0) and np.all(drive[~pins] > 0))
        self.assertTrue(np.all(drive <= (2 * np.pi * 2.0) ** 2 * (1 + 1e-6)))
        self.assertTrue(np.all(travel[pins] == 0) and np.all(travel[~pins] > 0))
        self.assertEqual(report[0]["render_vertices"], 63)
        names = {name for name, _ in cooked["bones"]}
        self.assertTrue(names <= BODY)
        streams = bind_outfit(m, sheets, BODY)
        cloth = streams[1]
        self.assertIsNone(streams[0])
        # Render pins are skinned, so the border with the unlabelled flap is closed by construction.
        self.assertTrue(np.all(cloth[paint == 0] == 0))
        bound = [unpack_vertex_binding_u16x4(row)[0] for row in cloth[paint > 0]]
        self.assertGreater(np.mean(bound), 0.5)
        staged = Glb(stage_model(m, streams))
        self.assertIn("_GARMENT", staged.primitives()[1]["attributes"])

    def test_piece_numbers_follow_the_label_not_the_recipe_order(self):
        m, _, _ = model()
        for number in (0, -3, 7.0, True):
            with self.subTest(number), self.assertRaisesRegex(ValueError, "unique positive integers"):
                cook_outfit("synthetic", m, [piece(piece=number)], BODY)
        with self.assertRaisesRegex(ValueError, r"labels without a recipe piece \[7\], recipe pieces without vertices \[8\]"):
            cook_outfit("synthetic", m, [piece(piece=8)], BODY)

    def test_a_lost_label_or_a_label_without_a_piece_is_refused(self):
        _, labels, paint = model()
        # A re-export that drops both attributes leaves the recipe piece without vertices.
        with self.assertRaisesRegex(ValueError, r"recipe pieces without vertices \[7\]"):
            cook_outfit("synthetic", model(label=0 * labels, freedom=0 * paint)[0], [piece()], BODY)
        # Labels lost on part of a piece leave freedom on unlabelled vertices.
        partial = labels.copy()
        partial[:20] = 0
        with self.assertRaisesRegex(ValueError, "_CLOTH is set on a vertex without a piece label"):
            cook_outfit("synthetic", model(label=partial)[0], [piece()], BODY)
        extra = labels.copy()
        extra[63:] = 9  # the pinned flap becomes a piece no recipe names
        with self.assertRaisesRegex(ValueError, r"labels without a recipe piece \[9\]"):
            cook_outfit("synthetic", model(label=extra)[0], [piece()], BODY)

    def test_a_non_integer_label_is_refused(self):
        _, labels, _ = model()
        labels[3] = 7.5
        with self.assertRaisesRegex(ValueError, "_CLOTH_PIECE must hold non-negative integers"):
            cook_outfit("synthetic", model(label=labels)[0], [piece()], BODY)

    def test_freedom_off_the_piece_or_on_its_border_is_refused(self):
        _, labels, paint = model()
        outside = paint.copy()
        outside[-1] = 0.5
        with self.assertRaisesRegex(ValueError, "_CLOTH is set on a vertex without a piece label"):
            cook_outfit("synthetic", model(freedom=outside)[0], [piece()], BODY)
        border = paint.copy()
        border[56] = 0.5  # on the row sewn to the flap
        with self.assertRaisesRegex(ValueError, "a piece border must be pinned"):
            cook_outfit("synthetic", model(freedom=border)[0], [piece()], BODY)

    def test_weights_outside_the_body_rig_are_refused(self):
        with self.assertRaisesRegex(ValueError, r"joints outside the body rig: \['Ribbon'\]"):
            cook_outfit("synthetic", model(cloth_joint=3)[0], [piece()], BODY)

    def test_depfile_escapes_make_metacharacters(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            text = depfile_text(root / "out file.json", [root / "a$b.glb", root / "c#d.json"])
        self.assertTrue(text.endswith("\n") and text.count(": ") == 1)
        self.assertIn("out\\ file.json: ", text)
        self.assertIn("a$$b.glb", text)
        self.assertIn("c\\#d.json", text)


if __name__ == "__main__":
    unittest.main()
