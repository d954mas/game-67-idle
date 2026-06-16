#!/usr/bin/env python3
"""UI / text readability zoom-audit.

A recurring iteration failure is judging text legibility from the FULL
screenshot (where text looks fine at thumbnail size) while it is actually tiny,
low-contrast, or sitting on a bright background (e.g. numbers over the sky).
This tool forces a zoom: it crops the UI bands of a screenshot, upscales them
into a montage you MUST look at, and flags the specific "ink on a bright
background" failure (dark text/outline pixels whose local background is bright =
no solid plate behind the text -> low contrast).

Usage:
  py -3.12 tools/devapi/ui_readability.py <screenshot.png> [--out montage.png]
      [--region top=0,0,1,0.30 ...]   (fractions x0,y0,x1,y1; repeatable)

Default regions: a top band (HUD) and a bottom band (panel), full width.
Exit code 0 = PASS (no band flagged), 1 = WARN (a band looks unreadable),
2 = error. The montage is always written so a human/agent can eyeball it.
"""
from __future__ import annotations

import os
import sys

import numpy as np
from PIL import Image, ImageDraw

# Heuristic thresholds (tuned to flag "text on bright sky" + washed text).
INK_LUMA = 80          # <= this = "ink" (text body / dark outline)
BRIGHT_BG_LUMA = 165   # local background mean >= this = bright (e.g. sky/snow)
INK_ON_BRIGHT_WARN = 0.18   # >= this fraction of ink sits on bright bg -> WARN
MIN_INK_PIXELS = 150        # ignore bands with almost no text
THIN_STROKE_PX = 3.5        # median vertical ink run <= this (source px) -> hairline text


def luma(arr: np.ndarray) -> np.ndarray:
    r, g, b = arr[..., 0].astype(float), arr[..., 1].astype(float), arr[..., 2].astype(float)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def local_mean(l: np.ndarray, k: int = 9) -> np.ndarray:
    # cheap box blur via cumulative sums
    pad = k // 2
    p = np.pad(l, pad, mode="edge")
    cs = np.cumsum(np.cumsum(p, axis=0), axis=1)
    cs = np.pad(cs, ((1, 0), (1, 0)), mode="constant")
    H, W = l.shape
    ys = np.arange(H)[:, None]
    xs = np.arange(W)[None, :]
    y0, x0 = ys, xs
    y1, x1 = ys + k, xs + k
    tot = cs[y1, x1] - cs[y0, x1] - cs[y1, x0] + cs[y0, x0]
    return tot / (k * k)


def median_stroke_px(ink: np.ndarray) -> float:
    # median length of vertical contiguous ink runs ~= text stroke / glyph thickness.
    runs: list[int] = []
    H, W = ink.shape
    for x in range(0, W, 2):  # sample every other column for speed
        col = ink[:, x]
        run = 0
        for v in col:
            if v:
                run += 1
            elif run:
                runs.append(run); run = 0
        if run:
            runs.append(run)
    return float(np.median(runs)) if runs else 0.0


def audit_band(crop: np.ndarray, name: str) -> dict:
    l = luma(crop)
    ink = l <= INK_LUMA
    n_ink = int(ink.sum())
    if n_ink < MIN_INK_PIXELS:
        return {"name": name, "ink": n_ink, "ink_on_bright": 0.0, "stroke": 0.0, "contrast": float(l.std()), "warn": False, "reason": "little/no text"}
    bg = local_mean(l, 9)
    frac = float((ink & (bg >= BRIGHT_BG_LUMA)).sum()) / max(1, n_ink)
    stroke = median_stroke_px(ink)
    contrast = float(l.std())
    reason = []
    if frac >= INK_ON_BRIGHT_WARN:
        reason.append(f"{frac*100:.0f}% of text on a BRIGHT background (no solid plate)")
    if stroke <= THIN_STROKE_PX:
        reason.append(f"text strokes ~{stroke:.0f}px (hairline -> too small/thin to read; enlarge + bolder/outline)")
    warn = bool(reason)
    return {"name": name, "ink": n_ink, "ink_on_bright": frac, "stroke": stroke, "contrast": contrast, "warn": warn, "reason": "; ".join(reason) or "ok"}


def parse_regions(args: list[str]) -> dict[str, tuple]:
    regions: dict[str, tuple] = {}
    for a in args:
        name, _, rest = a.partition("=")
        x0, y0, x1, y1 = (float(v) for v in rest.split(","))
        regions[name] = (x0, y0, x1, y1)
    return regions


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    path = sys.argv[1]
    out = None
    before = None
    region_args: list[str] = []
    i = 2
    while i < len(sys.argv):
        if sys.argv[i] == "--out":
            out = sys.argv[i + 1]; i += 2
        elif sys.argv[i] == "--region":
            region_args.append(sys.argv[i + 1]); i += 2
        elif sys.argv[i] == "--compare":
            before = sys.argv[i + 1]; i += 2
        else:
            i += 1
    if not os.path.exists(path):
        print(f"error: no such file {path}", file=sys.stderr); return 2
    im = Image.open(path).convert("RGB")
    W, H = im.size
    regions = parse_regions(region_args) or {
        "top_hud": (0.0, 0.0, 1.0, 0.30),
        "bottom_panel": (0.0, 0.78, 1.0, 1.0),
    }
    if out is None:
        base, _ = os.path.splitext(path)
        out = base + "_uizoom.png"

    results, zoomed = [], []
    for name, (fx0, fy0, fx1, fy1) in regions.items():
        box = (int(fx0 * W), int(fy0 * H), int(fx1 * W), int(fy1 * H))
        crop = im.crop(box)
        arr = np.asarray(crop)
        results.append(audit_band(arr, name))
        z = crop.resize((int(crop.width * 3), int(crop.height * 3)), Image.LANCZOS)
        zoomed.append((name, z))

    # montage (stacked)
    mw = max(z.width for _, z in zoomed)
    mh = sum(z.height for _, z in zoomed) + 8 * (len(zoomed) - 1)
    canvas = Image.new("RGB", (mw, mh), (28, 28, 28))
    y = 0
    for _, z in zoomed:
        canvas.paste(z, (0, y)); y += z.height + 8
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    canvas.save(out)

    any_warn = False
    print(f"UI readability audit: {path}  ->  zoom montage: {out}")
    for r in results:
        tag = "WARN" if r["warn"] else "ok"
        print(f"  [{tag}] {r['name']}: ink={r['ink']} ink_on_bright={r['ink_on_bright']*100:.0f}% contrast={r['contrast']:.0f} -> {r['reason']}")
        any_warn = any_warn or r["warn"]
    print("LOOK AT THE ZOOM MONTAGE — do not judge text from the full screenshot.")
    print("WARN: text readability likely poor (see above)." if any_warn else "PASS: no text-on-bright flag (still eyeball the montage).")
    if before and os.path.exists(before):
        bim = Image.open(before).convert("RGB")
        rows = []
        for name, (fx0, fy0, fx1, fy1) in regions.items():
            for tag, src in (("BEFORE", bim), ("AFTER", im)):
                ww, hh = src.size
                c = src.crop((int(fx0 * ww), int(fy0 * hh), int(fx1 * ww), int(fy1 * hh)))
                z = c.resize((int(c.width * 2.5), int(c.height * 2.5)), Image.LANCZOS)
                ImageDraw.Draw(z).text((6, 4), f"{tag} {name}", fill=(255, 80, 80))
                rows.append(z)
        mw = max(z.width for z in rows)
        mh = sum(z.height for z in rows) + 6 * (len(rows) - 1)
        cmp_canvas = Image.new("RGB", (mw, mh), (18, 18, 18))
        y = 0
        for z in rows:
            cmp_canvas.paste(z, (0, y)); y += z.height + 6
        cmp_out = os.path.splitext(out)[0] + "_cmp.png"
        cmp_canvas.save(cmp_out)
        print(f"BEFORE/AFTER regression montage: {cmp_out} — confirm the change did NOT make any band WORSE (do not trade one axis for another).")
    return 1 if any_warn else 0


if __name__ == "__main__":
    raise SystemExit(main())
