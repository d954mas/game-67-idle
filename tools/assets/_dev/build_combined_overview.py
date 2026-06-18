#!/usr/bin/env python3
"""Combined cutout overview: ONE sheet where every benchmark art shows BOTH
paths in a single row -- the single-background methods (source / current /
aggressive / holes / soft matte / pymatting / key matte) AND the dual-plate path
(white plate | black plate | result). Same arts as the single-bg overview.

Run: py -3.12 tools/assets/build_combined_overview.py
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
from tools.assets._dev.benchmark_cutout_modes import SINGLE_BACKGROUND_MODES, parse_color, source_image_for_case
from tools.assets._dev.build_dual_plate_overview import dual_plates_for_case
from tools.assets.dual_plate_alpha import extract_dual_plate_alpha

BENCH = ROOT / "gamedesign/projects/mine-cards/reviews/cutout_benchmark"


def font(size: int, bold: bool = False) -> ImageFont.ImageFont:
    for name in (["arialbd.ttf", "DejaVuSans-Bold.ttf"] if bold else ["arial.ttf", "DejaVuSans.ttf"]):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def fit(img: Image.Image, box: tuple[int, int]) -> Image.Image:
    im = img.convert("RGBA")
    scale = min((box[0] - 12) / im.width, (box[1] - 12) / im.height)
    return im.resize((max(1, int(im.width * scale)), max(1, int(im.height * scale))), Image.Resampling.LANCZOS)


def checker(size: tuple[int, int], cell: int = 11) -> Image.Image:
    w, h = size
    yy, xx = np.mgrid[0:h, 0:w]
    board = np.where(((xx // cell + yy // cell) % 2 == 0)[..., None], np.array([80, 82, 88.0]), np.array([56, 58, 64.0]))
    return Image.fromarray(board.astype("uint8"), "RGB").convert("RGBA")


def cell(img: Image.Image, size: tuple[int, int], mode: str, bg=(34, 36, 42)) -> Image.Image:
    base = checker(size) if mode == "checker" else Image.new("RGBA", size, (*bg, 255))
    im = fit(img, size)
    base.alpha_composite(im, ((size[0] - im.width) // 2, (size[1] - im.height) // 2))
    return base


def main() -> int:
    cases = json.loads((BENCH / "fixtures.json").read_text(encoding="utf-8"))["cases"]
    cs = 210
    gap, mx, top = 6, 30, 150
    name_col = 150  # left column for the art name
    single_labels = [name for name, _ in SINGLE_BACKGROUND_MODES]
    dual_labels = ["white plate", "black plate", "dual result"]
    columns = single_labels + dual_labels
    divider_after = len(single_labels)  # gap between path 1 and path 2

    rows: list[Image.Image] = []
    for case in cases:
        green, _label, _rect = source_image_for_case(case)
        key = parse_color(case["key"])
        cells: list[tuple[Image.Image, str]] = []
        for name, fn in SINGLE_BACKGROUND_MODES:
            out = fn(green, key)
            cells.append((cell(out, (cs, cs), "checker"), "solid" if name == "source" else "cut"))

        pair = dual_plates_for_case(case)
        if pair is not None:
            white, black = pair
            if black.size != white.size:
                black = black.resize(white.size, Image.Resampling.LANCZOS)
            dual = extract_dual_plate_alpha(white, black, bg_light=(255, 255, 255), bg_dark=(0, 0, 0), recovery_source="average")
            cells.append((cell(white, (cs, cs), "plain", (245, 245, 245)), "plate"))
            cells.append((cell(black, (cs, cs), "plain", (8, 8, 8)), "plate"))
            cells.append((cell(dual, (cs, cs), "checker"), "cut"))
        else:
            for _ in range(3):
                cells.append((cell(Image.new("RGBA", (cs, cs), (0, 0, 0, 0)), (cs, cs), "plain"), "na"))

        row_w = mx + name_col + len(columns) * (cs + gap) + 20
        row = Image.new("RGBA", (row_w, cs + 30), (20, 21, 26, 255))
        d = ImageDraw.Draw(row)
        # wrapped art name in the left column
        words = case["label"].split()
        line, lines = "", []
        for w in words:
            if d.textlength((line + " " + w).strip(), font=font(15, True)) > name_col - 8:
                lines.append(line)
                line = w
            else:
                line = (line + " " + w).strip()
        lines.append(line)
        for li, ln in enumerate(lines[:5]):
            d.text((mx, 14 + li * 18), ln, font=font(15, True), fill=(232, 226, 211))
        x0 = mx + name_col
        for i, (c, _kind) in enumerate(cells):
            extra = 14 if i >= divider_after else 0
            x = x0 + i * (cs + gap) + extra
            row.alpha_composite(c, (x, 12))
        rows.append(row)

    width = max(r.width for r in rows)
    height = top + sum(r.height for r in rows) + 8 * len(rows)
    canvas = Image.new("RGBA", (width, height), (12, 13, 16, 255))
    d = ImageDraw.Draw(canvas)
    d.text((mx, 30), "Вырезание AI-арта: путь 1 (matte) и путь 2 (dual-plate) на одних артах", font=font(32, True), fill=(244, 238, 223))
    d.text((mx, 74), "Слева направо: исходник на зелёном → методы одного фона → | → dual-plate: белый плейт, чёрный плейт, результат.", font=font(19), fill=(178, 183, 193))
    # column headers
    x0 = mx + name_col
    for i, label in enumerate(columns):
        extra = 14 if i >= divider_after else 0
        x = x0 + i * (cs + gap) + extra
        col = (122, 199, 142) if label in ("key matte", "dual result") else (188, 192, 200)
        tw = d.textlength(label, font=font(15, True))
        d.text((x + (cs - tw) / 2, top - 26), label, font=font(15, True), fill=col)
    # divider line between the two paths
    div_x = x0 + divider_after * (cs + gap) + 3
    d.line((div_x, top - 30, div_x, height - 20), fill=(70, 74, 84), width=2)
    y = top
    for row in rows:
        canvas.alpha_composite(row, (0, y))
        y += row.height + 8
    out = BENCH / "combined_overview.png"
    save_image_atomic(canvas, out)
    print("wrote", out.as_posix(), canvas.size)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
