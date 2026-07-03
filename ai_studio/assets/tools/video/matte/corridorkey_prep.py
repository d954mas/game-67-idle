#!/usr/bin/env python3
"""Build a CorridorKey shot: Input/ (raw frames) + AlphaHint/ (coarse chroma).

Runs in the REPO .venv (numpy + PIL). Mirrors the exact rough-chroma-hint
recipe verified in T0257 phase-3 R2 (setup_corridorkey_shot.py): green
dominance -> close -> light dilate -> feather, so the soft glow halo is
INCLUDED as foreground and CorridorKey reconstructs the fine alpha. CorridorKey
was trained on coarse, blurry, eroded masks; the hint does not need to be
precise.

Usage (invoked by matte.mjs):
  python corridorkey_prep.py --frames <framesDir> --shot <ClipsForInference/shot>
"""
from __future__ import annotations

import argparse
import json
import os
import shutil

import numpy as np
from PIL import Image, ImageFilter


def rough_chroma_hint(img: Image.Image) -> Image.Image:
    """Coarse foreground mask via green dominance, dilated + feathered so the
    soft glow halo is included as foreground."""
    arr = np.array(img.convert("RGB")).astype(np.int32)
    r, g, b = arr[..., 0], arr[..., 1], arr[..., 2]
    greenness = g - np.maximum(r, b)
    is_bg = (greenness > 40) & (g > 110)
    fg = (~is_bg).astype(np.uint8) * 255
    m = Image.fromarray(fg, "L")
    m = m.filter(ImageFilter.MaxFilter(5))  # close small holes (dilate)
    m = m.filter(ImageFilter.MinFilter(5))  # (erode)
    m = m.filter(ImageFilter.MaxFilter(5))  # light dilation to catch the glow halo
    m = m.filter(ImageFilter.GaussianBlur(3))  # feather / coarsen
    return m


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--frames", required=True)
    parser.add_argument("--shot", required=True)
    args = parser.parse_args()

    in_dir = os.path.join(args.shot, "Input")
    hint_dir = os.path.join(args.shot, "AlphaHint")
    os.makedirs(in_dir, exist_ok=True)
    os.makedirs(hint_dir, exist_ok=True)

    frames = sorted(f for f in os.listdir(args.frames) if f.lower().endswith(".png"))
    for fn in frames:
        src = os.path.join(args.frames, fn)
        shutil.copyfile(src, os.path.join(in_dir, fn))
        rough_chroma_hint(Image.open(src)).save(os.path.join(hint_dir, fn))

    print(json.dumps({"frame_count": len(frames), "input_dir": in_dir, "hint_dir": hint_dir}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
