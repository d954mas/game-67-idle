from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import body_peel


def _fixture(size: tuple[int, int] = (12, 10)) -> tuple[Image.Image, Image.Image, Image.Image]:
    w, h = size
    nude = np.full((h, w, 4), (224, 171, 145, 255), dtype=np.uint8)
    dressed = nude.copy()
    dressed[2:8, 3:9, :3] = (20, 80, 220)
    semantic = np.zeros((h, w), dtype=np.uint8)
    semantic[2:8, 3:9] = 255
    return (
        Image.fromarray(nude, "RGBA"),
        Image.fromarray(dressed, "RGBA"),
        Image.fromarray(semantic, "L"),
    )


class BodyPeelV3Test(unittest.TestCase):
    def test_visible_garment_rgb_is_copied_from_dressed_and_body_stays_transparent(self) -> None:
        nude, dressed, semantic = _fixture()

        layer, plate, report = body_peel.peel(
            np.asarray(nude),
            np.asarray(dressed),
            np.asarray(semantic),
            slot="main",
            seed_threshold=40,
            support_radius=1,
            min_support_ratio=0.85,
        )

        layer_pixels = np.asarray(layer)
        dressed_pixels = np.asarray(dressed)
        semantic_pixels = np.asarray(semantic) > 0
        self.assertTrue(np.array_equal(layer_pixels[semantic_pixels, :3], dressed_pixels[semantic_pixels, :3]))
        self.assertTrue(np.all(layer_pixels[~semantic_pixels, 3] == 0))
        self.assertEqual(report["verdict"], "ok")
        self.assertTrue(report["rgb_preserved"])

        plate_pixels = np.asarray(plate)
        self.assertTrue(np.all(plate_pixels[~semantic_pixels] == (255, 0, 255, 255)))
        self.assertTrue(np.array_equal(plate_pixels[semantic_pixels, :3], dressed_pixels[semantic_pixels, :3]))

    def test_dimension_mismatch_is_rejected_instead_of_resized(self) -> None:
        nude, dressed, semantic = _fixture()
        smaller = np.asarray(dressed.resize((8, 8)))

        with self.assertRaisesRegex(ValueError, "dimension mismatch"):
            body_peel.peel(
                np.asarray(nude),
                smaller,
                np.asarray(semantic),
                slot="main",
            )

    def test_jpeg_input_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            jpeg = Path(td) / "worn.jpg"
            Image.new("RGB", (8, 8), "blue").save(jpeg)

            with self.assertRaisesRegex(ValueError, "PNG only"):
                body_peel.load_png(jpeg, role="dressed")

    def test_process_files_writes_layer_plate_report_and_proofs(self) -> None:
        nude, dressed, semantic = _fixture()
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            nude_path = root / "nude.png"
            dressed_path = root / "worn.png"
            semantic_path = root / "mask.png"
            out_path = root / "garment.png"
            plate_path = root / "garment_plate.png"
            report_path = root / "report.json"
            proof_dir = root / "proof"
            nude.save(nude_path)
            dressed.save(dressed_path)
            semantic.save(semantic_path)

            report = body_peel.process_files(
                nude_path=nude_path,
                dressed_path=dressed_path,
                semantic_mask_path=semantic_path,
                out_path=out_path,
                out_plate_path=plate_path,
                report_path=report_path,
                proof_dir=proof_dir,
                slot="main",
                mask_channel="luminance",
                seed_threshold=40,
                support_radius=1,
                min_support_ratio=0.85,
            )

            self.assertEqual(report["verdict"], "ok")
            self.assertTrue(out_path.is_file())
            self.assertTrue(plate_path.is_file())
            self.assertEqual(json.loads(report_path.read_text(encoding="utf-8"))["slot"], "main")
            self.assertTrue((proof_dir / "checkerboard.png").is_file())
            self.assertTrue((proof_dir / "on_body.png").is_file())
            self.assertTrue((proof_dir / "on_white.png").is_file())
            self.assertTrue((proof_dir / "on_dark.png").is_file())

    def test_opaque_worn_background_against_transparent_nude_is_rejected(self) -> None:
        nude = np.zeros((10, 12, 4), dtype=np.uint8)
        nude[2:8, 3:9] = (224, 171, 145, 255)
        dressed = np.full((10, 12, 4), (238, 238, 238, 255), dtype=np.uint8)
        dressed[2:8, 3:9] = (20, 80, 220, 255)
        semantic = np.zeros((10, 12), dtype=np.uint8)
        semantic[2:8, 3:9] = 255

        with self.assertRaisesRegex(ValueError, "alpha/background mismatch"):
            body_peel.peel(nude, dressed, semantic, slot="main", seed_threshold=40)

    def test_exposed_body_drift_outside_semantic_mask_is_rejected(self) -> None:
        nude, dressed, semantic = _fixture()
        drifted = np.asarray(dressed).copy()
        drifted[:, :3, :3] = (90, 40, 180)

        with self.assertRaisesRegex(ValueError, "exposed-body drift"):
            body_peel.peel(
                np.asarray(nude),
                drifted,
                np.asarray(semantic),
                slot="main",
                seed_threshold=40,
                max_exposed_drift_ratio=0.05,
            )


if __name__ == "__main__":
    unittest.main()
