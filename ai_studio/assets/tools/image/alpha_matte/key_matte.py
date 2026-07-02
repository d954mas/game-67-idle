#!/usr/bin/env python3
"""Deterministic single-background cutout: known-key trimap -> bounded alpha
band -> edge-colour decontamination.

This is path 1 for opaque art and flat-key holes. Instead of importing a
JIT-backed global matte solver in the default gate, it:

1. builds a trimap from the KNOWN key colour distance
   (sure-bg = flat exact key anywhere, incl. interior holes;
    sure-fg = far from key; unknown = the thin band between),
2. assigns a bounded smooth alpha across the unknown key-distance band, and
3. decontaminates edge colour with deterministic image hygiene.

Use it for OPAQUE art and flat-key holes (rings, gaps, crisp silhouettes). Soft
fractional alpha (soft shadow, glow, glass, smoke) is mathematically
unrecoverable from one background -- route those to dual-plate
(``dual_plate_alpha.py``).

Requires numpy. There is no dependency on JIT-backed matting packages in the
default validation path.
A crop with no soft edge band (opaque art / flat-key holes) skips alpha-band
work entirely.
"""
from __future__ import annotations

from time import perf_counter

import numpy as np
from PIL import Image

from ai_studio.assets.tools.image.alpha_matte.chroma_key_alpha import (
    bleed_transparent_rgb,
    decontaminate_source_key_spill_image,
    repair_transparent_edge_rgb,
    source_key_spill_mask,
    zero_fully_transparent_rgb,
)

RGB = tuple[int, int, int]


def _limit_despill(rgb_float: "np.ndarray", key: RGB) -> "np.ndarray":
    """Vlahos 'limit' despill keyed on the actual key colour: the key's own
    channel(s) may not exceed a neutral level set by the other channels, so the
    key colour cannot dominate the recovered foreground. It is per-pixel and
    sharp, so it removes the key halo (e.g. green inside a ring hole) WITHOUT
    blurring detail, and it leaves non-key colours untouched (a grey/brown pixel
    whose green is already below max(r,b) is unchanged)."""
    red, green, blue = rgb_float[..., 0], rgb_float[..., 1], rgb_float[..., 2]
    key_red, key_green, key_blue = key
    bright, dim = 150, 120
    out = rgb_float.copy()
    if key_green > bright and key_red < dim and key_blue < dim:            # green key
        # Only touch pixels where green is the dominant channel (true spill); pull
        # those to the average of the other channels so dark olive residue goes
        # neutral, while leaving green-below-max content (gold, brown) untouched.
        dominant = green > np.maximum(red, blue)
        out[..., 1] = np.where(dominant, np.minimum(green, (red + blue) * 0.5), green)
    elif key_red > bright and key_blue > bright and key_green < dim:       # magenta key
        magenta = np.minimum(red, blue) > green
        neutral = (green + np.minimum(red, blue)) * 0.5
        out[..., 0] = np.where(magenta, np.minimum(red, np.maximum(green, neutral)), red)
        out[..., 2] = np.where(magenta, np.minimum(blue, np.maximum(green, neutral)), blue)
    elif key_red > bright and key_green < dim and key_blue < dim:          # red key
        out[..., 0] = np.minimum(red, np.maximum(green, blue))
    elif key_blue > bright and key_red < dim and key_green < dim:          # blue key
        out[..., 2] = np.minimum(blue, np.maximum(red, green))
    elif key_green > bright and key_blue > bright and key_red < dim:       # cyan key
        out[..., 1] = np.minimum(green, np.maximum(red, blue))
        out[..., 2] = np.minimum(blue, np.maximum(red, green))
    return out


def key_matte_cutout(
    image: Image.Image,
    key: RGB,
    *,
    exact_tolerance: int = 12,
    foreground_tolerance: int = 80,
    max_dim: int = 512,
    timings: dict | None = None,
) -> Image.Image:
    """Return an RGBA cutout of ``image`` against the flat ``key`` colour using a
    bounded key-distance alpha band. Intended to run PER CROP (small image);
    run routing/audits on full source sheets before extracting crops. Pass
    ``timings`` (a dict) to record per-step milliseconds for profiling."""
    def _mark(name: str, start: float) -> float:
        if timings is not None:
            timings[name] = round((perf_counter() - start) * 1000.0, 2)
        return perf_counter()

    step = perf_counter()
    rgba = image.convert("RGBA")
    original_size = rgba.size
    work = rgba
    longest = max(work.size)
    if longest > max_dim:
        scale = max_dim / longest
        work = work.resize((max(1, round(work.width * scale)), max(1, round(work.height * scale))), Image.Resampling.LANCZOS)

    array = np.asarray(work, dtype=np.float64) / 255.0
    rgb = array[..., :3]
    key_array = np.asarray(key, dtype=np.float64) / 255.0
    distance = np.max(np.abs(rgb - key_array), axis=2) * 255.0
    trimap = np.full(distance.shape, 0.5, dtype=np.float64)
    trimap[distance <= exact_tolerance] = 0.0
    trimap[distance >= foreground_tolerance] = 1.0
    step = _mark("prep_trimap", step)
    if np.any(trimap == 0.5):
        band = (distance - float(exact_tolerance)) / max(1.0, float(foreground_tolerance - exact_tolerance))
        alpha = np.clip(band, 0.0, 1.0)
        alpha = alpha * alpha * (3.0 - 2.0 * alpha)
        alpha[trimap == 0.0] = 0.0
        alpha[trimap == 1.0] = 1.0
        foreground = rgb
        step = _mark("alpha_band", step)
        step = _mark("foreground_preserve", step)
    else:
        # No unknown band: opaque art / flat-key holes only. The global solve and
        # ML foreground would be a no-op, so skip both heavy steps -- the alpha IS
        # the trimap and every visible pixel is already its own foreground.
        alpha = trimap
        foreground = rgb
        step = _mark("alpha_band", step)
        step = _mark("foreground_preserve", step)

    # Composite at the ORIGINAL resolution: keep the crisp original RGB where
    # opaque, blend in the band foreground only across the thin edge band. This
    # also undoes any work-resolution downscale blur.
    if work.size == original_size:
        alpha_full = alpha
        estimated_full = foreground * 255.0
    else:
        alpha_full = (
            np.asarray(Image.fromarray(np.rint(alpha * 255.0).astype(np.uint8), "L").resize(original_size, Image.Resampling.LANCZOS), dtype=np.float64)
            / 255.0
        )
        estimated_full = np.asarray(
            Image.fromarray(np.rint(foreground * 255.0).astype(np.uint8), "RGB").resize(original_size, Image.Resampling.LANCZOS),
            dtype=np.float64,
        )
    original_rgb = np.asarray(rgba, dtype=np.float64)[..., :3]
    keep_original = np.clip((alpha_full - 0.80) / 0.15, 0.0, 1.0)[..., None]
    foreground_full = keep_original * original_rgb + (1.0 - keep_original) * estimated_full
    # Sharp limit despill so the kept-original edge pixels can't carry a key halo
    # (e.g. green specks on the inner rim of a ring hole) without blurring detail.
    foreground_full = _limit_despill(foreground_full, key)

    output_array = np.zeros((original_size[1], original_size[0], 4), dtype=np.uint8)
    output_array[..., :3] = np.rint(np.clip(foreground_full, 0.0, 255.0)).astype(np.uint8)
    output_array[..., 3] = np.rint(np.clip(alpha_full * 255.0, 0.0, 255.0)).astype(np.uint8)
    result = Image.fromarray(output_array, "RGBA")
    # One general key-spill pass (keyed on the actual key colour, not per-palette
    # magic numbers) clears the residual anti-aliased edge fringe. Then
    # method-agnostic atlas hygiene.
    # Q3: the sharp _limit_despill above usually clears all key spill, so skip the
    # (expensive integral-image) decontamination unless visible spill remains.
    if _has_key_spill(result, key):
        decontaminate_source_key_spill_image(result, key=key, require_transparent_touch=False)
    # Q2: a 4px bleed covers the anti-aliased halo under transparent pixels; the
    # atlas packer's 2px extrude covers the rest. Only invisible RGB changes.
    bleed_transparent_rgb(result, key=key, passes=4)
    repair_transparent_edge_rgb(result, key=key)
    zero_fully_transparent_rgb(result)
    _mark("finalize_despill_hygiene", step)
    return result


def _has_key_spill(image: Image.Image, key: RGB) -> bool:
    array = np.asarray(image.convert("RGBA"), dtype=np.uint8)
    visible = array[..., 3] > 12
    if not visible.any():
        return False
    red = array[..., 0].astype(np.int16)
    green = array[..., 1].astype(np.int16)
    blue = array[..., 2].astype(np.int16)
    return bool(np.any(visible & source_key_spill_mask(red, green, blue, key)))
