#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
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


def is_bad_edge_pixel(
    image: Image.Image,
    x: int,
    y: int,
    *,
    source_key: tuple[int, int, int] | None,
    preserve_purple: bool,
    preserve_green: bool,
    preserve_source_key: bool,
) -> str | None:
    pixels = image.load()
    alpha = image.getchannel("A")
    alpha_pixels = alpha.load()
    width, height = image.size
    red, green, blue, current_alpha = pixels[x, y]
    purple_bad = not preserve_purple and (is_key_fringe_like(red, green, blue) or is_any_purple_halo_like(red, green, blue))
    green_bad = not preserve_green and is_green_screen_spill_like(red, green, blue)
    source_key_bad = (
        source_key is not None
        and not preserve_source_key
        and is_source_key_spill_like(red, green, blue, source_key)
    )
    bad_color = purple_bad or green_bad or source_key_bad
    if not bad_color:
        return None
    if current_alpha > 12 and touches_transparent(alpha_pixels, x, y, width, height, 6):
        return "visible"
    if current_alpha <= 12 and near_visible(alpha_pixels, x, y, width, height):
        return "transparent_rgb"
    return None


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
) -> tuple[Image.Image, int]:
    crop = image.crop(rect).convert("RGBA")
    proof = checkerboard(crop.size)
    proof.alpha_composite(crop)
    bad_count = 0
    if mark_bad_pixels:
        draw = ImageDraw.Draw(proof)
        left, top, _right, _bottom = rect
        for y in range(top, top + crop.height):
            for x in range(left, left + crop.width):
                kind = is_bad_edge_pixel(
                    image,
                    x,
                    y,
                    source_key=source_key,
                    preserve_purple=preserve_purple,
                    preserve_green=preserve_green,
                    preserve_source_key=preserve_source_key,
                )
                if kind is None:
                    continue
                bad_count += 1
                local = (x - left, y - top)
                color = (255, 38, 38, 255) if kind == "visible" else (255, 220, 0, 255)
                draw.point(local, fill=color)
    return proof.resize((proof.width * zoom, proof.height * zoom), Image.Resampling.NEAREST), bad_count


def render_edge_proof(
    manifest: dict[str, Any],
    root: Path,
    zoom: int,
    strip: int,
    pad: int,
    mark_bad_pixels: bool,
    asset_ids: set[str] | None,
    sides_filter: set[str] | None,
) -> Image.Image:
    font = ImageFont.load_default()
    rows: list[tuple[str, Image.Image, int]] = []
    sides = [side for side in ["top", "right", "bottom", "left"] if sides_filter is None or side in sides_filter]
    source_key = parse_hex_color(manifest.get("green_screen", {}).get("key"))
    for crop in crop_entries(manifest):
        crop_id = str(crop.get("id", ""))
        if asset_ids is not None and crop_id not in asset_ids:
            continue
        output = crop.get("output")
        if not isinstance(output, str) or not output:
            continue
        path = (root / output).resolve()
        if not path.exists():
            continue
        image = Image.open(path).convert("RGBA")
        bbox = alpha_bbox(image)
        if bbox is None:
            continue
        preserve_purple = bool(crop.get("preserve_purple_edges"))
        preserve_green = bool(crop.get("preserve_green_edges"))
        preserve_source_key = bool(crop.get("preserve_source_key_edges"))
        for side in sides:
            rect = crop_rect_for_side(bbox, image.size, side, strip, pad)
            strip_image, bad_count = render_strip(
                image,
                rect,
                zoom,
                mark_bad_pixels,
                source_key=source_key,
                preserve_purple=preserve_purple,
                preserve_green=preserve_green,
                preserve_source_key=preserve_source_key,
            )
            label = f"{crop_id or path.stem} / {side} / rect={list(rect)} / bad_marks={bad_count}"
            rows.append((label, strip_image, bad_count))

    if not rows:
        return Image.new("RGBA", (480, 80), (24, 24, 24, 255))

    label_height = 18
    gutter = 12
    margin = 12
    width = max(image.width for _label, image, _bad in rows) + margin * 2
    height = margin + sum(label_height + image.height + gutter for _label, image, _bad in rows)
    sheet = Image.new("RGBA", (width, height), (24, 24, 28, 255))
    draw = ImageDraw.Draw(sheet)
    y = margin
    for label, image, bad_count in rows:
        color = (255, 120, 120, 255) if bad_count else (235, 235, 235, 255)
        draw.text((margin, y), label, font=font, fill=color)
        y += label_height
        sheet.alpha_composite(image, (margin, y))
        y += image.height + gutter
    return sheet


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
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    manifest_path = project_path(args.crop_manifest)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    proof = render_edge_proof(
        manifest,
        ROOT,
        max(1, args.zoom),
        max(1, args.strip),
        max(0, args.pad),
        not args.no_mark_bad_pixels,
        set(args.asset_id) if args.asset_id else None,
        set(args.side) if args.side else None,
    )
    output = project_path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    proof.save(output)
    print(f"wrote edge proof: {output} size={proof.width}x{proof.height}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
