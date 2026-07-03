#!/usr/bin/env python3
"""Stage 4 of the Track B pipeline: RGBA frames -> a flipbook spritesheet.

Runs in the REPO .venv (pure PIL) so this stage does NOT depend on the video
experiment at all -- given a folder of RGBA frames it can pack a sheet on any
machine with the studio venv. Packs frames into a near-square grid (one uniform
frame box per cell, row-major) and writes <name>_sheet.png plus a
<name>_sheet.json meta with the full source provenance chain.

Deterministic column choice: columns = ceil(sqrt(count)) unless --columns is
given. Trim is OFF by default -- a flipbook wants uniform frame boxes so every
frame lands in the same cell rectangle; --trim crops every frame to the shared
union alpha bbox (kept identical across frames, so alignment is preserved).

Usage:
  python pack_sheet.py --run-dir <dir>                 (derives matte->sheet)
  python pack_sheet.py --frames <dir> --out <dir> --name <n> [--fps 16]
                       [--columns N] [--trim]
"""
from __future__ import annotations

import argparse
import json
import math
import os

from PIL import Image

SCHEMA = "ai_studio.video.spritesheet.v1"


def choose_columns(count: int) -> int:
    """Deterministic near-square column count: ceil(sqrt(count))."""
    if count <= 0:
        raise ValueError("cannot pack a spritesheet from 0 frames")
    return int(math.ceil(math.sqrt(count)))


def _union_alpha_bbox(frames: list[Image.Image]) -> tuple[int, int, int, int] | None:
    """Union of every frame's non-transparent bounding box (or None if all
    frames are fully transparent)."""
    box = None
    for img in frames:
        b = img.getchannel("A").getbbox()
        if b is None:
            continue
        if box is None:
            box = b
        else:
            box = (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    return box


def pack(frames: list[Image.Image], columns: int | None = None, trim: bool = False):
    """Pure packing: a list of RGBA frames -> (sheet Image, partial meta dict).

    LOUD if frames are empty or not all the same size (a flipbook needs a
    uniform frame box). No file IO here so the packing math is unit-testable
    with synthetic PIL frames.
    """
    if not frames:
        raise ValueError("cannot pack a spritesheet from 0 frames")
    frames = [f.convert("RGBA") for f in frames]

    if trim:
        box = _union_alpha_bbox(frames)
        if box is not None:
            frames = [f.crop(box) for f in frames]

    sizes = {f.size for f in frames}
    if len(sizes) != 1:
        raise ValueError(f"all frames must share one size; got {sorted(sizes)}")
    frame_w, frame_h = frames[0].size

    count = len(frames)
    cols = int(columns) if columns else choose_columns(count)
    if cols <= 0:
        raise ValueError(f"columns must be positive (got {cols})")
    rows = int(math.ceil(count / cols))

    sheet = Image.new("RGBA", (cols * frame_w, rows * frame_h), (0, 0, 0, 0))
    for i, frame in enumerate(frames):
        r, c = divmod(i, cols)
        sheet.paste(frame, (c * frame_w, r * frame_h))

    meta = {
        "frame_w": frame_w,
        "frame_h": frame_h,
        "count": count,
        "columns": cols,
        "rows": rows,
        "trim": bool(trim),
        "layout": "row-major",
    }
    return sheet, meta


def _load_frames(frames_dir: str) -> list[Image.Image]:
    names = sorted(f for f in os.listdir(frames_dir) if f.lower().endswith(".png"))
    if not names:
        raise SystemExit(f"no .png frames in {frames_dir}")
    return [Image.open(os.path.join(frames_dir, n)).convert("RGBA") for n in names]


def _read_json(path: str):
    if path and os.path.isfile(path):
        with open(path, "r", encoding="utf-8") as fh:
            return json.load(fh)
    return None


def build_sheet(frames_dir, out_dir, name, fps=16, columns=None, trim=False, source=None):
    """IO wrapper: load frames, pack, write <name>_sheet.png + <name>_sheet.json."""
    frames = _load_frames(frames_dir)
    sheet, meta = pack(frames, columns=columns, trim=trim)
    os.makedirs(out_dir, exist_ok=True)
    sheet_png = f"{name}_sheet.png"
    sheet_json = f"{name}_sheet.json"
    sheet.save(os.path.join(out_dir, sheet_png))
    full_meta = {
        "schema": SCHEMA,
        "name": name,
        **meta,
        "fps": fps,
        "sheet_png": sheet_png,
        "source": source or {"frames_dir": frames_dir},
    }
    with open(os.path.join(out_dir, sheet_json), "w", encoding="utf-8") as fh:
        json.dump(full_meta, fh, indent=2)
        fh.write("\n")
    return full_meta


def _harvest_source(run_dir, frames_dir):
    """Provenance chain from the sibling generate/ and matte/ stage outputs."""
    gen = _read_json(os.path.join(run_dir, "generate", "params.json")) if run_dir else None
    matte = _read_json(os.path.join(run_dir, "matte", "report.json")) if run_dir else None
    source = {"frames_dir": frames_dir}
    if gen:
        source["generate"] = {
            "profile": gen.get("profile"),
            "seed": gen.get("seed"),
            "motion_text": gen.get("motion_text"),
            "positive_prompt": gen.get("positive_prompt"),
            "workflow_file": gen.get("workflow_file"),
            "models": gen.get("models"),
            "video_file": gen.get("video_file"),
        }
    if matte:
        source["matte"] = {
            "tool": matte.get("tool"),
            "license": matte.get("license"),
            "settings": matte.get("settings"),
            "key": matte.get("key"),
            "screen_color": matte.get("screen_color"),
        }
    return source


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", dest="run_dir")
    parser.add_argument("--frames")
    parser.add_argument("--out")
    parser.add_argument("--name")
    parser.add_argument("--fps", type=float, default=16.0)
    parser.add_argument("--columns", type=int, default=None)
    parser.add_argument("--trim", action="store_true")
    args = parser.parse_args()

    run_dir = os.path.abspath(args.run_dir) if args.run_dir else None
    frames_dir = os.path.abspath(args.frames) if args.frames else (os.path.join(run_dir, "matte") if run_dir else None)
    out_dir = os.path.abspath(args.out) if args.out else (os.path.join(run_dir, "sheet") if run_dir else None)
    name = args.name or (os.path.basename(run_dir) if run_dir else None)
    if not frames_dir or not out_dir or not name:
        raise SystemExit("provide --run-dir OR (--frames AND --out AND --name)")

    source = _harvest_source(run_dir, frames_dir) if run_dir else {"frames_dir": frames_dir}
    meta = build_sheet(frames_dir, out_dir, name, fps=args.fps, columns=args.columns, trim=args.trim, source=source)
    print(json.dumps({"sheet_png": os.path.join(out_dir, meta["sheet_png"]), "meta": meta}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
