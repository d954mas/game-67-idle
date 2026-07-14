#!/usr/bin/env python3
"""Palette quantization: reduce an image's RGB color count, alpha preserved exactly.

T0207 (lead-settled 2026-07-02/03): Cleanup is TWO separate interactive tools —
Quantize (this one) and Denoise (`../denoise/denoise.py`) — not one monolithic
"clean up" pass. The Canvas alpha-and-cleanup contract
found quantize IS the fix for color-banding/gradient-noise artifacts, and that an
RGBA image can only be quantized with FASTOCTREE (libimagequant is absent from this
Pillow build) — so this ALWAYS splits alpha out, quantizes RGB with MEDIANCUT, and
reattaches the ORIGINAL alpha byte-identical. Dither is off by default (an exact
per-pixel nearest-palette-color mapping); pass --dither for Floyd-Steinberg.

CPU + numpy/Pillow only, through the studio venv. No network, deterministic (MEDIANCUT
carries no randomness), no fallbacks.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Any

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[5]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import save_image_atomic, write_json_atomic
from ai_studio.assets.tools.lib.color import merge_alpha, split_alpha

MIN_COLORS = 2
MAX_COLORS = 256
# Counting every unique RGB triple over a large sheet is fine for typical asset sizes,
# but the report caps the NUMBER IT PRINTS so a pathological huge/noisy source can't
# make the report itself misleadingly large (a "capped" count still proves "lots").
UNIQUE_COLOR_CAP = 100_000


def _count_unique_colors(rgb_array: "np.ndarray", cap: int = UNIQUE_COLOR_CAP) -> int:
    flat = np.ascontiguousarray(rgb_array.reshape(-1, rgb_array.shape[-1]))
    view = flat.view([("", flat.dtype)] * flat.shape[-1])
    unique = np.unique(view)
    return int(min(len(unique), cap))


def quantize_image(image: Image.Image, colors: int, *, dither: bool = False) -> tuple[Image.Image, dict[str, Any]]:
    """Reduce ``image`` to at most ``colors`` RGB colors via Pillow MEDIANCUT. The
    alpha channel is split out BEFORE quantizing and reattached byte-identical
    afterward (RGBA quantize would force FASTOCTREE and can shift alpha-adjacent
    pixels invisibly — this never touches alpha at all, by construction).
    Transparent pixels' RGB may still change under quantization; that is
    invisible and expected, not a bug. Returns (result_rgba, stats)."""
    if colors != int(colors) or not (MIN_COLORS <= int(colors) <= MAX_COLORS):
        raise ValueError(f"colors must be an integer between {MIN_COLORS} and {MAX_COLORS}, got {colors!r}")
    colors = int(colors)

    _rgba, rgb_before, alpha = split_alpha(image)

    palette_size_before = _count_unique_colors(rgb_before)

    rgb_image = Image.fromarray(rgb_before, "RGB")
    dither_mode = Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE
    quantized = rgb_image.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=dither_mode)
    quantized_rgb = np.asarray(quantized.convert("RGB"))

    palette_size_after = _count_unique_colors(quantized_rgb)

    result = merge_alpha(quantized_rgb, alpha)  # byte-identical alpha, never touched by quantization

    changed = np.any(quantized_rgb != rgb_before, axis=-1)
    changed_pixel_pct = float(np.count_nonzero(changed)) / float(changed.size) * 100.0 if changed.size else 0.0

    stats = {
        "colors_requested": colors,
        "palette_size_before": palette_size_before,
        "palette_size_after": palette_size_after,
        "changed_pixel_pct": round(changed_pixel_pct, 4),
    }
    return result, stats


def run(source: Path, output: Path, colors: int, dither: bool) -> dict[str, Any]:
    if not source.exists():
        raise FileNotFoundError(f"source image missing: {source}")
    image = Image.open(source).convert("RGBA")
    result, stats = quantize_image(image, colors, dither=dither)
    save_image_atomic(result, output)
    return {
        "schema": "ai_studio.image_tools.quantize_report.v1",
        "source": source.as_posix(),
        "dither": bool(dither),
        **stats,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Palette-quantize a PNG (RGB only; alpha preserved exactly).")
    parser.add_argument("--source", required=True, type=Path, help="source PNG")
    parser.add_argument("--out", required=True, type=Path, help="output PNG path")
    parser.add_argument("--colors", required=True, type=int, help="target palette size (2..256)")
    parser.add_argument("--dither", action="store_true", help="Floyd-Steinberg dither (default: off, exact mapping)")
    parser.add_argument("--report", type=Path, help="optional report JSON path")
    args = parser.parse_args()

    try:
        report = run(args.source, args.out, args.colors, args.dither)
    except (RuntimeError, ValueError, FileNotFoundError) as exc:
        # Deliberate refusals (bad colors, missing source) travel as ONE clean message —
        # no raw Python traceback surfaced to the operator. Unexpected bugs still traceback.
        raise SystemExit(str(exc)) from exc

    if args.report:
        write_json_atomic(args.report, report)
    print(
        f"pass: quantize ({report['colors_requested']} colors, "
        f"{report['palette_size_before']}->{report['palette_size_after']} unique, "
        f"{report['changed_pixel_pct']}% changed)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
