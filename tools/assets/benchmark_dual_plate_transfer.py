#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from time import perf_counter
from typing import Any

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

try:
    import numpy as np
except ImportError:  # pragma: no cover
    np = None

from tools.assets.atomic_io import save_image_atomic, write_json_atomic, write_text_atomic
from tools.assets.benchmark_cutout_modes import (
    BACKGROUNDS,
    checkerboard,
    contain,
    font,
    metrics,
    project_path,
    rel,
    source_image_for_case,
    soft_chroma_matte,
)
from tools.assets.dual_plate_alpha import extract_dual_plate_alpha


RGB = tuple[int, int, int]


def parse_color(value: str) -> RGB:
    text = value.strip().lstrip("#")
    if len(text) != 6:
        raise ValueError(f"expected #rrggbb, got {value!r}")
    return (int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16))


def load_cases(fixtures_path: Path) -> list[dict[str, Any]]:
    fixtures = json.loads(fixtures_path.read_text(encoding="utf-8"))
    cases = fixtures.get("cases")
    if not isinstance(cases, list) or not cases:
        raise SystemExit("fixtures must contain non-empty cases")
    return cases


def crop_case(case: dict[str, Any]) -> Image.Image:
    source, _source_label, _rect = source_image_for_case(case)
    return source


def composite_on(image: Image.Image, bg: RGB, size: tuple[int, int]) -> Image.Image:
    canvas = Image.new("RGBA", size, (*bg, 255))
    preview = contain(image, (size[0] - 12, size[1] - 12))
    canvas.alpha_composite(preview, ((size[0] - preview.width) // 2, (size[1] - preview.height) // 2))
    return canvas


def render_cell(name: str, image: Image.Image, lines: list[str], size: tuple[int, int] = (250, 320)) -> Image.Image:
    cell = Image.new("RGBA", size, (18, 19, 23, 255))
    draw = ImageDraw.Draw(cell)
    draw.rectangle((0, 0, size[0] - 1, size[1] - 1), outline=(90, 86, 78, 255), width=1)
    draw.text((10, 8), name, fill=(244, 238, 220, 255), font=font(14))

    top = checkerboard((size[0] - 20, 154), 12)
    preview = contain(image, (top.width - 14, top.height - 14))
    top.alpha_composite(preview, ((top.width - preview.width) // 2, (top.height - preview.height) // 2))
    cell.alpha_composite(top, (10, 32))

    bg_w = (size[0] - 28) // 3
    for index, (_name, bg) in enumerate(BACKGROUNDS):
        swatch = composite_on(image, bg, (bg_w, 56))
        cell.alpha_composite(swatch, (10 + index * (bg_w + 4), 198))

    for index, line in enumerate(lines[:4]):
        draw.text((10, 266 + index * 13), line, fill=(218, 211, 196, 255), font=font(10))
    return cell


def prepare_source_reference(cases: list[dict[str, Any]], output_dir: Path) -> dict[str, Any]:
    source_dir = output_dir / "source_crops"
    source_dir.mkdir(parents=True, exist_ok=True)
    row_w = 520
    row_h = 250
    sheet = Image.new("RGBA", (row_w, row_h * len(cases)), (34, 34, 38, 255))
    draw = ImageDraw.Draw(sheet)
    for index, case in enumerate(cases):
        crop = crop_case(case)
        crop_path = source_dir / f"{case['id']}.png"
        save_image_atomic(crop, crop_path)
        y = index * row_h
        draw.rectangle((0, y, row_w - 1, y + row_h - 1), fill=(36, 36, 40, 255), outline=(84, 80, 70, 255))
        preview = contain(crop, (430, 190))
        sheet.alpha_composite(preview, ((row_w - preview.width) // 2, y + 46 + (190 - preview.height) // 2))
        draw.text((14, y + 12), f"{index + 1}. {case['id']}", fill=(235, 226, 202, 255), font=font(18))
    output_path = output_dir / "source_reference_sheet.png"
    save_image_atomic(sheet, output_path)
    return {"source_reference_sheet": rel(output_path)}


def alpha_bbox(image: Image.Image) -> tuple[int, int, int, int] | None:
    return image.convert("RGBA").getchannel("A").getbbox()


def trim_alpha(image: Image.Image) -> Image.Image:
    bbox = alpha_bbox(image)
    if not bbox:
        return image.convert("RGBA")
    return image.convert("RGBA").crop(bbox)


def threshold_bbox(image: Image.Image, threshold: int = 12) -> tuple[int, int, int, int] | None:
    alpha = image.convert("RGBA").getchannel("A")
    if np is None:
        return alpha.point(lambda value: 255 if value > threshold else 0).getbbox()
    array = np.asarray(alpha, dtype=np.uint8)
    mask = np.where(array > threshold, 255, 0).astype(np.uint8)
    return Image.fromarray(mask, "L").getbbox()


def transfer_alpha_to_source(source: Image.Image, alpha_source: Image.Image) -> Image.Image:
    source_rgba = source.convert("RGBA")
    alpha_crop = trim_alpha(alpha_source)
    alpha = alpha_crop.getchannel("A").resize(source_rgba.size, Image.Resampling.LANCZOS)
    output = source_rgba.copy()
    output.putalpha(alpha)
    return output


def transfer_alpha_to_source_bbox(source: Image.Image, alpha_source: Image.Image, target_reference: Image.Image) -> Image.Image:
    source_rgba = source.convert("RGBA")
    alpha_crop = trim_alpha(alpha_source)
    target_bbox = threshold_bbox(target_reference)
    if not target_bbox:
        return transfer_alpha_to_source(source, alpha_source)
    left, top, right, bottom = target_bbox
    target_w = max(1, right - left)
    target_h = max(1, bottom - top)
    alpha = alpha_crop.getchannel("A").resize((target_w, target_h), Image.Resampling.LANCZOS)
    full_alpha = Image.new("L", source_rgba.size, 0)
    full_alpha.paste(alpha, (left, top))
    output = source_rgba.copy()
    output.putalpha(full_alpha)
    return output


def alpha_iou(a: Image.Image, b: Image.Image, threshold: int = 12) -> float | None:
    if np is None:
        return None
    aa = np.asarray(a.convert("RGBA").getchannel("A").resize(b.size, Image.Resampling.LANCZOS), dtype=np.uint8) > threshold
    bb = np.asarray(b.convert("RGBA").getchannel("A"), dtype=np.uint8) > threshold
    union = np.count_nonzero(aa | bb)
    if union == 0:
        return None
    return round(float(np.count_nonzero(aa & bb) / union), 3)


def foreground_mask_from_bg(image: Image.Image, bg: RGB, threshold: int = 24) -> Image.Image:
    rgba = image.convert("RGBA")
    if np is None:
        output = Image.new("L", rgba.size, 0)
        out = output.load()
        for y in range(rgba.height):
            for x in range(rgba.width):
                red, green, blue, _alpha = rgba.getpixel((x, y))
                if max(abs(red - bg[0]), abs(green - bg[1]), abs(blue - bg[2])) > threshold:
                    out[x, y] = 255
        return output
    array = np.asarray(rgba, dtype=np.int16)
    bg_array = np.asarray(bg, dtype=np.int16)
    distance = np.max(np.abs(array[..., :3] - bg_array), axis=2)
    mask = np.where(distance > threshold, 255, 0).astype(np.uint8)
    return Image.fromarray(mask, "L")


def bbox_from_mask(mask: Image.Image, padding: int = 10) -> tuple[int, int, int, int] | None:
    bbox = mask.convert("L").point(lambda value: 255 if value > 0 else 0).getbbox()
    if not bbox:
        return None
    left, top, right, bottom = bbox
    return (
        max(0, left - padding),
        max(0, top - padding),
        min(mask.width, right + padding),
        min(mask.height, bottom + padding),
    )


def align_plate_pair(light_plate: Image.Image, dark_plate: Image.Image) -> tuple[Image.Image, Image.Image, dict[str, Any]]:
    light_rgba = light_plate.convert("RGBA")
    dark_rgba = dark_plate.convert("RGBA")
    light_mask = foreground_mask_from_bg(light_rgba, (255, 255, 255))
    dark_mask = foreground_mask_from_bg(dark_rgba, (0, 0, 0))
    light_bbox = bbox_from_mask(light_mask)
    dark_bbox = bbox_from_mask(dark_mask)
    if not light_bbox or not dark_bbox:
        return light_rgba, dark_rgba, {"aligned": False, "reason": "missing foreground bbox"}

    light_crop = light_rgba.crop(light_bbox)
    dark_crop = dark_rgba.crop(dark_bbox)
    target_w = max(light_crop.width, dark_crop.width)
    target_h = max(light_crop.height, dark_crop.height)
    target_size = (target_w, target_h)

    def fit_on_bg(image: Image.Image, bg: RGB) -> Image.Image:
        fitted = image.resize(target_size, Image.Resampling.LANCZOS) if image.size != target_size else image
        canvas = Image.new("RGBA", target_size, (*bg, 255))
        canvas.alpha_composite(fitted)
        return canvas

    return (
        fit_on_bg(light_crop, (255, 255, 255)),
        fit_on_bg(dark_crop, (0, 0, 0)),
        {
            "aligned": True,
            "light_bbox": list(light_bbox),
            "dark_bbox": list(dark_bbox),
            "target_size": list(target_size),
        },
    )


def mask_iou(a: Image.Image, b: Image.Image, threshold: int = 12) -> float | None:
    if np is None:
        return None
    aa = np.asarray(a.convert("L").resize(b.size, Image.Resampling.LANCZOS), dtype=np.uint8) > threshold
    bb = np.asarray(b.convert("L"), dtype=np.uint8) > threshold
    union = np.count_nonzero(aa | bb)
    if union == 0:
        return None
    return round(float(np.count_nonzero(aa & bb) / union), 3)


def split_ai_sheet(ai_sheet: Image.Image, rows: int) -> list[tuple[Image.Image, Image.Image]]:
    rgba = ai_sheet.convert("RGBA")
    half_w = rgba.width // 2
    row_h = rgba.height // rows
    pairs: list[tuple[Image.Image, Image.Image]] = []
    for index in range(rows):
        y0 = index * row_h
        y1 = rgba.height if index == rows - 1 else (index + 1) * row_h
        light = rgba.crop((0, y0, half_w, y1))
        dark = rgba.crop((half_w, y0, rgba.width, y1))
        pairs.append((light, dark))
    return pairs


def process_ai_sheet(cases: list[dict[str, Any]], ai_sheet_path: Path, output_dir: Path) -> dict[str, Any]:
    ai_sheet = Image.open(ai_sheet_path).convert("RGBA")
    pairs = split_ai_sheet(ai_sheet, len(cases))
    rows: list[Image.Image] = []
    reports: list[dict[str, Any]] = []
    for case, (light_plate, dark_plate) in zip(cases, pairs, strict=True):
        source = crop_case(case)
        key = parse_color(case["key"])
        soft = soft_chroma_matte(source, key)
        started = perf_counter()
        dual_alpha = extract_dual_plate_alpha(
            light_plate,
            dark_plate,
            bg_light=(255, 255, 255),
            bg_dark=(0, 0, 0),
            recovery_source="average",
        )
        dual_ms = round((perf_counter() - started) * 1000, 3)
        transferred = transfer_alpha_to_source(source, dual_alpha)
        source_mask_agreement = alpha_iou(soft, transferred)

        aligned_light, aligned_dark, align_report = align_plate_pair(light_plate, dark_plate)
        aligned_started = perf_counter()
        aligned_dual_alpha = extract_dual_plate_alpha(
            aligned_light,
            aligned_dark,
            bg_light=(255, 255, 255),
            bg_dark=(0, 0, 0),
            recovery_source="average",
        )
        aligned_dual_ms = round((perf_counter() - aligned_started) * 1000, 3)
        aligned_transferred = transfer_alpha_to_source_bbox(source, aligned_dual_alpha, soft)
        aligned_source_mask_agreement = alpha_iou(soft, aligned_transferred)

        light_mask = foreground_mask_from_bg(light_plate, (255, 255, 255))
        dark_mask = foreground_mask_from_bg(dark_plate, (0, 0, 0))
        plate_pair_iou = mask_iou(light_mask, dark_mask)
        aligned_light_mask = foreground_mask_from_bg(aligned_light, (255, 255, 255))
        aligned_dark_mask = foreground_mask_from_bg(aligned_dark, (0, 0, 0))
        aligned_plate_pair_iou = mask_iou(aligned_light_mask, aligned_dark_mask)
        verdict = "review"
        best_agreement = aligned_source_mask_agreement if aligned_source_mask_agreement is not None else source_mask_agreement
        if best_agreement is not None:
            verdict = "candidate mask" if best_agreement >= 0.72 else "transfer mismatch"

        row_cells = [
            render_cell("source RGB", source, [f"case: {case['id']}"]),
            render_cell("AI white plate", light_plate, ["plate candidate"]),
            render_cell("AI black plate", dark_plate, ["plate candidate"]),
            render_cell("soft matte", soft, [f"key: {case['key']}"]),
            render_cell("raw dual transfer", transferred, [f"pair iou: {plate_pair_iou}", f"mask vs soft: {source_mask_agreement}", f"time: {dual_ms} ms"]),
            render_cell(
                "aligned transfer",
                aligned_transferred,
                [f"pair iou: {aligned_plate_pair_iou}", f"mask vs soft: {aligned_source_mask_agreement}", verdict],
            ),
        ]
        title_h = 58
        cell_w, cell_h = row_cells[0].size
        row = Image.new("RGBA", (cell_w * len(row_cells), title_h + cell_h), (12, 13, 16, 255))
        draw = ImageDraw.Draw(row)
        draw.text((12, 10), f"{case['id']} - dual-plate mask transfer", fill=(248, 241, 222, 255), font=font(18))
        draw.text((12, 34), "RGB stays from source; AI white/black plates are used only as alpha-mask candidates.", fill=(188, 181, 164, 255), font=font(11))
        for cell_index, cell in enumerate(row_cells):
            row.alpha_composite(cell, (cell_index * cell_w, title_h))
        row_path = output_dir / f"{case['id']}_dual_transfer.png"
        save_image_atomic(row, row_path)
        rows.append(row)
        reports.append(
            {
                "id": case["id"],
                "row_image": rel(row_path),
                "plate_pair_iou": plate_pair_iou,
                "source_mask_agreement_vs_soft_matte": source_mask_agreement,
                "aligned_plate_pair_iou": aligned_plate_pair_iou,
                "aligned_source_mask_agreement_vs_soft_matte": aligned_source_mask_agreement,
                "align": align_report,
                "verdict": verdict,
                "dual_plate_ms": dual_ms,
                "aligned_dual_plate_ms": aligned_dual_ms,
                "soft_matte": metrics(soft, key),
                "dual_transfer": metrics(transferred, key),
                "aligned_dual_transfer": metrics(aligned_transferred, key),
            }
        )

    width = max(row.width for row in rows)
    height = sum(row.height for row in rows) + (len(rows) - 1) * 16
    overview = Image.new("RGBA", (width, height), (8, 9, 11, 255))
    y = 0
    for row in rows:
        overview.alpha_composite(row, ((width - row.width) // 2, y))
        y += row.height + 16
    overview_path = output_dir / "dual_plate_transfer_benchmark.png"
    save_image_atomic(overview, overview_path)
    return {
        "schema": "game.dual_plate_transfer_benchmark",
        "version": 1,
        "ai_plate_sheet": rel(ai_sheet_path),
        "overview": rel(overview_path),
        "cases": reports,
    }


def markdown_report(report: dict[str, Any]) -> str:
    lines = [
        "# Dual Plate Transfer Benchmark",
        "",
        f"AI plate sheet: `{report['ai_plate_sheet']}`",
        f"Overview: `{report['overview']}`",
        "",
        "RGB is always taken from the selected source crop. AI white/black plates are used only as alpha candidates.",
        "",
        "| case | raw pair iou | raw source agreement | aligned pair iou | aligned source agreement | verdict | row |",
        "|---|---:|---:|---:|---:|---|---|",
    ]
    for case in report["cases"]:
        lines.append(
            f"| {case['id']} | {case['plate_pair_iou']} | "
            f"{case['source_mask_agreement_vs_soft_matte']} | {case['aligned_plate_pair_iou']} | "
            f"{case['aligned_source_mask_agreement_vs_soft_matte']} | {case['verdict']} | `{case['row_image']}` |"
        )
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Benchmark AI white/black plate alpha transfer onto original source RGB.")
    parser.add_argument(
        "--fixtures",
        type=Path,
        default=Path("gamedesign/projects/mine-cards/reviews/cutout_benchmark/fixtures.json"),
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("gamedesign/projects/mine-cards/reviews/cutout_benchmark/dual_plate_transfer"),
    )
    parser.add_argument("--ai-plate-sheet", type=Path)
    args = parser.parse_args()

    output_dir = project_path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    cases = load_cases(project_path(args.fixtures))
    prepare = prepare_source_reference(cases, output_dir)
    if not args.ai_plate_sheet:
        write_json_atomic(output_dir / "prepare_report.json", {"schema": "game.dual_plate_transfer_prepare", **prepare})
        print(f"wrote source reference: {prepare['source_reference_sheet']}")
        return 0

    report = process_ai_sheet(cases, project_path(args.ai_plate_sheet), output_dir)
    report.update(prepare)
    write_json_atomic(output_dir / "dual_plate_transfer_benchmark.json", report)
    write_text_atomic(output_dir / "dual_plate_transfer_benchmark.md", markdown_report(report))
    print(f"wrote overview: {report['overview']}")
    print(f"wrote report: {rel(output_dir / 'dual_plate_transfer_benchmark.md')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
