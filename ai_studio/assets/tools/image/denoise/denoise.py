#!/usr/bin/env python3
"""Light median denoise: strength maps to a median-filter pass ladder on RGB only.

T0207 (lead-settled 2026-07-02/03): Cleanup is TWO separate interactive tools —
Quantize (`../quantize/quantize.py`) and Denoise (this one). Kept deliberately
simple and honest per the lead's spec rather than importing an ML restorer or
opencv/skimage (cut in the research pass, tmp/research_art_cleanup_2026-07-03.md):
strength 1 = one 3px median pass, strength 2 = two 3px passes, strength 3 = one
5px pass. The alpha channel is NEVER filtered (the halo law — a soft/keyed edge's
alpha must stay exactly what the keyer produced; only RGB may be smoothed).

CPU + numpy/Pillow only, through the studio venv. No network, deterministic (a
median filter carries no randomness), no fallbacks.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Any

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[5]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import save_image_atomic, write_json_atomic
from ai_studio.assets.tools.lib.color import merge_alpha, split_alpha

STRENGTHS = (1, 2, 3)


def _median_passes(strength: int) -> list[int]:
    """Strength -> ordered list of median-filter kernel sizes applied sequentially."""
    if strength == 1:
        return [3]
    if strength == 2:
        return [3, 3]
    if strength == 3:
        return [5]
    raise ValueError(f"strength must be one of {STRENGTHS}, got {strength!r}")


def denoise_image(image: Image.Image, strength: int) -> tuple[Image.Image, dict[str, Any]]:
    """Median-filter ``image``'s RGB channels only; the alpha channel is split out
    BEFORE filtering and reattached byte-identical afterward (never filtered — the
    halo law). Returns (result_rgba, stats)."""
    passes = _median_passes(strength)
    _rgba, rgb_before, alpha = split_alpha(image)

    rgb_image = Image.fromarray(rgb_before, "RGB")
    for size in passes:
        rgb_image = rgb_image.filter(ImageFilter.MedianFilter(size=size))
    rgb_after = np.asarray(rgb_image)

    result = merge_alpha(rgb_after, alpha)  # byte-identical alpha, never filtered

    changed = np.any(rgb_after != rgb_before, axis=-1)
    changed_pixel_pct = float(np.count_nonzero(changed)) / float(changed.size) * 100.0 if changed.size else 0.0

    stats = {"strength": strength, "changed_pixel_pct": round(changed_pixel_pct, 4)}
    return result, stats


def run(source: Path, output: Path, strength: int) -> dict[str, Any]:
    if not source.exists():
        raise FileNotFoundError(f"source image missing: {source}")
    image = Image.open(source).convert("RGBA")
    result, stats = denoise_image(image, strength)
    save_image_atomic(result, output)
    return {
        "schema": "ai_studio.image_tools.denoise_report.v1",
        "source": source.as_posix(),
        **stats,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Light median denoise (RGB only; alpha never filtered).")
    parser.add_argument("--source", required=True, type=Path, help="source PNG")
    parser.add_argument("--out", required=True, type=Path, help="output PNG path")
    parser.add_argument("--strength", required=True, type=int, choices=STRENGTHS, help="1|2|3")
    parser.add_argument("--report", type=Path, help="optional report JSON path")
    args = parser.parse_args()

    try:
        report = run(args.source, args.out, args.strength)
    except (RuntimeError, ValueError, FileNotFoundError) as exc:
        # Deliberate refusals (missing source) travel as ONE clean message — no raw
        # Python traceback surfaced to the operator. Unexpected bugs still traceback.
        raise SystemExit(str(exc)) from exc

    if args.report:
        write_json_atomic(args.report, report)
    print(f"pass: denoise (strength {report['strength']}, {report['changed_pixel_pct']}% changed)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
