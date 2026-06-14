#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from time import perf_counter
from typing import Any

from PIL import Image, ImageDraw, ImageFont


ROOT = Path.cwd()
SCRIPT_ROOT = Path(__file__).resolve().parents[2]
if str(SCRIPT_ROOT) not in sys.path:
    sys.path.insert(0, str(SCRIPT_ROOT))

from tools.assets.audit_generated_ui_assets import alpha_bbox, crop_entries, parse_hex_color, touches_transparent
from tools.assets.chroma_key_alpha import (
    is_any_purple_halo_like,
    is_green_screen_spill_like,
    is_key_fringe_like,
    is_source_key_spill_like,
)


def project_path(path: str) -> Path:
    candidate = Path(path)
    if candidate.is_absolute():
        return candidate
    return ROOT / candidate


def checkerboard(size: tuple[int, int], cell: int = 8) -> Image.Image:
    width, height = size
    image = Image.new("RGBA", size, (54, 50, 58, 255))
    draw = ImageDraw.Draw(image)
    for y in range(0, height, cell):
        for x in range(0, width, cell):
            color = (42, 38, 46, 255) if ((x // cell) + (y // cell)) % 2 else (58, 54, 62, 255)
            draw.rectangle((x, y, min(width, x + cell) - 1, min(height, y + cell) - 1), fill=color)
    return image


def crop_rect_for_side(
    bbox: tuple[int, int, int, int],
    image_size: tuple[int, int],
    side: str,
    strip: int,
    pad: int,
) -> tuple[int, int, int, int]:
    left, top, right, bottom = bbox
    width, height = image_size
    if side == "top":
        return (max(0, left - pad), max(0, top - strip), min(width, right + pad), min(height, top + strip))
    if side == "bottom":
        return (max(0, left - pad), max(0, bottom - strip), min(width, right + pad), min(height, bottom + strip))
    if side == "left":
        return (max(0, left - strip), max(0, top - pad), min(width, left + strip), min(height, bottom + pad))
    if side == "right":
        return (max(0, right - strip), max(0, top - pad), min(width, right + strip), min(height, bottom + pad))
    raise ValueError(f"unknown side: {side}")


def near_visible(alpha_pixels: Any, x: int, y: int, width: int, height: int, radius: int = 2) -> bool:
    for ny in range(max(0, y - radius), min(height, y + radius + 1)):
        for nx in range(max(0, x - radius), min(width, x + radius + 1)):
            if alpha_pixels[nx, ny] > 12:
                return True
    return False


def classify_bad_edge_pixel(
    pixels: Any,
    alpha_pixels: Any,
    image_size: tuple[int, int],
    x: int,
    y: int,
    *,
    source_key: tuple[int, int, int] | None,
    preserve_purple: bool,
    preserve_green: bool,
    preserve_source_key: bool,
) -> str | None:
    width, height = image_size
    red, green, blue, current_alpha = pixels[x, y]
    source_key_bad = (
        source_key is not None
        and not preserve_source_key
        and is_source_key_spill_like(red, green, blue, source_key)
    )
    green_bad = not preserve_green and is_green_screen_spill_like(red, green, blue)
    key_fringe_bad = not preserve_purple and is_key_fringe_like(red, green, blue)
    purple_bad = not preserve_purple and is_any_purple_halo_like(red, green, blue)
    if source_key_bad:
        reason = "source_key_spill"
    elif green_bad:
        reason = "green_screen_spill"
    elif key_fringe_bad:
        reason = "key_color_fringe"
    elif purple_bad:
        reason = "purple_halo"
    else:
        return None
    if current_alpha > 12 and touches_transparent(alpha_pixels, x, y, width, height, 6):
        return ("visible", reason)
    if current_alpha <= 12 and near_visible(alpha_pixels, x, y, width, height):
        return ("transparent_rgb", reason)
    return None


def empty_counts() -> dict[str, Any]:
    return {"total": 0, "visible": 0, "transparent_rgb": 0, "reasons": {}}


def add_bad_mark(counts: dict[str, Any], kind: str, reason: str) -> None:
    counts["total"] += 1
    counts[kind] += 1
    reasons = counts["reasons"]
    reasons[reason] = int(reasons.get(reason, 0)) + 1


def render_strip(
    image: Image.Image,
    rect: tuple[int, int, int, int],
    zoom: int,
    mark_bad_pixels: bool,
    *,
    source_key: tuple[int, int, int] | None,
    preserve_purple: bool,
    preserve_green: bool,
    preserve_source_key: bool,
) -> tuple[Image.Image, dict[str, Any]]:
    crop = image.crop(rect).convert("RGBA")
    proof = checkerboard(crop.size)
    proof.alpha_composite(crop)
    counts = empty_counts()
    draw = ImageDraw.Draw(proof) if mark_bad_pixels else None
    pixels = image.load()
    alpha = image.getchannel("A")
    alpha_pixels = alpha.load()
    image_size = image.size
    left, top, _right, _bottom = rect
    for y in range(top, top + crop.height):
        for x in range(left, left + crop.width):
            classified = classify_bad_edge_pixel(
                pixels,
                alpha_pixels,
                image_size,
                x,
                y,
                source_key=source_key,
                preserve_purple=preserve_purple,
                preserve_green=preserve_green,
                preserve_source_key=preserve_source_key,
            )
            if classified is None:
                continue
            kind, reason = classified
            add_bad_mark(counts, kind, reason)
            if draw is not None:
                local = (x - left, y - top)
                color = (255, 38, 38, 255) if kind == "visible" else (255, 220, 0, 255)
                draw.point(local, fill=color)
    return proof.resize((proof.width * zoom, proof.height * zoom), Image.Resampling.NEAREST), counts


def aggregate_counts(rows: list[dict[str, Any]]) -> dict[str, Any]:
    counts = empty_counts()
    for row in rows:
        row_counts = row["counts"]
        counts["total"] += int(row_counts.get("total", 0))
        counts["visible"] += int(row_counts.get("visible", 0))
        counts["transparent_rgb"] += int(row_counts.get("transparent_rgb", 0))
        for reason, value in row_counts.get("reasons", {}).items():
            counts["reasons"][reason] = int(counts["reasons"].get(reason, 0)) + int(value)
    return counts


def render_edge_proof(
    manifest: dict[str, Any],
    root: Path,
    zoom: int,
    strip: int,
    pad: int,
    mark_bad_pixels: bool,
    asset_ids: set[str] | None,
    sides_filter: set[str] | None,
    profile: bool = False,
) -> tuple[Image.Image, dict[str, Any]]:
    started = perf_counter()
    font = ImageFont.load_default()
    rows: list[tuple[str, Image.Image, dict[str, Any]]] = []
    report_rows: list[dict[str, Any]] = []
    timings: dict[str, float] = {}
    asset_timings: list[dict[str, Any]] = []
    render_strips_ms = 0.0
    sides = [side for side in ["top", "right", "bottom", "left"] if sides_filter is None or side in sides_filter]
    source_key = parse_hex_color(manifest.get("green_screen", {}).get("key"))
    for crop in crop_entries(manifest):
        asset_started = perf_counter()
        asset_timing: dict[str, Any] = {}
        crop_id = str(crop.get("id", ""))
        if asset_ids is not None and crop_id not in asset_ids:
            continue
        output = crop.get("output")
        if not isinstance(output, str) or not output:
            continue
        path = (root / output).resolve()
        if not path.exists():
            continue
        load_started = perf_counter()
        image = Image.open(path).convert("RGBA")
        if profile:
            asset_timing["load_image"] = round((perf_counter() - load_started) * 1000, 3)
        bbox_started = perf_counter()
        bbox = alpha_bbox(image)
        if profile:
            asset_timing["alpha_bbox"] = round((perf_counter() - bbox_started) * 1000, 3)
        if bbox is None:
            continue
        preserve_purple = bool(crop.get("preserve_purple_edges"))
        preserve_green = bool(crop.get("preserve_green_edges"))
        preserve_source_key = bool(crop.get("preserve_source_key_edges"))
        for side in sides:
            rect = crop_rect_for_side(bbox, image.size, side, strip, pad)
            strip_started = perf_counter()
            strip_image, counts = render_strip(
                image,
                rect,
                zoom,
                mark_bad_pixels,
                source_key=source_key,
                preserve_purple=preserve_purple,
                preserve_green=preserve_green,
                preserve_source_key=preserve_source_key,
            )
            strip_ms = round((perf_counter() - strip_started) * 1000, 3)
            if profile:
                render_strips_ms += strip_ms
            label = f"{crop_id or path.stem} / {side} / rect={list(rect)} / bad_marks={counts['total']}"
            rows.append((label, strip_image, counts))
            row = {
                "asset_id": crop_id,
                "kind": crop.get("kind", ""),
                "output": output,
                "side": side,
                "rect": list(rect),
                "counts": counts,
                "preserve": {
                    "purple": preserve_purple,
                    "green": preserve_green,
                    "source_key": preserve_source_key,
                },
            }
            if profile:
                row["timing_ms"] = {"render_strip": strip_ms}
            report_rows.append(row)
        if profile:
            asset_timing["total"] = round((perf_counter() - asset_started) * 1000, 3)
            asset_timings.append({"asset_id": crop_id, "output": output, "timing_ms": asset_timing})

    if not rows:
        report = {
            "schema": "game.ui_asset_edge_proof",
            "version": 1,
            "rows": [],
            "counts": empty_counts(),
        }
        if profile:
            report["timing_ms"] = {"total": round((perf_counter() - started) * 1000, 3)}
            report["asset_timings"] = asset_timings
        return Image.new("RGBA", (480, 80), (24, 24, 24, 255)), report

    compose_started = perf_counter()
    label_height = 18
    gutter = 12
    margin = 12
    width = max(image.width for _label, image, _bad in rows) + margin * 2
    height = margin + sum(label_height + image.height + gutter for _label, image, _bad in rows)
    sheet = Image.new("RGBA", (width, height), (24, 24, 28, 255))
    draw = ImageDraw.Draw(sheet)
    y = margin
    for label, image, counts in rows:
        color = (255, 120, 120, 255) if counts["total"] else (235, 235, 235, 255)
        draw.text((margin, y), label, font=font, fill=color)
        y += label_height
        sheet.alpha_composite(image, (margin, y))
        y += image.height + gutter
    if profile:
        timings = {
            "total": round((perf_counter() - started) * 1000, 3),
            "render_strips": round(render_strips_ms, 3),
            "compose_sheet": round((perf_counter() - compose_started) * 1000, 3),
        }
    report = {
        "schema": "game.ui_asset_edge_proof",
        "version": 1,
        "rows": report_rows,
        "counts": aggregate_counts(report_rows),
    }
    if profile:
        report["timing_ms"] = timings
        report["asset_timings"] = asset_timings
    return sheet, report


def render_markdown(report: dict[str, Any]) -> str:
    lines = [
        "---",
        "type: UIAssetEdgeProof",
        f"crop_manifest: {report.get('crop_manifest', '-')}",
        f"image_output: {report.get('image_output', '-')}",
        "---",
        "",
        "# UI Asset Edge Proof",
        "",
        f"Total bad marks: **{report['counts']['total']}**",
        f"Visible edge marks: {report['counts']['visible']}",
        f"Transparent RGB marks: {report['counts']['transparent_rgb']}",
        "",
    ]
    reasons = report["counts"].get("reasons", {})
    if reasons:
        lines.extend(["## Reasons", ""])
        for reason, count in sorted(reasons.items()):
            lines.append(f"- `{reason}`: {count}")
        lines.append("")
    if report.get("timing_ms"):
        lines.extend(["## Timing", "", f"Total: {report['timing_ms'].get('total', '-')} ms", ""])
        for name, elapsed in report["timing_ms"].items():
            if name == "total":
                continue
            lines.append(f"- {name}: {elapsed} ms")
        if report.get("asset_timings"):
            lines.append("")
            for asset in sorted(report["asset_timings"], key=lambda item: item.get("timing_ms", {}).get("total", 0), reverse=True)[:10]:
                timing = asset.get("timing_ms", {})
                lines.append(f"- `{asset.get('asset_id')}` total={timing.get('total', '-')} ms output={asset.get('output')}")
        lines.append("")
    lines.extend(["## Rows", ""])
    for row in report["rows"]:
        counts = row["counts"]
        lines.append(
            f"- `{row['asset_id']}` {row['side']} rect={row['rect']} "
            f"bad={counts['total']} visible={counts['visible']} "
            f"transparent_rgb={counts['transparent_rgb']} reasons={counts.get('reasons', {})}"
        )
    lines.append("")
    return "\n".join(lines)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Render zoomed edge strips for generated UI runtime PNG review.")
    parser.add_argument("--crop-manifest", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--zoom", type=int, default=4)
    parser.add_argument("--strip", type=int, default=18)
    parser.add_argument("--pad", type=int, default=6)
    parser.add_argument("--asset-id", action="append", help="Limit proof to one asset id; can be repeated.")
    parser.add_argument("--side", action="append", choices=["top", "right", "bottom", "left"], help="Limit proof to one side; can be repeated.")
    parser.add_argument("--no-mark-bad-pixels", action="store_true")
    parser.add_argument("--json-output", help="Write structured edge-proof counts by asset side and defect class.")
    parser.add_argument("--report", help="Write a Markdown edge-proof report next to the proof image.")
    parser.add_argument("--profile", action="store_true", help="Record edge-proof timing in JSON/Markdown and print the slowest asset side.")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    manifest_path = project_path(args.crop_manifest)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    proof, report = render_edge_proof(
        manifest,
        ROOT,
        max(1, args.zoom),
        max(1, args.strip),
        max(0, args.pad),
        not args.no_mark_bad_pixels,
        set(args.asset_id) if args.asset_id else None,
        set(args.side) if args.side else None,
        args.profile,
    )
    output = project_path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    proof.save(output)
    report["crop_manifest"] = args.crop_manifest.replace("\\", "/")
    report["image_output"] = args.output.replace("\\", "/")
    if args.json_output:
        json_output = project_path(args.json_output)
        json_output.parent.mkdir(parents=True, exist_ok=True)
        json_output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if args.report:
        report_output = project_path(args.report)
        report_output.parent.mkdir(parents=True, exist_ok=True)
        report_output.write_text(render_markdown(report), encoding="utf-8")
    print(f"wrote edge proof: {output} size={proof.width}x{proof.height}")
    if args.json_output or args.report:
        print(f"edge proof marks: total={report['counts']['total']} reasons={report['counts']['reasons']}")
    if args.profile and report.get("rows"):
        slowest = max(report["rows"], key=lambda row: row.get("timing_ms", {}).get("render_strip", 0))
        print(
            "profile: slowest edge strip "
            f"`{slowest.get('asset_id')}` {slowest.get('side')} "
            f"{slowest.get('timing_ms', {}).get('render_strip', 0)} ms"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
