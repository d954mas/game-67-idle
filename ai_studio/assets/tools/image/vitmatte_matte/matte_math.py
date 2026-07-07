#!/usr/bin/env python3
"""Pure numpy/scipy/PIL matting math for ``vitmatte_matte`` -- trimap builders
and the post-hoc despill un-blend. NO torch import here (deliberately): this
module must stay importable by the SHARED repo ``.venv`` (which never gets the
~2.7GB GPU torch stack), so tests and any non-GPU tooling can exercise the pure
math without the tool's own GPU venv. Only ``vitmatte_matte.py`` (the model
glue, run in the tool's OWN venv) touches torch/transformers.

Trimap builders ported from the alpha-bench study (tmp/alpha_bench/final/
vit_trimap.py, alpha-methods-portfolio bench 2026-07-07):

  auto-trimap (production path, flat-key plate): chroma distance to the KNOWN
  key colour. dist < T1 -> sure background (0); erode(dist > T2) -> sure
  foreground (255); everything else, dilated by DILATE_UNKNOWN -> unknown
  (128). T1/T2 were tuned ONCE on the opaque_hard_scavenger fixture (magenta
  plate) and then frozen for every flat-key fixture in the bench -- do not
  re-tune per image, that was the bench's own production-path assumption.

  mask-seeded trimap (busy/no-flat-plate path): same erode/dilate recipe, but
  seeded from a coarse neural mask's alpha (e.g. an SOD/rembg pass) instead of
  a chroma distance, for subjects that were never composited on a flat key.
"""
from __future__ import annotations

import numpy as np
from PIL import Image
from scipy import ndimage

RGB = tuple[int, int, int]

# Tuned once on opaque_hard_scavenger (magenta plate); frozen for all flat
# fixtures in the bench. Do not re-tune per image -- that defeats the point of
# a deterministic, zero-shot auto-trimap.
AUTO_T1 = 70.0   # chroma distance below this -> sure background (near key)
AUTO_T2 = 150.0  # chroma distance above this (then eroded) -> sure foreground
ERODE_FG = 3
DILATE_UNKNOWN = 7

# Below this alpha the compositing un-blend below is numerically degenerate
# (dividing by ~0) and the pixel is visually indistinguishable from pure
# background anyway.
DESPILL_ALPHA_CLAMP = 0.02


def _disk(radius: int) -> np.ndarray:
    """Circular structuring element of the given radius (matches vit_trimap.py's
    erosion/dilation shape, not scipy's default square)."""
    y, x = np.ogrid[-radius : radius + 1, -radius : radius + 1]
    return (x * x + y * y) <= radius * radius


def build_auto_trimap(
    plate_rgb: Image.Image,
    key: RGB = (255, 0, 255),
    t1: float = AUTO_T1,
    t2: float = AUTO_T2,
    erode_fg: int = ERODE_FG,
    dilate_unknown: int = DILATE_UNKNOWN,
) -> Image.Image:
    """Trimap from chroma distance to a flat plate ``key`` colour: the SAME
    protocol the canvas conveyor already uses for its key colours (MAGENTA /
    GREEN) -- a flat, known background lets the trimap be built with zero
    manual annotation. Euclidean RGB distance (0..441), matching the tuned
    thresholds above; not the Chebyshev metric ``lib/color.key_distance`` uses
    elsewhere in this tools tree -- T1/T2 were tuned against THIS metric."""
    rgb = np.asarray(plate_rgb.convert("RGB"), dtype=np.float64)
    key_array = np.asarray(key, dtype=np.float64)
    dist = np.sqrt(((rgb - key_array) ** 2).sum(axis=-1))
    bg = dist < t1
    fg_raw = dist > t2
    sure_fg = ndimage.binary_erosion(fg_raw, _disk(erode_fg))
    unknown = (~bg) & (~sure_fg)
    unknown = ndimage.binary_dilation(unknown, _disk(dilate_unknown))
    tri = np.full(dist.shape, 128, dtype=np.uint8)
    tri[bg] = 0
    tri[sure_fg] = 255
    tri[unknown] = 128
    return Image.fromarray(tri, "L")


def build_mask_seeded_trimap(
    mask_alpha: Image.Image,
    fg_thr: int = 200,
    bg_thr: int = 30,
    erode: int = ERODE_FG,
    dilate_unknown: int = DILATE_UNKNOWN,
) -> Image.Image:
    """Trimap seeded from a coarse neural mask's alpha channel, for busy/real
    backgrounds with no flat plate to chroma-key on (e.g. an SOD/rembg pass
    run first as the seed). Same erode/dilate recipe as ``build_auto_trimap``,
    just a different sure-fg/sure-bg source."""
    a = np.asarray(mask_alpha, dtype=np.uint8)
    sure_fg = ndimage.binary_erosion(a >= fg_thr, _disk(erode))
    sure_bg = ndimage.binary_erosion(a <= bg_thr, _disk(erode))
    unknown = (~sure_fg) & (~sure_bg)
    unknown = ndimage.binary_dilation(unknown, _disk(dilate_unknown))
    tri = np.full(a.shape, 128, dtype=np.uint8)
    tri[sure_bg] = 0
    tri[sure_fg] = 255
    tri[unknown] = 128
    return Image.fromarray(tri, "L")


def trimap_stats(trimap: Image.Image) -> dict:
    """bg/fg/unknown percentages of a trimap (0/128/255 only)."""
    a = np.asarray(trimap)
    return {
        "bg": round(100 * float((a == 0).mean()), 2),
        "fg": round(100 * float((a == 255).mean()), 2),
        "unknown": round(100 * float((a == 128).mean()), 2),
    }


def despill(plate_rgb_u8: np.ndarray, alpha01: np.ndarray, key_rgb: RGB) -> np.ndarray:
    """Chroma un-blend: recover the true foreground colour hidden under a
    partial key-colour blend. ViTMatte (like every alpha-only matting model)
    returns ALPHA ONLY -- its output RGB is untouched plate RGB, so every
    fractional-alpha pixel still carries a visible key-colour halo (the very
    compositing blend that produced it). The plate obeys the standard
    compositing equation ``plate = fg*a + key*(1-a)``; solving for ``fg``:

        fg = (plate - key * (1 - a)) / a

    Below ``DESPILL_ALPHA_CLAMP`` (0.02) the divide is numerically degenerate
    (dividing by ~0) and the pixel is indistinguishable from pure background
    anyway, so those pixels are clamped to fully transparent BLACK (fg=0, RGB
    zeroed). This function only returns the recovered RGB -- callers must also
    zero the alpha channel at the same clamp (this keeps despill a pure
    RGB-in/RGB-out transform with no alpha side effect of its own).

    Recovered RGB leak 32.1 -> 0.5 (rgb_leak_mean_abs) on the web_synthetic
    (spider-web) alpha-bench fixture when applied to raw vitmatte_auto output
    (alpha-methods-portfolio bench 2026-07-07; tmp/alpha_bench/final/
    metrics_final.json has the pre-despill ViTMatte leak numbers -- despill
    validation itself was a follow-up, not yet re-baked into that json).

    Vectorized; no python loops. ``plate_rgb_u8`` HxWx3 uint8, ``alpha01`` HxW
    float in [0, 1], ``key_rgb`` an (r, g, b) tuple in the same 0-255 scale as
    the plate. Returns HxWx3 uint8, clipped to [0, 255]."""
    plate = np.asarray(plate_rgb_u8, dtype=np.float64)
    alpha = np.asarray(alpha01, dtype=np.float64)
    key = np.asarray(key_rgb, dtype=np.float64)
    clamp = alpha < DESPILL_ALPHA_CLAMP
    # Divide by a safe (non-zero) stand-in under the clamp -- the result there
    # is discarded (overwritten with 0 below) regardless of what the divide
    # produces, so this only avoids a runtime divide-by-~0 warning/NaN.
    safe_alpha = np.where(clamp, 1.0, alpha)[..., None]
    fg = (plate - key * (1.0 - safe_alpha)) / safe_alpha
    fg = np.clip(fg, 0.0, 255.0)
    fg[clamp] = 0.0
    return np.rint(fg).astype(np.uint8)
