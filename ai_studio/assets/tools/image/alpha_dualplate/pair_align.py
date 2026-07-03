#!/usr/bin/env python3
"""Translation alignment for a dual-plate white/black pair (T0243).

Dual-plate extraction (dual_plate_alpha.py) is exact only when the two plates
show the SAME subject in the SAME place. Our pairs come from chained gpt-image
edits (black = edit of the white plate), and the model routinely drifts the
subject by a few px between plates. The pair gate (dual_plate_pair_gate.py)
flags this as verdict "align" — "a translation align may rescue it, else
regenerate" — but did not implement the rescue. This module does: it searches
small integer translations of the DARK plate that MINIMIZE the gate's OWN
inconsistency metric (dual_plate_pair_gate.compute_inconsistency) against the
light plate. No second metric is invented; align_pair optimizes the exact
number the gate reports as `inconsistent_fraction`.

Sign convention: align_pair(light, dark) returns (dx, dy) meaning "shift the
DARK plate dx columns right and dy rows down (either may be negative) to best
match the light plate". Concretely, aligned[y, x] = dark[y - dy, x - dx]
(revealed border pixels are filled with the dark plate's own background,
sampled as the median of its border ring — NOT numpy.roll's wrap-around, which
would smear the opposite edge into frame). If the dark plate's subject sits
(3, -2) away from the light plate's subject (3 px right, 2 px up), the fix is
to shift the dark plate (-3, +2) — align_pair returns (-3, 2) in that case.

Performance: an exhaustive full-resolution search over a 2*max_shift+1 grid
costs one gate-metric evaluation per candidate (~70-80ms at ~1250px on this
machine), so the naive 17x17 grid at max_shift=8 is ~20s — too slow for an
interactive canvas op. Instead this module evaluates the (exact, not
approximate-shift) candidate on a spatially decimated sample of the pair to
cheaply RANK all candidates, then re-verifies only the top few candidates at
full resolution before picking a winner. The shift itself is always applied at
full integer-pixel precision; decimation only thins which pixels are sampled
for the coarse ranking pass, never the shift amount.

Zero shift is always a candidate and always wins ties, so align_pair can never
make a pair worse than it already was.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[5]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import write_json_atomic
from ai_studio.assets.tools.image.alpha_dualplate.dual_plate_pair_gate import compute_inconsistency

DEFAULT_MAX_SHIFT = 8
# Aim for roughly this many pixels on the short side during the coarse ranking
# pass; below this the image is already cheap enough and step collapses to 1
# (an exact exhaustive search, which is what the unit-test-sized fixtures get).
COARSE_TARGET_SIDE = 180
# How many of the coarse-ranked candidates get a full-resolution verification pass.
REFINE_TOP_K = 8
# Border ring width (px) sampled to estimate a plate's flat background color.
BORDER_SAMPLE_MARGIN = 4


def _border_median(array: np.ndarray, margin: int = BORDER_SAMPLE_MARGIN) -> np.ndarray:
    """Median color of the array's outer ring — an estimate of the plate's flat
    background, used to fill the border revealed by a translation shift."""
    height, width = array.shape[:2]
    m = max(1, min(margin, height // 2, width // 2))
    channels = array.shape[-1]
    samples = np.concatenate(
        [
            array[:m, :].reshape(-1, channels),
            array[-m:, :].reshape(-1, channels),
            array[:, :m].reshape(-1, channels),
            array[:, -m:].reshape(-1, channels),
        ],
        axis=0,
    )
    return np.median(samples, axis=0)


def _pad_with_fill(array: np.ndarray, pad: int, fill: np.ndarray) -> np.ndarray:
    """Border-pad `array` by `pad` px on every side with `fill`, so any
    translation within [-pad, pad] can be read back as a zero-copy VIEW (see
    `_shifted_view`) instead of an edge-wrapping numpy.roll."""
    height, width, channels = array.shape
    padded = np.empty((height + 2 * pad, width + 2 * pad, channels), dtype=array.dtype)
    padded[...] = fill
    padded[pad : pad + height, pad : pad + width] = array
    return padded


def _shifted_view(padded: np.ndarray, pad: int, height: int, width: int, dx: int, dy: int) -> np.ndarray:
    """View into a `_pad_with_fill`-padded array equal to shifting the original
    dx columns right / dy rows down (see module docstring for the sign
    convention), for any |dx|, |dy| <= pad. Zero-copy."""
    return padded[pad - dy : pad - dy + height, pad - dx : pad - dx + width]


def align_pair(light: Image.Image, dark: Image.Image, max_shift: int = DEFAULT_MAX_SHIFT) -> tuple[int, int, float, Image.Image]:
    """Search integer translations of `dark` in [-max_shift, max_shift] on both
    axes that minimize the pair gate's inconsistency fraction against `light`.
    Returns (best_dx, best_dy, best_fraction, aligned_dark) — aligned_dark is an
    RGBA image (best_dx, best_dy) == (0, 0) leaves it pixel-identical to `dark`
    (border-padded but not offset)."""
    if dark.size != light.size:
        dark = dark.resize(light.size, Image.Resampling.LANCZOS)
    light_rgba = np.asarray(light.convert("RGBA"), dtype=np.float32)
    dark_rgba = np.asarray(dark.convert("RGBA"), dtype=np.float32)
    height, width = dark_rgba.shape[:2]

    fill = _border_median(dark_rgba)
    padded = _pad_with_fill(dark_rgba, max_shift, fill)

    candidates = [(dx, dy) for dy in range(-max_shift, max_shift + 1) for dx in range(-max_shift, max_shift + 1)]

    step = max(1, min(height, width) // COARSE_TARGET_SIDE)
    light_rgb = light_rgba[..., :3]
    if step == 1:
        # Small enough already (unit-test fixtures, thumbnails) — exact exhaustive search.
        shortlist = candidates
    else:
        light_small = light_rgb[::step, ::step]
        ranked = []
        for dx, dy in candidates:
            shifted_small = _shifted_view(padded, max_shift, height, width, dx, dy)[::step, ::step, :3]
            fraction = compute_inconsistency(light_small, shifted_small)["fraction"]
            ranked.append((fraction, dx, dy))
        ranked.sort(key=lambda item: item[0])
        shortlist = {(dx, dy) for _, dx, dy in ranked[:REFINE_TOP_K]}
        shortlist.add((0, 0))  # zero shift must always be a candidate

    # Baseline (zero shift) is always evaluated first and only ever beaten by a
    # STRICTLY better fraction, so it wins every tie — align_pair can never
    # return a shift that makes the pair worse (or even just "not better").
    baseline_shifted = _shifted_view(padded, max_shift, height, width, 0, 0)[..., :3]
    best_dx, best_dy = 0, 0
    best_fraction = compute_inconsistency(light_rgb, baseline_shifted)["fraction"]
    for dx, dy in shortlist:
        if dx == 0 and dy == 0:
            continue
        shifted = _shifted_view(padded, max_shift, height, width, dx, dy)[..., :3]
        fraction = compute_inconsistency(light_rgb, shifted)["fraction"]
        if fraction < best_fraction:
            best_fraction, best_dx, best_dy = fraction, dx, dy

    aligned_array = _shifted_view(padded, max_shift, height, width, best_dx, best_dy)
    aligned_dark = Image.fromarray(np.rint(aligned_array).clip(0, 255).astype(np.uint8), "RGBA")
    return best_dx, best_dy, best_fraction, aligned_dark


def main() -> int:
    parser = argparse.ArgumentParser(description="Search a translation that minimizes the dual-plate pair gate's inconsistency fraction.")
    parser.add_argument("--light", type=Path, required=True, help="subject on flat WHITE background")
    parser.add_argument("--dark", type=Path, required=True, help="subject on flat BLACK background")
    parser.add_argument("--max-shift", type=int, default=DEFAULT_MAX_SHIFT)
    parser.add_argument("--output", type=Path, help="write the aligned dark plate here")
    parser.add_argument("--json-output", type=Path)
    args = parser.parse_args()

    light_image = Image.open(args.light).convert("RGBA")
    dark_image = Image.open(args.dark).convert("RGBA")
    before = compute_inconsistency(
        np.asarray(light_image, dtype=np.float32)[..., :3],
        np.asarray(dark_image.resize(light_image.size, Image.Resampling.LANCZOS) if dark_image.size != light_image.size else dark_image, dtype=np.float32)[..., :3],
    )["fraction"]
    dx, dy, after, aligned_dark = align_pair(light_image, dark_image, max_shift=args.max_shift)

    report = {
        "schema": "game.dual_plate_pair_align",
        "version": 1,
        "dx": dx,
        "dy": dy,
        "fraction_before": round(before, 4),
        "fraction_after": round(after, 4),
        "light": args.light.as_posix(),
        "dark": args.dark.as_posix(),
    }
    if args.output:
        aligned_dark.save(args.output)
        report["output"] = args.output.as_posix()
    if args.json_output:
        write_json_atomic(args.json_output, report)
    print(f"align: dx={dx} dy={dy} inconsistent_fraction {report['fraction_before']} -> {report['fraction_after']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
