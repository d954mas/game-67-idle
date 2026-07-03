#!/usr/bin/env python3
"""Alternative matte path for OPAQUE / non-glow assets: run the in-repo
key_matte cutout per frame.

Runs in the REPO .venv. Imports the production
ai_studio.assets.tools.image.alpha_matte.key_matte tool (single source of the
op) and applies it frame-by-frame against a flat key colour. This is the cheap,
in-repo, no-licence path; glow/translucent assets should use CorridorKey
instead (key_matte cannot recover soft fractional alpha from one background --
its own docstring, and the T0257 R2 verdict).

Usage (invoked by matte.mjs; needs PYTHONPATH=<repo root>):
  python run_key_matte.py --frames <dir> --out <dir> --key 0,255,0
"""
from __future__ import annotations

import argparse
import json
import os
from time import perf_counter

from PIL import Image

from ai_studio.assets.tools.image.alpha_matte.key_matte import key_matte_cutout


def parse_key(text: str) -> tuple[int, int, int]:
    parts = [int(x) for x in text.replace("#", "").split(",")] if "," in text else None
    if parts is None and len(text.replace("#", "")) == 6:
        h = text.replace("#", "")
        parts = [int(h[i : i + 2], 16) for i in (0, 2, 4)]
    if not parts or len(parts) != 3:
        raise SystemExit(f"--key must be 'r,g,b' or '#rrggbb' (got '{text}')")
    return (parts[0], parts[1], parts[2])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--frames", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--key", default="0,255,0")
    args = parser.parse_args()

    key = parse_key(args.key)
    os.makedirs(args.out, exist_ok=True)
    frames = sorted(f for f in os.listdir(args.frames) if f.lower().endswith(".png"))
    if not frames:
        print(f"ERROR: no .png frames in {args.frames}", flush=True)
        return 2

    per_frame = []
    for fn in frames:
        img = Image.open(os.path.join(args.frames, fn)).convert("RGB")
        t0 = perf_counter()
        cutout = key_matte_cutout(img, key)
        dt = (perf_counter() - t0) * 1000.0
        cutout.save(os.path.join(args.out, fn))
        per_frame.append(round(dt, 1))

    print(
        json.dumps(
            {
                "frame_count": len(frames),
                "key": list(key),
                "per_frame_ms": per_frame,
                "mean_ms": round(sum(per_frame) / len(per_frame), 1),
            }
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
