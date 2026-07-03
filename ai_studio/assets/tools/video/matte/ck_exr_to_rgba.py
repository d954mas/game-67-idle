#!/usr/bin/env python3
"""Convert CorridorKey EXR outputs (FG straight sRGB + Matte linear alpha) into
straight 8-bit RGBA PNGs.

Runs in the CorridorKey venv (its cv2 has OpenEXR). Mirrors the exact
conversion verified in T0257 phase-3 R2 (ck_exr_to_rgba.py): straight
foreground color + linear alpha -> premult-free RGBA, apples-to-apples with
key_matte output.

Usage (invoked by matte.mjs, OPENCV_IO_ENABLE_OPENEXR=1 in the env):
  python ck_exr_to_rgba.py --fg <Output/FG> --matte <Output/Matte> --out <dir>
"""
from __future__ import annotations

import argparse
import json
import os

os.environ.setdefault("OPENCV_IO_ENABLE_OPENEXR", "1")
import cv2  # noqa: E402  (env must be set before import)
import numpy as np  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fg", required=True, help="CorridorKey Output/FG dir (EXR)")
    parser.add_argument("--matte", required=True, help="CorridorKey Output/Matte dir (EXR)")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)
    frames = sorted(f for f in os.listdir(args.matte) if f.lower().endswith(".exr"))
    if not frames:
        print(f"ERROR: no .exr found in {args.matte}", flush=True)
        return 2

    written = 0
    for fn in frames:
        stem = os.path.splitext(fn)[0]
        fg_bgr = cv2.imread(os.path.join(args.fg, fn), cv2.IMREAD_ANYDEPTH | cv2.IMREAD_UNCHANGED)
        alpha = cv2.imread(os.path.join(args.matte, fn), cv2.IMREAD_ANYDEPTH | cv2.IMREAD_UNCHANGED)
        if fg_bgr is None or alpha is None:
            print(f"ERROR: failed to read EXR pair for {stem}", flush=True)
            return 3
        if alpha.ndim == 3:
            alpha = alpha[..., 0]
        fg_rgb = cv2.cvtColor(fg_bgr, cv2.COLOR_BGR2RGB)
        fg8 = (np.clip(fg_rgb, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
        a8 = (np.clip(alpha, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
        rgba = np.dstack([fg8, a8])
        cv2.imwrite(os.path.join(args.out, stem + ".png"), cv2.cvtColor(rgba, cv2.COLOR_RGBA2BGRA))
        written += 1

    print(json.dumps({"frame_count": written, "out": args.out}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
