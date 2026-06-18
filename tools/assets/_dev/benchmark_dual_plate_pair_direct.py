#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
from pathlib import Path
from time import perf_counter

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[3]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.assets.atomic_io import save_image_atomic, write_json_atomic, write_text_atomic
from tools.assets._dev.benchmark_cutout_modes import BACKGROUNDS, checkerboard, contain, font, metrics, project_path, rel, soft_chroma_matte
from tools.assets._dev.benchmark_dual_plate_transfer import foreground_mask_from_bg, mask_iou
from tools.assets.dual_plate_alpha import extract_dual_plate_alpha


def composite_on(image: Image.Image, bg: tuple[int, int, int], size: tuple[int, int]) -> Image.Image:
    canvas = Image.new("RGBA", size, (*bg, 255))
    preview = contain(image, (size[0] - 12, size[1] - 12))
    canvas.alpha_composite(preview, ((size[0] - preview.width) // 2, (size[1] - preview.height) // 2))
    return canvas


def render_cell(name: str, image: Image.Image, lines: list[str], size: tuple[int, int] = (270, 340)) -> Image.Image:
    cell = Image.new("RGBA", size, (18, 19, 23, 255))
    draw = ImageDraw.Draw(cell)
    draw.rectangle((0, 0, size[0] - 1, size[1] - 1), outline=(90, 86, 78, 255), width=1)
    draw.text((10, 8), name, fill=(244, 238, 220, 255), font=font(14))
    top = checkerboard((size[0] - 20, 168), 12)
    preview = contain(image, (top.width - 14, top.height - 14))
    top.alpha_composite(preview, ((top.width - preview.width) // 2, (top.height - preview.height) // 2))
    cell.alpha_composite(top, (10, 34))
    bg_w = (size[0] - 28) // 3
    for index, (_name, bg) in enumerate(BACKGROUNDS):
        swatch = composite_on(image, bg, (bg_w, 62))
        cell.alpha_composite(swatch, (10 + index * (bg_w + 4), 218))
    for index, line in enumerate(lines[:4]):
        draw.text((10, 292 + index * 13), line, fill=(218, 211, 196, 255), font=font(10))
    return cell


def main() -> int:
    parser = argparse.ArgumentParser(description="Benchmark one direct AI white/black dual-plate pair.")
    parser.add_argument("--light", type=Path, required=True)
    parser.add_argument("--dark", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--id", default="dual_plate_pair")
    args = parser.parse_args()

    output_dir = project_path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    light_path = project_path(args.light)
    dark_path = project_path(args.dark)
    light = Image.open(light_path).convert("RGBA")
    dark = Image.open(dark_path).convert("RGBA")
    if light.size != dark.size:
        dark = dark.resize(light.size, Image.Resampling.LANCZOS)

    soft = soft_chroma_matte(light, (255, 255, 255))
    started = perf_counter()
    dual = extract_dual_plate_alpha(
        light,
        dark,
        bg_light=(255, 255, 255),
        bg_dark=(0, 0, 0),
        recovery_source="average",
    )
    dual_ms = round((perf_counter() - started) * 1000, 3)
    dual_path = output_dir / f"{args.id}_dual_alpha.png"
    save_image_atomic(dual, dual_path)

    light_mask = foreground_mask_from_bg(light, (255, 255, 255))
    dark_mask = foreground_mask_from_bg(dark, (0, 0, 0))
    pair_iou = mask_iou(light_mask, dark_mask)
    light_metrics = metrics(light, (255, 255, 255))
    soft_metrics = metrics(soft, (255, 255, 255))
    dual_metrics = metrics(dual, (255, 255, 255))

    cells = [
        render_cell("AI white plate", light, [f"size: {light.width}x{light.height}"]),
        render_cell("AI black plate", dark, [f"size: {dark.width}x{dark.height}", f"pair iou: {pair_iou}"]),
        render_cell("soft matte white", soft, [f"white px: {soft_metrics['visible_key_pixels']}", f"alpha: {soft_metrics['visible_pixels']}"]),
        render_cell("dual alpha result", dual, [f"time: {dual_ms} ms", f"white px: {dual_metrics['visible_key_pixels']}", f"alpha: {dual_metrics['visible_pixels']}"]),
    ]
    title_h = 76
    cell_w, cell_h = cells[0].size
    sheet = Image.new("RGBA", (cell_w * len(cells), title_h + cell_h), (12, 13, 16, 255))
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 10), f"{args.id} - direct AI dual-plate pair", fill=(248, 241, 222, 255), font=font(18))
    draw.text((12, 36), "Generated white/black pair is treated as the new source art. No transfer to the old crop and no source auto-align.", fill=(188, 181, 164, 255), font=font(11))
    draw.text((12, 52), f"light={rel(light_path)} dark={rel(dark_path)}", fill=(188, 181, 164, 255), font=font(10))
    for index, cell in enumerate(cells):
        sheet.alpha_composite(cell, (index * cell_w, title_h))

    overview_path = output_dir / f"{args.id}_direct_dual_plate_benchmark.png"
    save_image_atomic(sheet, overview_path)
    report = {
        "schema": "game.direct_dual_plate_pair_benchmark",
        "version": 1,
        "id": args.id,
        "light": rel(light_path),
        "dark": rel(dark_path),
        "dual_alpha": rel(dual_path),
        "overview": rel(overview_path),
        "pair_iou": pair_iou,
        "dual_plate_ms": dual_ms,
        "light_metrics": light_metrics,
        "soft_matte_metrics": soft_metrics,
        "dual_metrics": dual_metrics,
    }
    write_json_atomic(output_dir / f"{args.id}_direct_dual_plate_benchmark.json", report)
    write_text_atomic(
        output_dir / f"{args.id}_direct_dual_plate_benchmark.md",
        "\n".join(
            [
                "# Direct AI Dual-Plate Pair Benchmark",
                "",
                f"Overview: `{report['overview']}`",
                f"Dual alpha: `{report['dual_alpha']}`",
                f"Pair IoU: `{pair_iou}`",
                f"Dual extraction: `{dual_ms} ms`",
                "",
            ]
        ),
    )
    print(f"wrote overview: {rel(overview_path)}")
    print(f"wrote dual alpha: {rel(dual_path)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
