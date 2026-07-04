#!/usr/bin/env python3
"""Shared canonical math for the non-destructive image filters (T0273: brightness/
saturation/contrast/tint) — the ONE Python implementation both `render_group.py` (live
render/export, filters still non-destructive on the element) and `bake_filters.py`
(T0274 "Apply" — burns the CURRENT filters + opacity into a NEW source file) call, so
there is exactly one formula, never two copies that could drift. See the canvas
README's "Image filters" section for the full contract (both renderers — this PIL side
and the browser's own CSS-filter approximation — implement the SAME per-pixel math).

`apply_filters` was originally inline in `render_group.py`; extracted here (T0274) with
its exact body — render_group.py's own tests/behavior are unchanged, just imported
from one level over instead of defined locally.
"""
from __future__ import annotations

from typing import Any

import numpy as np
from PIL import Image


def apply_filters(image: Image.Image, filters: dict[str, Any] | None) -> Image.Image:
    """Apply the canonical non-destructive filter chain — brightness -> saturate ->
    contrast -> tint — to an RGBA image's COLOR channels; alpha is never touched (a
    fully transparent pixel stays fully transparent, a semi-transparent one keeps its
    alpha exactly). This is the ONE canonical math both renderers implement identically
    (see README "Image filters"): PIL here is the source of rendered truth; the canvas's
    own paintElement approximates the SAME formulas via the browser's spec'd CSS
    `filter: brightness() saturate() contrast()` plus an offscreen source-atop tint scrim.

    Order (matches `site/workspace.js` byte-for-byte, same non-premultiplied sRGB [0,1]
    channel math):
      1. brightness: C' = clamp(C * b)
      2. saturate (SVG matrix, luma 0.2126/0.7152/0.0722 — NOT PIL's default 0.299/0.587/
         0.114 grayscale luma; saturation=0 must land on the FORMER, the whole reason this
         is hand-rolled in numpy instead of `ImageEnhance.Color`)
      3. contrast: C' = clamp((C - 0.5) * c + 0.5)
      4. tint: linear RGB mix toward `tint.color` by `tint.strength`, alpha untouched

    No-op (returns `image` unchanged, no numpy work) when `filters` is empty/absent or
    every value is already at its default — an unfiltered element pays zero extra cost,
    the pre-existing fast path."""
    if not filters:
        return image
    brightness = float(filters.get("brightness", 1))
    saturation = float(filters.get("saturation", 1))
    contrast = float(filters.get("contrast", 1))
    tint = filters.get("tint")
    tint_strength = float(tint.get("strength", 0)) if tint else 0.0
    if brightness == 1 and saturation == 1 and contrast == 1 and tint_strength <= 0:
        return image

    src = np.asarray(image, dtype=np.uint8)
    alpha = src[..., 3]  # untouched, reassembled verbatim at the end
    rgb = src[..., :3].astype(np.float32) / 255.0

    if brightness != 1:
        rgb = rgb * brightness

    if saturation != 1:
        s = saturation
        r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
        rgb = np.stack(
            [
                (0.2126 + 0.7874 * s) * r + (0.7152 - 0.7152 * s) * g + (0.0722 - 0.0722 * s) * b,
                (0.2126 - 0.2126 * s) * r + (0.7152 + 0.2848 * s) * g + (0.0722 - 0.0722 * s) * b,
                (0.2126 - 0.2126 * s) * r + (0.7152 - 0.7152 * s) * g + (0.0722 + 0.9278 * s) * b,
            ],
            axis=-1,
        )

    if contrast != 1:
        rgb = (rgb - 0.5) * contrast + 0.5

    rgb = np.clip(rgb, 0.0, 1.0)

    if tint_strength > 0:
        hexs = str(tint["color"]).lstrip("#")
        tint_rgb = np.array([int(hexs[i : i + 2], 16) / 255.0 for i in (0, 2, 4)], dtype=np.float32)
        rgb = rgb * (1 - tint_strength) + tint_rgb * tint_strength
        rgb = np.clip(rgb, 0.0, 1.0)

    out_rgb = np.round(rgb * 255.0).astype(np.uint8)
    out = np.dstack([out_rgb, alpha])
    return Image.fromarray(out, mode="RGBA")
