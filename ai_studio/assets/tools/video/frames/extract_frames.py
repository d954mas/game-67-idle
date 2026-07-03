#!/usr/bin/env python3
"""Extract every frame of a video to <out>/frame_%03d.png using PyAV.

Runs against the ComfyUI portable EMBEDDED Python (videoGenRoot
python_embeded), which ships PyAV (av) and PIL. The repo `.venv` deliberately
has no heavy video deps, so the frames stage subprocesses this interpreter --
that coupling is documented in the video/ README (v1 pragmatism).

Usage (invoked by frames.mjs; standalone-runnable too):
  python_embeded\\python.exe -s extract_frames.py --video <mp4> --out <dir>

Prints a one-line JSON summary to stdout for the caller. LOUD (nonzero exit)
if the source video is missing or decodes to zero frames.
"""
from __future__ import annotations

import argparse
import json
import os
import sys

import av  # PyAV — embedded python only


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--video", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    if not os.path.isfile(args.video):
        print(f"ERROR: source video not found: {args.video}", file=sys.stderr)
        return 2
    os.makedirs(args.out, exist_ok=True)

    count = 0
    container = av.open(args.video)
    try:
        stream = container.streams.video[0]
        avg_fps = float(stream.average_rate) if stream.average_rate else None
        for frame in container.decode(stream):
            img = frame.to_image()  # PIL.Image (RGB)
            img.save(os.path.join(args.out, f"frame_{count:03d}.png"))
            count += 1
    finally:
        container.close()

    if count == 0:
        print(f"ERROR: decoded 0 frames from {args.video}", file=sys.stderr)
        return 3

    print(json.dumps({"frame_count": count, "avg_fps": avg_fps, "av_version": av.__version__}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
