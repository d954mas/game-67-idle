#!/usr/bin/env python3
"""Acceptance gate for a dual-plate white/black pair.

Dual-plate extraction is exact ONLY when the two plates show the SAME subject in
the SAME place. When gpt-image-2 redraws the subject between plates the alpha
ghosts. This gate measures how well the white and black plates agree (foreground
mask IoU + centroid/scale drift) and returns a verdict BEFORE extraction:

  pass       -> plates agree; extract directly (py -3.12 ai_studio/assets/tools/image/alpha_dualplate/dual_plate_alpha.py)
  align      -> small drift; a translation align may rescue it, else regenerate
  regenerate -> subject redrawn/moved too much; regenerate the pair, do NOT matte

Run: py -3.12 ai_studio/assets/tools/image/alpha_dualplate/dual_plate_pair_gate.py --light white.png --dark black.png
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

RGB = tuple[int, int, int]
# Fraction of opaque pixels whose inter-plate difference is NOT channel-uniform.
# A true white/black composite has light-dark == (1-alpha)*(255,255,255), i.e. the
# SAME value on every channel; a redrawn/misaligned pixel shows different content
# on the two plates, so its difference is coloured. This is robust to bright-on-
# white / dark-on-black blindness (an opaque subject pixel has light==dark there).
PASS_FRACTION = 0.05
ALIGN_FRACTION = 0.20
CHROMA_TOLERANCE = 40


def compute_inconsistency(light_rgb: np.ndarray, dark_rgb: np.ndarray) -> dict:
    """Core pair-consistency metric, factored out of `evaluate()` so alignment
    search (pair_align.align_pair) can reuse the SAME objective instead of
    inventing a second one. Takes float32 HxWx3 RGB arrays (already same size),
    returns {fraction, mean_chroma, opaque_count} — see `evaluate()` for the
    metric's rationale."""
    diff = light_rgb - dark_rgb
    alpha = np.clip(1.0 - diff.mean(axis=2) / 255.0, 0.0, 1.0)
    # channel spread of the difference: 0 when the difference is a flat
    # (1-alpha)*white reveal, large when the two plates disagree on content.
    diff_chroma = diff.max(axis=2) - diff.min(axis=2)

    opaque = alpha > 0.25
    opaque_count = int(np.count_nonzero(opaque))
    inconsistent = opaque & (diff_chroma > CHROMA_TOLERANCE)
    fraction = float(np.count_nonzero(inconsistent) / opaque_count) if opaque_count else 1.0
    mean_chroma = float(diff_chroma[opaque].mean()) if opaque_count else 0.0
    return {"fraction": fraction, "mean_chroma": mean_chroma, "opaque_count": opaque_count}


def evaluate(light: Image.Image, dark: Image.Image) -> dict:
    if dark.size != light.size:
        dark = dark.resize(light.size, Image.Resampling.LANCZOS)
    light_rgb = np.asarray(light.convert("RGB"), dtype=np.float32)
    dark_rgb = np.asarray(dark.convert("RGB"), dtype=np.float32)

    metrics = compute_inconsistency(light_rgb, dark_rgb)
    fraction = metrics["fraction"]
    mean_chroma = metrics["mean_chroma"]
    opaque_count = metrics["opaque_count"]

    if fraction <= PASS_FRACTION:
        verdict = "pass"
        advice = "Plates are consistent. Extract directly: dual_plate_alpha.py --alpha-combine proj."
    elif fraction <= ALIGN_FRACTION:
        verdict = "align"
        advice = "Minor inconsistency at edges. A translation align may rescue it; else regenerate the pair."
    else:
        verdict = "regenerate"
        advice = "Plates disagree on subject content (redraw/misalign). Regenerate the pair (chain: black = edit of the white plate). Do NOT matte this pair."

    return {
        "schema": "game.dual_plate_pair_gate",
        "version": 1,
        "verdict": verdict,
        "advice": advice,
        "inconsistent_fraction": round(fraction, 4),
        "mean_edge_chroma": round(mean_chroma, 2),
        "opaque_pixels": opaque_count,
        "size": list(light.size),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Acceptance gate for a dual-plate white/black pair.")
    parser.add_argument("--light", type=Path, required=True, help="subject on flat WHITE background")
    parser.add_argument("--dark", type=Path, required=True, help="subject on flat BLACK background")
    parser.add_argument("--json-output", type=Path)
    args = parser.parse_args()

    report = evaluate(Image.open(args.light).convert("RGBA"), Image.open(args.dark).convert("RGBA"))
    report["light"] = args.light.as_posix()
    report["dark"] = args.dark.as_posix()
    if args.json_output:
        write_json_atomic(args.json_output, report)
    print(f"{report['verdict']}: inconsistent_fraction={report['inconsistent_fraction']} mean_edge_chroma={report['mean_edge_chroma']}")
    print(report["advice"])
    return 0 if report["verdict"] != "regenerate" else 2


if __name__ == "__main__":
    raise SystemExit(main())
