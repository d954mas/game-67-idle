#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[3]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.assets._dev.benchmark_cutout_modes import BACKGROUNDS, checkerboard, contain, metrics, source_image_for_case, soft_chroma_matte
from tools.assets.dual_plate_alpha import extract_dual_plate_alpha

BENCHMARK_DIR = ROOT / "gamedesign/projects/mine-cards/reviews/cutout_benchmark"
FIXTURES_PATH = BENCHMARK_DIR / "fixtures.json"
RGB = tuple[int, int, int]


def font(size: int) -> ImageFont.ImageFont:
    for name in ("DejaVuSans.ttf", "Arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def parse_color(value: str) -> RGB:
    text = value.strip().lstrip("#")
    return (int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16))


def composite_on(image: Image.Image, bg: RGB, size: tuple[int, int]) -> Image.Image:
    canvas = Image.new("RGBA", size, (*bg, 255))
    preview = contain(image, (size[0] - 12, size[1] - 12))
    canvas.alpha_composite(preview, ((size[0] - preview.width) // 2, (size[1] - preview.height) // 2))
    return canvas


def render_cell(name: str, image: Image.Image, lines: list[str], size: tuple[int, int] = (260, 330)) -> Image.Image:
    cell = Image.new("RGBA", size, (18, 19, 23, 255))
    draw = ImageDraw.Draw(cell)
    draw.rectangle((0, 0, size[0] - 1, size[1] - 1), outline=(90, 86, 78, 255), width=1)
    draw.text((10, 8), name, fill=(244, 238, 220, 255), font=font(14))

    top = checkerboard((size[0] - 20, 164), 12)
    preview = contain(image, (top.width - 14, top.height - 14))
    top.alpha_composite(preview, ((top.width - preview.width) // 2, (top.height - preview.height) // 2))
    cell.alpha_composite(top, (10, 34))

    bg_w = (size[0] - 28) // 3
    for index, (_name, bg) in enumerate(BACKGROUNDS):
        swatch = composite_on(image, bg, (bg_w, 62))
        cell.alpha_composite(swatch, (10 + index * (bg_w + 4), 214))

    for index, line in enumerate(lines[:3]):
        draw.text((10, 290 + index * 14), line, fill=(218, 211, 196, 255), font=font(10))
    return cell


def plate_pair_for_case(case_id: str) -> tuple[Image.Image, Image.Image, str]:
    pair_dir = BENCHMARK_DIR / "dual_plate_transfer/per_asset" / case_id
    light_path = pair_dir / "ai_white_from_source.png"
    dark_path = pair_dir / "ai_black_from_source.png"
    missing = [str(path) for path in (light_path, dark_path) if not path.exists()]
    if missing:
        raise FileNotFoundError("missing per-asset dual-plate file(s): " + ", ".join(missing))
    return Image.open(light_path).convert("RGBA"), Image.open(dark_path).convert("RGBA"), "per-asset source edit"


def build_full_dual_plate_benchmark() -> Path:
    fixtures = json.loads(FIXTURES_PATH.read_text(encoding="utf-8"))
    rows: list[Image.Image] = []

    for case in fixtures["cases"]:
        case_id = case["id"]
        key = parse_color(case["key"])
        green_source, _source_label, _rect = source_image_for_case(case)
        light_plate, dark_plate, pair_kind = plate_pair_for_case(case_id)
        if light_plate.size != dark_plate.size:
            dark_plate = dark_plate.resize(light_plate.size, Image.Resampling.LANCZOS)

        soft = soft_chroma_matte(green_source, key)
        dual_alpha = extract_dual_plate_alpha(
            light_plate,
            dark_plate,
            bg_light=(255, 255, 255),
            bg_dark=(0, 0, 0),
            recovery_source="average",
        )
        soft_metrics = metrics(soft, key)
        dual_metrics = metrics(dual_alpha, (255, 255, 255))
        cells = [
            render_cell("green source", green_source, [f"case: {case_id}", f"key: {case['key']}"]),
            render_cell("AI white plate", light_plate, [pair_kind, f"size: {light_plate.width}x{light_plate.height}"]),
            render_cell("AI black plate", dark_plate, [pair_kind, f"size: {dark_plate.width}x{dark_plate.height}"]),
            render_cell(
                "soft matte from green",
                soft,
                [f"alpha: {soft_metrics['visible_pixels']}", f"key px: {soft_metrics['visible_key_pixels']}"],
            ),
            render_cell(
                "direct dual alpha",
                dual_alpha,
                [f"alpha: {dual_metrics['visible_pixels']}", f"white px: {dual_metrics['visible_key_pixels']}"],
            ),
        ]

        title_h = 64
        cell_w, cell_h = cells[0].size
        row = Image.new("RGBA", (cell_w * len(cells), title_h + cell_h), (12, 13, 16, 255))
        draw = ImageDraw.Draw(row)
        draw.text((12, 10), f"{case_id} - green source vs direct dual-plate", fill=(248, 241, 222, 255), font=font(18))
        draw.text(
            (12, 36),
            "Green source is shown as the original problem. White/black pair is treated as the new source art for alpha extraction.",
            fill=(188, 181, 164, 255),
            font=font(11),
        )
        for index, cell in enumerate(cells):
            row.alpha_composite(cell, (index * cell_w, title_h))
        rows.append(row)

    width = max(row.width for row in rows)
    title_h = 94
    pad = 16
    height = title_h + sum(row.height for row in rows) + pad * len(rows)
    canvas = Image.new("RGBA", (width, height), (8, 9, 11, 255))
    draw = ImageDraw.Draw(canvas)
    draw.text((14, 12), "Direct AI dual-plate benchmark: full fixture set", fill=(248, 241, 222, 255), font=font(24))
    draw.text(
        (14, 46),
        "Each row shows the green problem source, AI white/black plates, single-background soft matte, and direct dual alpha.",
        fill=(188, 181, 164, 255),
        font=font(14),
    )
    y = title_h
    for row in rows:
        canvas.alpha_composite(row, ((width - row.width) // 2, y))
        y += row.height + pad

    dual_path = BENCHMARK_DIR / "02_direct_ai_dual_plate_benchmark.png"
    canvas.save(dual_path)
    return dual_path


def main() -> int:
    overview = Image.open(BENCHMARK_DIR / "overview.png").convert("RGBA")
    single_path = BENCHMARK_DIR / "01_single_green_chroma_benchmark.png"
    overview.save(single_path)
    dual_path = build_full_dual_plate_benchmark()

    print(single_path)
    print(dual_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
