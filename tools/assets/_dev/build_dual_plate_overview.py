#!/usr/bin/env python3
"""Dual-plate overview: for each benchmark art, show the three dual-plate frames
white plate | black plate | recovered result. This is the path-2 counterpart to
the single-background `benchmark_cutout_modes` overview, on the SAME real arts.

Plates come from the per-asset white/black pairs under the cutout benchmark; the
legendary wings use the dedicated legendary white/black candidate pair. Each row
also reports the pair acceptance-gate verdict.

Run: py -3.12 tools/assets/build_dual_plate_overview.py
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[3]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.assets.atomic_io import save_image_atomic
from tools.assets._dev.benchmark_cutout_modes import add_drop_shadow, crop_from_case, flatten_on_background, make_procedural_sign_truth
from tools.assets.cutout.dual_plate_alpha import extract_dual_plate_alpha
from tools.assets.cutout.dual_plate_pair_gate import evaluate

BENCH = ROOT / "gamedesign/projects/mine-cards/reviews/cutout_benchmark"
CAND = ROOT / "gamedesign/projects/mine-cards/art/candidates"
WINGS_ID = "green_angel_wings_glow_bad"


def font(size: int, bold: bool = False) -> ImageFont.ImageFont:
    for name in (["arialbd.ttf", "DejaVuSans-Bold.ttf"] if bold else ["arial.ttf", "DejaVuSans.ttf"]):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def fit(img: Image.Image, box: tuple[int, int]) -> Image.Image:
    im = img.convert("RGBA")
    scale = min((box[0] - 18) / im.width, (box[1] - 18) / im.height)
    return im.resize((max(1, int(im.width * scale)), max(1, int(im.height * scale))), Image.Resampling.LANCZOS)


def checker(size: tuple[int, int], cell: int = 12) -> Image.Image:
    w, h = size
    yy, xx = np.mgrid[0:h, 0:w]
    board = np.where(((xx // cell + yy // cell) % 2 == 0)[..., None], np.array([80, 82, 88.0]), np.array([58, 60, 66.0]))
    return Image.fromarray(board.astype("uint8"), "RGB").convert("RGBA")


def panel(img: Image.Image, size: tuple[int, int], mode: str, bg: tuple[int, int, int] = (0, 0, 0)) -> Image.Image:
    base = checker(size) if mode == "checker" else Image.new("RGBA", size, (*bg, 255))
    im = fit(img, size)
    base.alpha_composite(im, ((size[0] - im.width) // 2, (size[1] - im.height) // 2))
    return base


def _truth_for_case(case: dict):
    """Reconstruct the known-alpha art for synthetic/procedural fixtures so we can
    build a PERFECTLY ALIGNED white/black pair (no generator redraw, no ghosting)."""
    if case.get("procedural_source") == "wooden_sign":
        return make_procedural_sign_truth()
    if "alpha_source" in case and "source_key" not in case:
        rect = [int(v) for v in case.get("rect", [])] or None
        truth = crop_from_case(case["alpha_source"], rect)
        if case.get("add_shadow", False):
            shadow = case.get("shadow", {})
            truth = add_drop_shadow(
                truth,
                offset=tuple(shadow.get("offset", [18, 24])),
                blur=int(shadow.get("blur", 18)),
                opacity=int(shadow.get("opacity", 96)),
                padding=int(shadow.get("padding", 34)),
            )
        return truth
    return None


def dual_plates_for_case(case: dict):
    """White/black plate pair for a fixture. Prefer a REAL generated pair so the
    plates look like actual generations: legendary wings -> their candidate pair;
    other arts -> the per-asset AI white/black pair. Synthetic truth-derived
    plates are only a fallback when no real generated pair exists."""
    if case["id"] == WINGS_ID:
        white = CAND / "mine-cards-angel-wings-legendary-white-v001.png"
        black = CAND / "mine-cards-angel-wings-legendary-black-v001.png"
        if white.exists() and black.exists():
            return Image.open(white).convert("RGBA"), Image.open(black).convert("RGBA")
    folder = BENCH / "dual_plate_transfer/per_asset" / case["id"]
    white = folder / "ai_white_from_source.png"
    black = folder / "ai_black_from_source.png"
    if white.exists() and black.exists():
        return Image.open(white).convert("RGBA"), Image.open(black).convert("RGBA")
    truth = _truth_for_case(case)
    if truth is not None:
        return flatten_on_background(truth, (255, 255, 255)), flatten_on_background(truth, (0, 0, 0))
    return None


def main() -> int:
    cases = json.loads((BENCH / "fixtures.json").read_text(encoding="utf-8"))["cases"]
    cw, ch = 380, 380
    gap, mx, label_h, top = 18, 40, 30, 132
    rows: list[Image.Image] = []
    for case in cases:
        pair = dual_plates_for_case(case)
        if pair is None:
            continue
        white, black = pair
        if black.size != white.size:
            black = black.resize(white.size, Image.Resampling.LANCZOS)
        dual = extract_dual_plate_alpha(white, black, bg_light=(255, 255, 255), bg_dark=(0, 0, 0), recovery_source="average")
        verdict = evaluate(white, black)

        cells = [
            ("Белый плейт", panel(white, (cw, ch), "solid", (255, 255, 255))),
            ("Чёрный плейт", panel(black, (cw, ch), "solid", (0, 0, 0))),
            ("Результат (на любом фоне)", panel(dual, (cw, ch), "checker")),
        ]
        row_w = mx * 2 + cw * 3 + gap * 2
        row = Image.new("RGBA", (row_w, ch + label_h + 44), (20, 21, 26, 255))
        d = ImageDraw.Draw(row)
        d.text((mx, 8), case["label"], font=font(21, True), fill=(238, 232, 217))
        gv = verdict["verdict"]
        gcol = {"pass": (122, 199, 142), "align": (226, 196, 120)}.get(gv, (214, 132, 120))
        tag = f"пара: {gv} ({int(verdict['inconsistent_fraction'] * 100)}% расхождение)"
        d.text((row_w - mx - d.textlength(tag, font=font(16, True)), 12), tag, font=font(16, True), fill=gcol)
        for i, (lab, cell) in enumerate(cells):
            x = mx + i * (cw + gap)
            y = 40
            row.alpha_composite(cell, (x, y))
            d.text((x + 2, y + ch + 6), lab, font=font(16), fill=(188, 192, 200))
            if i < 2:
                d.text((x + cw + gap // 2 - 8, y + ch // 2 - 22), "+" if i == 0 else "", font=font(40, True), fill=(150, 154, 162))
        d.text((mx + 2 * (cw + gap) - gap // 2 - 12, 40 + ch // 2 - 26), "→", font=font(40, True), fill=(122, 199, 142))
        rows.append(row)

    width = max(r.width for r in rows)
    height = top + sum(r.height for r in rows) + 18 * len(rows)
    canvas = Image.new("RGBA", (width, height), (12, 13, 16, 255))
    d = ImageDraw.Draw(canvas)
    d.text((40, 34), "Dual-plate путь: белый + чёрный → результат", font=font(36, True), fill=(244, 238, 223))
    d.text((40, 84), "Тот же объект на двух фонах. alpha = 1 − (белый − чёрный) / 255 — так вытаскивается свечение и полупрозрачность.", font=font(21), fill=(178, 183, 193))
    y = top
    for row in rows:
        canvas.alpha_composite(row, ((width - row.width) // 2, y))
        y += row.height + 18
    out = BENCH / "dual_overview.png"
    save_image_atomic(canvas, out)
    print("wrote", out.as_posix(), canvas.size)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
