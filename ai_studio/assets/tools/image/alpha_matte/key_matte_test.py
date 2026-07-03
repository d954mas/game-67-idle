#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import unittest

import numpy as np
from PIL import Image

import ai_studio.assets.tools.image.alpha_matte.key_matte as key_matte
from ai_studio.assets.tools.image.alpha_matte.key_matte import key_matte_cutout


def make_ring_on_key(size: int = 160, key: tuple[int, int, int] = (0, 255, 0)):
    import numpy as np

    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    center = (size - 1) / 2.0
    radius = np.sqrt((xx - center) ** 2 + (yy - center) ** 2)
    outer, inner, aa = size * 0.42, size * 0.24, 1.6
    annulus = np.clip((outer - radius) / aa, 0, 1) * np.clip((radius - inner) / aa, 0, 1)
    foreground = np.asarray((150, 162, 178), dtype=np.float64)
    composite = np.empty((size, size, 3), dtype=np.float64)
    for channel in range(3):
        composite[..., channel] = foreground[channel] * annulus + key[channel] * (1.0 - annulus)
    image = np.zeros((size, size, 4), dtype=np.uint8)
    image[..., :3] = np.rint(composite).astype(np.uint8)
    image[..., 3] = 255
    truth_alpha = np.rint(annulus * 255.0).astype(np.uint8)
    return Image.fromarray(image, "RGBA"), truth_alpha, (int(round(center)), int(round(center)))


def make_dark_square_on_magenta(size: int = 96, aa: float = 2.5):
    """A dark navy rounded square on flat magenta with a soft anti-aliased edge —
    the halo failure case: blend pixels (dark art + bright key) pass the
    foreground tolerance and keep a purple rim unless edge colours are rebuilt."""
    import numpy as np

    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    center = (size - 1) / 2.0
    half = size * 0.30
    box = np.maximum(np.abs(xx - center), np.abs(yy - center))
    coverage = np.clip((half - box) / aa, 0, 1)
    key = (255, 0, 255)
    foreground = np.asarray((30, 40, 90), dtype=np.float64)
    composite = np.empty((size, size, 3), dtype=np.float64)
    for channel in range(3):
        composite[..., channel] = foreground[channel] * coverage + key[channel] * (1.0 - coverage)
    image = np.zeros((size, size, 4), dtype=np.uint8)
    image[..., :3] = np.rint(composite).astype(np.uint8)
    image[..., 3] = 255
    return Image.fromarray(image, "RGBA")


class KeyMatteTests(unittest.TestCase):
    def test_recovers_ring_with_transparent_hole(self) -> None:
        import numpy as np

        # Keep the unit fixture small. This behavior test should not become a
        # hidden performance gate for source-sheet scale workloads.
        size = 48
        crop, truth_alpha, center = make_ring_on_key(size)
        result = key_matte_cutout(crop, (0, 255, 0))
        alpha = np.asarray(result.getchannel("A"))

        # Interior hole must be transparent (the chroma-inside-art failure case).
        self.assertLess(int(alpha[center[1], center[0]]), 40)
        # A point on the annulus must stay opaque.
        ring_y = int(center[1] - size * 0.33)
        self.assertGreater(int(alpha[ring_y, center[0]]), 200)
        # Recovered alpha is close to the known truth.
        sad = float(np.mean(np.abs(alpha.astype(np.float64) - truth_alpha.astype(np.float64))))
        self.assertLess(sad, 10.0)
        # Edge colour is decontaminated: almost no strongly-green opaque pixels remain.
        array = np.asarray(result).astype(np.int16)
        visible = array[..., 3] > 40
        green_spill = visible & (array[..., 1] > array[..., 0] + 60) & (array[..., 1] > array[..., 2] + 60)
        self.assertLess(int(np.count_nonzero(green_spill)), 10)


    def test_no_key_halo_on_dark_art(self) -> None:
        import numpy as np

        # Dark navy on magenta: every anti-aliased blend pixel used to survive as
        # an opaque purple rim (min(r,b) far above g). After edge-colour
        # extension NO visible pixel may lean magenta — including over white.
        crop = make_dark_square_on_magenta()
        result = key_matte_cutout(crop, (255, 0, 255))
        array = np.asarray(result).astype(np.float64)
        alpha = array[..., 3] / 255.0
        over_white = array[..., :3] * alpha[..., None] + 255.0 * (1.0 - alpha[..., None])
        visible = alpha > 0.05
        purple = visible & (
            np.minimum(over_white[..., 0], over_white[..., 2]) > over_white[..., 1] + 15.0
        )
        self.assertEqual(int(np.count_nonzero(purple)), 0)
        # The subject interior keeps its own colour (extension must not flatten it).
        center = array.shape[0] // 2
        self.assertLess(float(np.abs(array[center, center, :3] - (30, 40, 90)).max()), 3.0)


def _image_digest(image: Image.Image) -> str:
    """sha256 over the raw RGBA bytes -- a byte-exact pin, cheaper than
    checking in binary PNG fixtures (this repo's Python tools build every
    other fixture procedurally too; see the two helpers above)."""
    array = np.asarray(image.convert("RGBA"), dtype=np.uint8)
    return hashlib.sha256(array.tobytes()).hexdigest()


class KeyMatteGoldenBytesTest(unittest.TestCase):
    """T0254 F1/F7/F10: pins key_matte_cutout's exact output bytes on three
    representative crops (opaque ring on both key colors, soft anti-aliased
    edge on magenta). Nothing upstream of this asserted transparent-pixel RGB,
    so a finalize-hygiene change (e.g. removing the bleed+repair passes the
    T0254 review found were fully overwritten by zero_fully_transparent_rgb)
    could previously change output silently. If this test goes red, the
    output changed and that needs a decision, not a silent edit."""

    GOLDEN = {
        "ring_on_green": "2d9f52e02a84e8ce74152fb3bc9574c03e5438ebfa6dc14ac65a563b0f5b04c3",
        "ring_on_magenta": "3af0cf8a00d0aaf45351441fa3050e5746b81ac58e782e3803a69e0696b7aa5d",
        "soft_square_on_magenta": "e51c1df36ceff757b0a90787d35660f9105a237383a03d3a93df641bc4cb5ca0",
    }

    def test_golden_bytes_ring_on_green(self) -> None:
        crop, _truth_alpha, _center = make_ring_on_key(64, key=(0, 255, 0))
        result = key_matte_cutout(crop, (0, 255, 0))
        self.assertEqual(_image_digest(result), self.GOLDEN["ring_on_green"])

    def test_golden_bytes_ring_on_magenta(self) -> None:
        crop, _truth_alpha, _center = make_ring_on_key(64, key=(255, 0, 255))
        result = key_matte_cutout(crop, (255, 0, 255))
        self.assertEqual(_image_digest(result), self.GOLDEN["ring_on_magenta"])

    def test_golden_bytes_soft_square_on_magenta(self) -> None:
        crop = make_dark_square_on_magenta(64)
        result = key_matte_cutout(crop, (255, 0, 255))
        self.assertEqual(_image_digest(result), self.GOLDEN["soft_square_on_magenta"])


if __name__ == "__main__":
    unittest.main()
