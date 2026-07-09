#!/usr/bin/env python3
"""Peel garment layer from a dressed plate vs locked nude body plate.

Contract (paper-doll fit):
  nude   = body only (same pose/camera as game body_base)
  dressed = same figure + ONE garment (image_edit with nude as ref)
  out    = full-frame RGBA: garment only, registered to nude

This is NOT dual-plate (light/dark bg). It is content peel: pixels that
changed from nude keep dressed RGB; body-like pixels become transparent.

Usage:
  .venv\\Scripts\\python.exe games/web-dressup/tools/body_peel.py \\
    --nude nude.png --dressed dressed_tee.png --out tee_layer.png
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

W, H = 512, 896


def load_rgb(path: Path, size: tuple[int, int] | None = None) -> np.ndarray:
    im = Image.open(path).convert("RGB")
    if size and im.size != size:
        im = im.resize(size, Image.Resampling.LANCZOS)
    return np.asarray(im, dtype=np.float32)


def load_rgba(path: Path, size: tuple[int, int] | None = None) -> np.ndarray:
    im = Image.open(path).convert("RGBA")
    if size and im.size != size:
        im = im.resize(size, Image.Resampling.LANCZOS)
    return np.asarray(im, dtype=np.float32)


def _skin_mask(rgb: np.ndarray) -> np.ndarray:
    r, g, b = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
    return (r > 150) & (g > 100) & (b > 80) & (r > g) & (r > b) & ((r - b) > 15)


def peel(
    nude_rgb: np.ndarray,
    dressed_rgb: np.ndarray,
    *,
    thr: float = 28.0,
    soft: float = 18.0,
    skin_protect: bool = True,
    face_clear: bool = False,
) -> tuple[Image.Image, dict]:
    """Return RGBA layer + report."""
    if nude_rgb.shape != dressed_rgb.shape:
        raise ValueError(f"shape mismatch {nude_rgb.shape} vs {dressed_rgb.shape}")

    h, w = nude_rgb.shape[:2]
    diff = np.abs(dressed_rgb - nude_rgb).max(axis=2)
    # soft alpha: 0 at thr-soft, 1 at thr+soft
    alpha = np.clip((diff - (thr - soft)) / max(1.0, 2.0 * soft), 0.0, 1.0)

    if skin_protect:
        # Suppress peel on flat bg
        gray_n = nude_rgb.mean(axis=2)
        gray_d = dressed_rgb.mean(axis=2)
        bg = (np.abs(gray_n - gray_d) < 10) & (diff < 14)
        alpha = np.where(bg, 0.0, alpha)
        # On nude skin, require MUCH stronger change (kills arm/face ghost from edit recolor)
        skin = _skin_mask(nude_rgb)
        thr_skin = thr + 22.0
        alpha_skin = np.clip((diff - (thr_skin - soft)) / max(1.0, 2.0 * soft), 0.0, 1.0)
        alpha = np.where(skin, np.minimum(alpha, alpha_skin), alpha)
        # If dressed pixel is still skin-colored, drop it (not fabric)
        dressed_skin = _skin_mask(dressed_rgb)
        alpha = np.where(dressed_skin & skin, alpha * 0.15, alpha)

    # Morphological cleanup
    a_u8 = (alpha * 255).astype(np.uint8)
    a_img = Image.fromarray(a_u8, "L")
    a_img = a_img.filter(ImageFilter.MedianFilter(3))
    a_img = a_img.filter(ImageFilter.MaxFilter(3))
    a_img = a_img.filter(ImageFilter.MinFilter(3))
    alpha = np.asarray(a_img, dtype=np.float32) / 255.0

    if face_clear:
        # Punch face window so body face shows under hair/glasses stack
        cx, cy, rx, ry = w // 2, int(h * 0.13), int(w * 0.10), int(h * 0.07)
        yy, xx = np.ogrid[:h, :w]
        face = ((xx - cx) / max(1, rx)) ** 2 + ((yy - cy) / max(1, ry)) ** 2 <= 1.0
        bangs = yy < (cy - int(ry * 0.45))
        alpha = np.where(face & ~bangs, 0.0, alpha)

    out = np.zeros((*dressed_rgb.shape[:2], 4), dtype=np.uint8)
    out[:, :, :3] = np.clip(dressed_rgb, 0, 255).astype(np.uint8)
    out[:, :, 3] = np.clip(alpha * 255, 0, 255).astype(np.uint8)

    solid = int(np.count_nonzero(out[:, :, 3] > 40))
    report = {
        "solid_px": solid,
        "diff_mean": float(diff.mean()),
        "diff_p90": float(np.percentile(diff, 90)),
        "thr": thr,
        "soft": soft,
        "verdict": "ok" if solid > 800 else "empty",
    }
    if solid > (out.shape[0] * out.shape[1] * 0.55):
        report["verdict"] = "too_full"
    return Image.fromarray(out, "RGBA"), report


def main() -> int:
    ap = argparse.ArgumentParser(description="Body peel: dressed - nude → garment RGBA")
    ap.add_argument("--nude", required=True, type=Path)
    ap.add_argument("--dressed", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--size", default="512x896", help="WxH")
    ap.add_argument("--thr", type=float, default=28.0)
    ap.add_argument("--soft", type=float, default=18.0)
    ap.add_argument("--face-clear", action="store_true", help="Punch face oval (hair/glasses)")
    ap.add_argument("--report", type=Path, default=None)
    args = ap.parse_args()

    w, h = (int(x) for x in args.size.lower().split("x"))
    size = (w, h)
    nude = load_rgb(args.nude, size)
    dressed = load_rgb(args.dressed, size)
    layer, report = peel(
        nude, dressed, thr=args.thr, soft=args.soft, face_clear=args.face_clear
    )
    args.out.parent.mkdir(parents=True, exist_ok=True)
    layer.save(args.out)
    report["out"] = str(args.out)
    report["nude"] = str(args.nude)
    report["dressed"] = str(args.dressed)
    if args.report:
        args.report.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report))
    return 0 if report["verdict"] == "ok" else 2


if __name__ == "__main__":
    raise SystemExit(main())
