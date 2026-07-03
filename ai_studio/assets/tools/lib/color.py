from __future__ import annotations

import argparse
from typing import Any

import numpy as np
from PIL import Image

RGB = tuple[int, int, int]

# T0254 (python-tools review, F3): color-distance-to-key was reimplemented >=5x
# with a metric SPLIT -- most call sites used Chebyshev (max |delta channel|)
# but route_cutout used Euclidean. Chebyshev is the majority AND the keyer's
# own metric (key_matte.py, chroma_key_alpha.py), so it is the one convention
# every call site converges on here.


def key_distance(pixels: Any, key: RGB | Any) -> Any:
    """Chebyshev (max absolute per-channel delta) color distance from every
    pixel to ``key``. Vectorized over numpy arrays whose last axis holds RGB(A)
    channels -- only the first 3 channels are compared, so RGBA arrays can be
    passed directly. ``pixels`` and ``key`` must already be in the same numeric
    scale (both 0-255, or both caller-normalized to 0-1); this function does
    not rescale either input."""
    key_array = np.asarray(key, dtype=np.float64)
    rgb = np.asarray(pixels, dtype=np.float64)[..., :3]
    return np.max(np.abs(rgb - key_array), axis=-1)


def parse_hex(value: str) -> RGB:
    """Parse a ``#rrggbb`` (or ``rrggbb``) string into an (r, g, b) int tuple.
    Raises ``argparse.ArgumentTypeError`` on malformed input so it can be used
    directly as an ``argparse`` ``type=`` callback (its former call sites all
    did this)."""
    text = value.strip().lstrip("#")
    if len(text) != 6:
        raise argparse.ArgumentTypeError("color must be #rrggbb")
    try:
        return tuple(int(text[index : index + 2], 16) for index in (0, 2, 4))  # type: ignore[return-value]
    except ValueError as exc:
        raise argparse.ArgumentTypeError("color must be #rrggbb") from exc


def format_hex(value: RGB) -> str:
    """Format an (r, g, b) int tuple as a ``#rrggbb`` string."""
    return "#{:02x}{:02x}{:02x}".format(*value)


def estimate_border_key(
    image: Image.Image,
    *,
    alpha_threshold: int = 0,
    fallback: RGB = (255, 0, 255),
) -> RGB:
    """Most-common (mode) opaque border color.

    T0254 (F4): border-key estimation was duplicated 5x with DIVERGENT
    semantics -- some call sites used mode, others median, so the same sheet
    could resolve to different keys depending on which copy ran. This is the
    one convention every in-scope call site converges on: the mode of the
    border ring's opaque pixels, matching ``normalize_background.py`` (the
    tool that actually burns the estimated key into production sheets, i.e.
    the "production keyer" for this decision) rather than the median used by
    ``route_cutout.py``'s prior inline estimator. Mode is also more robust to
    a border that's 99% flat key + a few stray anti-aliased/art-bleed pixels:
    the exact key color still wins outright, where a median could drift.
    """
    array = np.asarray(image.convert("RGBA"), dtype=np.uint8)
    border = np.concatenate([array[0, :, :], array[-1, :, :], array[:, 0, :], array[:, -1, :]], axis=0)
    opaque = border[border[:, 3] > alpha_threshold]
    if opaque.size == 0:
        return fallback
    rgb = opaque[:, :3]
    colors, counts = np.unique(rgb, axis=0, return_counts=True)
    key = colors[int(np.argmax(counts))]
    return int(key[0]), int(key[1]), int(key[2])


def split_alpha(image: Image.Image) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Convert ``image`` to RGBA once and split it into ``(rgba, rgb, alpha)``
    views, where ``rgb`` and ``alpha`` are views into ``rgba`` (no copy). Shared
    by tools (quantize, denoise) that process RGB only and must leave alpha
    byte-identical."""
    rgba = np.asarray(image.convert("RGBA"))
    return rgba, rgba[..., :3], rgba[..., 3]


def merge_alpha(rgb: np.ndarray, alpha: np.ndarray) -> Image.Image:
    """Recombine a processed RGB array with an (untouched) alpha array back
    into an RGBA image. Counterpart to ``split_alpha``."""
    out = np.empty((*rgb.shape[:2], 4), dtype=rgb.dtype)
    out[..., :3] = rgb
    out[..., 3] = alpha
    return Image.fromarray(out, "RGBA")
