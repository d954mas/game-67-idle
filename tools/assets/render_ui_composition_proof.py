#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from time import perf_counter
from typing import Any

from PIL import Image, ImageDraw, ImageFont


SCRIPT_ROOT = Path(__file__).resolve().parents[2]
if str(SCRIPT_ROOT) not in sys.path:
    sys.path.insert(0, str(SCRIPT_ROOT))

from tools.assets.atomic_io import save_image_atomic, write_text_atomic
from tools.assets.chroma_key_alpha import resize_rgba_premultiplied


ROOT = Path.cwd()
ANCHORS = {
    "top_left",
    "top_center",
    "top_right",
    "bottom_left",
    "bottom_center",
    "bottom_right",
    "left_mid",
    "right_mid",
    "center",
}


class CompositionRenderCache:
    def __init__(self) -> None:
        self.images: dict[str, Image.Image] = {}
        self.slice_tiles: dict[tuple[str, tuple[int, int, int, int], tuple[int, int]], list[list[Image.Image]]] = {}
        self.resized_tiles: dict[tuple[str, tuple[int, int, int, int], tuple[int, int], int, int, tuple[int, int]], Image.Image] = {}
        self.panels: dict[tuple[str, tuple[int, int], tuple[int, int, int, int], tuple[int, int]], Image.Image] = {}
        self.resized_overlays: dict[tuple[str, tuple[int, int], tuple[int, int]], Image.Image] = {}
        self.stats = {
            "image_hits": 0,
            "image_misses": 0,
            "slice_tile_hits": 0,
            "slice_tile_misses": 0,
            "resized_tile_hits": 0,
            "resized_tile_misses": 0,
            "panel_hits": 0,
            "panel_misses": 0,
            "overlay_resize_hits": 0,
            "overlay_resize_misses": 0,
        }


def project_path(path: str | Path) -> Path:
    candidate = Path(path)
    if candidate.is_absolute():
        return candidate
    return ROOT / candidate


def normalize_path(path: str | Path) -> str:
    try:
        return project_path(path).relative_to(ROOT).as_posix()
    except ValueError:
        return project_path(path).as_posix()


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_text(path: Path, text: str) -> None:
    write_text_atomic(path, text)


def checkerboard(size: tuple[int, int], cell: int = 16) -> Image.Image:
    width, height = size
    image = Image.new("RGBA", size, (30, 32, 38, 255))
    draw = ImageDraw.Draw(image)
    for y in range(0, height, cell):
        for x in range(0, width, cell):
            color = (37, 39, 46, 255) if ((x // cell) + (y // cell)) % 2 else (48, 50, 58, 255)
            draw.rectangle((x, y, min(width, x + cell) - 1, min(height, y + cell) - 1), fill=color)
    return image


def font(size: int = 14) -> ImageFont.ImageFont:
    for name in ("arial.ttf", "DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def text_size(draw: ImageDraw.ImageDraw, text: str, text_font: ImageFont.ImageFont) -> tuple[int, int]:
    bbox = draw.textbbox((0, 0), text, font=text_font)
    return bbox[2] - bbox[0], bbox[3] - bbox[1]


def rects_intersect(a: tuple[int, int, int, int], b: tuple[int, int, int, int]) -> bool:
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return ax < bx + bw and ax + aw > bx and ay < by + bh and ay + ah > by


def margins_key(margins: dict[str, Any]) -> tuple[int, int, int, int]:
    return (int(margins["left"]), int(margins["top"]), int(margins["right"]), int(margins["bottom"]))


def nine_slice_source_tiles(
    image: Image.Image,
    margins: dict[str, Any],
    *,
    asset_id: str | None = None,
    cache: CompositionRenderCache | None = None,
) -> list[list[Image.Image]]:
    left, top, right, bottom = margins_key(margins)
    source_width, source_height = image.size
    cache_key = (asset_id or "", (left, top, right, bottom), image.size)
    if cache is not None and asset_id and cache_key in cache.slice_tiles:
        cache.stats["slice_tile_hits"] += 1
        return cache.slice_tiles[cache_key]
    if cache is not None and asset_id:
        cache.stats["slice_tile_misses"] += 1
    source_x = [0, left, source_width - right, source_width]
    source_y = [0, top, source_height - bottom, source_height]
    tiles = [
        [image.crop((source_x[col], source_y[row], source_x[col + 1], source_y[row + 1])) for col in range(3)]
        for row in range(3)
    ]
    if cache is not None and asset_id:
        cache.slice_tiles[cache_key] = tiles
    return tiles


def nine_slice_resize(
    image: Image.Image,
    margins: dict[str, Any],
    size: tuple[int, int],
    *,
    asset_id: str | None = None,
    cache: CompositionRenderCache | None = None,
) -> Image.Image:
    left = int(margins["left"])
    top = int(margins["top"])
    right = int(margins["right"])
    bottom = int(margins["bottom"])
    out_width, out_height = size
    panel_key = (asset_id or "", size, (left, top, right, bottom), image.size)
    if cache is not None and asset_id and panel_key in cache.panels:
        cache.stats["panel_hits"] += 1
        return cache.panels[panel_key].copy()
    if cache is not None and asset_id:
        cache.stats["panel_misses"] += 1
    result = Image.new("RGBA", size, (0, 0, 0, 0))
    dest_x = [0, left, out_width - right, out_width]
    dest_y = [0, top, out_height - bottom, out_height]
    source_tiles = nine_slice_source_tiles(image, margins, asset_id=asset_id, cache=cache)
    margin_key = (left, top, right, bottom)
    for row in range(3):
        for col in range(3):
            dest_box = (dest_x[col], dest_y[row], dest_x[col + 1], dest_y[row + 1])
            dest_width = max(1, dest_box[2] - dest_box[0])
            dest_height = max(1, dest_box[3] - dest_box[1])
            tile = source_tiles[row][col]
            if tile.size != (dest_width, dest_height):
                resized_key = (asset_id or "", margin_key, image.size, row, col, (dest_width, dest_height))
                if cache is not None and asset_id and resized_key in cache.resized_tiles:
                    cache.stats["resized_tile_hits"] += 1
                    tile = cache.resized_tiles[resized_key]
                else:
                    if cache is not None and asset_id:
                        cache.stats["resized_tile_misses"] += 1
                    tile = resize_rgba_premultiplied(tile, (dest_width, dest_height))
                    if cache is not None and asset_id:
                        cache.resized_tiles[resized_key] = tile
            result.alpha_composite(tile, (dest_box[0], dest_box[1]))
    if cache is not None and asset_id:
        cache.panels[panel_key] = result.copy()
    return result


def scaled_axis(start: int, length: int, source_size: int, target_size: int, start_margin: int, end_margin: int) -> tuple[int, int]:
    delta = target_size - source_size
    fixed_end = source_size - end_margin
    end = start + length

    def map_position(position: int) -> int:
        if position <= start_margin:
            return position
        if position >= fixed_end:
            return position + delta
        source_center = max(1, fixed_end - start_margin)
        target_center = max(1, target_size - start_margin - end_margin)
        return start_margin + round((position - start_margin) * target_center / source_center)

    scaled_start = map_position(start)
    scaled_end = map_position(end)
    return scaled_start, max(1, scaled_end - scaled_start)


def content_rect_for_size(asset: dict[str, Any], source_size: tuple[int, int], target_size: tuple[int, int]) -> tuple[int, int, int, int]:
    content = asset.get("content") or asset.get("content_rect")
    if not isinstance(content, dict):
        return (8, 8, max(1, target_size[0] - 16), max(1, target_size[1] - 16))
    margins = asset["slice9"]
    x, width = scaled_axis(int(content["x"]), int(content["w"]), source_size[0], target_size[0], int(margins["left"]), int(margins["right"]))
    y, height = scaled_axis(int(content["y"]), int(content["h"]), source_size[1], target_size[1], int(margins["top"]), int(margins["bottom"]))
    x = max(0, min(target_size[0] - 1, x))
    y = max(0, min(target_size[1] - 1, y))
    width = max(1, min(target_size[0] - x, width))
    height = max(1, min(target_size[1] - y, height))
    return x, y, width, height


def anchor_position(anchor: str, base_size: tuple[int, int], overlay_size: tuple[int, int], offset: tuple[int, int]) -> tuple[int, int]:
    base_width, base_height = base_size
    overlay_width, overlay_height = overlay_size
    if anchor == "top_left":
        x, y = 0, 0
    elif anchor == "top_center":
        x, y = (base_width - overlay_width) // 2, 0
    elif anchor == "top_right":
        x, y = base_width - overlay_width, 0
    elif anchor == "bottom_left":
        x, y = 0, base_height - overlay_height
    elif anchor == "bottom_center":
        x, y = (base_width - overlay_width) // 2, base_height - overlay_height
    elif anchor == "bottom_right":
        x, y = base_width - overlay_width, base_height - overlay_height
    elif anchor == "left_mid":
        x, y = 0, (base_height - overlay_height) // 2
    elif anchor == "right_mid":
        x, y = base_width - overlay_width, (base_height - overlay_height) // 2
    else:
        x, y = (base_width - overlay_width) // 2, (base_height - overlay_height) // 2
    return x + offset[0], y + offset[1]


def positive_pair(value: Any) -> tuple[int, int] | None:
    if not isinstance(value, list) or len(value) != 2:
        return None
    try:
        width = int(value[0])
        height = int(value[1])
    except (TypeError, ValueError):
        return None
    if width <= 0 or height <= 0:
        return None
    return width, height


def resized_overlay_image(
    overlay_id: str,
    overlay_data: dict[str, Any],
    image: Image.Image,
    problems: list[str],
    cache: CompositionRenderCache | None = None,
) -> tuple[Image.Image, dict[str, Any]]:
    source_size = image.size
    requested_size = overlay_data.get("size")
    requested_max_size = overlay_data.get("max_size")
    requested_scale = overlay_data.get("scale")
    resize_keys = [key for key, value in (("size", requested_size), ("max_size", requested_max_size), ("scale", requested_scale)) if value is not None]
    if len(resize_keys) > 1:
        problems.append(f"overlay {overlay_id} must use only one of size, max_size, or scale")
        return image, {"mode": "source", "source_size": list(source_size), "render_size": list(source_size)}

    target_size = source_size
    mode = "source"
    if requested_size is not None:
        parsed = positive_pair(requested_size)
        if parsed is None:
            problems.append(f"overlay {overlay_id} size must be [width,height] with positive integers")
        else:
            target_size = parsed
            mode = "size"
    elif requested_max_size is not None:
        parsed = positive_pair(requested_max_size)
        if parsed is None:
            problems.append(f"overlay {overlay_id} max_size must be [width,height] with positive integers")
        else:
            max_width, max_height = parsed
            scale = min(1.0, max_width / source_size[0], max_height / source_size[1])
            target_size = (max(1, round(source_size[0] * scale)), max(1, round(source_size[1] * scale)))
            mode = "max_size"
    elif requested_scale is not None:
        try:
            scale = float(requested_scale)
        except (TypeError, ValueError):
            scale = 0
        if scale <= 0:
            problems.append(f"overlay {overlay_id} scale must be a positive number")
        else:
            target_size = (max(1, round(source_size[0] * scale)), max(1, round(source_size[1] * scale)))
            mode = "scale"

    report = {"mode": mode, "source_size": list(source_size), "render_size": list(target_size)}
    if target_size == source_size:
        return image, report
    cache_key = (overlay_id, source_size, target_size)
    if cache is not None and cache_key in cache.resized_overlays:
        cache.stats["overlay_resize_hits"] += 1
        return cache.resized_overlays[cache_key], report
    if cache is not None:
        cache.stats["overlay_resize_misses"] += 1
    resized = resize_rgba_premultiplied(image, target_size)
    if cache is not None:
        cache.resized_overlays[cache_key] = resized
    return resized, report


def asset_map(manifest: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {str(asset.get("id")): asset for asset in manifest.get("assets", []) if asset.get("id")}


def default_label(asset: dict[str, Any]) -> str:
    if asset.get("kind") == "slice9" and "panel" in str(asset.get("id", "")):
        return "Quest"
    asset_id = str(asset.get("id", ""))
    if asset.get("state") == "disabled":
        return "Lock" if "short" in asset_id else "Locked"
    if "short" in asset_id:
        return "Map"
    if "medium" in asset_id:
        return "Travel"
    return "Explore"


def default_layout(manifest: dict[str, Any]) -> dict[str, Any]:
    items = []
    for asset in manifest.get("assets", []):
        if asset.get("kind") != "slice9":
            continue
        sizes = asset.get("target_preview_sizes") or asset.get("preview_sizes") or []
        for size in sizes:
            if isinstance(size, list) and len(size) == 2:
                items.append({"base_id": asset["id"], "size": size, "label": default_label(asset)})
    return {
        "schema": "game.ui_composition_proof_layout",
        "version": 1,
        "title": "Default Runtime UI Composition Proof",
        "items": items,
    }


def load_image(asset: dict[str, Any], problems: list[str], cache: CompositionRenderCache | None = None) -> Image.Image | None:
    asset_id = str(asset.get("id") or "")
    if cache is not None and asset_id in cache.images:
        cache.stats["image_hits"] += 1
        return cache.images[asset_id]
    path_value = asset.get("path") or asset.get("output")
    if not isinstance(path_value, str) or not path_value:
        problems.append(f"asset {asset.get('id', '(unknown)')} needs path")
        return None
    path = project_path(path_value)
    if not path.exists():
        problems.append(f"asset {asset.get('id', '(unknown)')} image missing: {normalize_path(path)}")
        return None
    image = Image.open(path).convert("RGBA")
    if cache is not None and asset_id:
        cache.images[asset_id] = image
        cache.stats["image_misses"] += 1
    return image


def draw_content_rect(draw: ImageDraw.ImageDraw, rect: tuple[int, int, int, int]) -> None:
    x, y, width, height = rect
    draw.rectangle((x, y, x + width - 1, y + height - 1), outline=(0, 215, 255, 160), width=1)


def render_item(
    item: dict[str, Any],
    assets: dict[str, dict[str, Any]],
    text_font: ImageFont.ImageFont,
    cache: CompositionRenderCache | None = None,
    profile: bool = False,
) -> tuple[Image.Image | None, dict[str, Any]]:
    started = perf_counter()
    problems: list[str] = []
    base_id = item.get("base_id")
    base = assets.get(str(base_id))
    if not base:
        report = {"base_id": base_id, "status": "fail", "problems": [f"base asset missing: {base_id}"]}
        if profile:
            report["timing_ms"] = {"total": round((perf_counter() - started) * 1000, 3)}
        return None, report
    if base.get("kind") != "slice9":
        problems.append(f"base {base_id} must be kind=slice9")
    base_image = load_image(base, problems, cache)
    size = item.get("size")
    if not isinstance(size, list) or len(size) != 2:
        problems.append(f"item {base_id} needs size [width,height]")
        size = [base_image.width if base_image else 1, base_image.height if base_image else 1]
    target_size = (max(1, int(size[0])), max(1, int(size[1])))
    margins = base.get("slice9")
    if not isinstance(margins, dict):
        problems.append(f"base {base_id} needs slice9 margins")
        panel = Image.new("RGBA", target_size, (0, 0, 0, 0))
    elif int(margins.get("left", 0)) + int(margins.get("right", 0)) >= target_size[0] or int(margins.get("top", 0)) + int(margins.get("bottom", 0)) >= target_size[1]:
        problems.append(f"base {base_id} size {target_size[0]}x{target_size[1]} is smaller than slice9 margins")
        panel = Image.new("RGBA", target_size, (0, 0, 0, 0))
    elif base_image:
        panel = nine_slice_resize(base_image, margins, target_size, asset_id=str(base_id), cache=cache)
    else:
        panel = Image.new("RGBA", target_size, (0, 0, 0, 0))

    usage = base.get("usage_policy")
    if isinstance(usage, dict) and isinstance(usage.get("min_size"), list) and len(usage["min_size"]) == 2:
        min_width, min_height = usage["min_size"]
        if target_size[0] < int(min_width) or target_size[1] < int(min_height):
            problems.append(f"base {base_id} size {target_size[0]}x{target_size[1]} is below usage_policy.min_size {min_width}x{min_height}")

    content_rect = content_rect_for_size(base, base_image.size if base_image else target_size, target_size)
    overlay_reports: list[dict[str, Any]] = []
    overlay_specs = []
    overlay_specs.extend(item.get("decor_overlays") or [])
    overlay_specs.extend(item.get("state_overlays") or [])
    overlay_specs.extend(item.get("overlays") or [])
    for overlay_spec in overlay_specs:
        overlay_data = overlay_spec if isinstance(overlay_spec, dict) else {"id": overlay_spec}
        overlay_id = overlay_data.get("id")
        overlay = assets.get(str(overlay_id))
        if not overlay:
            problems.append(f"overlay asset missing: {overlay_id}")
            continue
        overlay_image = load_image(overlay, problems, cache)
        if overlay_image is None:
            continue
        source_size = overlay_image.size
        overlay_image, resize_report = resized_overlay_image(str(overlay_id), overlay_data, overlay_image, problems, cache)
        anchor = str(overlay_data.get("anchor") or overlay.get("anchor") or "center")
        if anchor not in ANCHORS:
            problems.append(f"overlay {overlay_id} has unsupported anchor {anchor}")
            anchor = "center"
        offset_value = overlay_data.get("offset") or overlay.get("offset") or [0, 0]
        if not isinstance(offset_value, list) or len(offset_value) != 2:
            problems.append(f"overlay {overlay_id} offset must be [x,y]")
            offset_value = [0, 0]
        x, y = anchor_position(anchor, target_size, overlay_image.size, (int(offset_value[0]), int(offset_value[1])))
        overlay_rect = (x, y, overlay_image.width, overlay_image.height)
        if x < 0 or y < 0 or x + overlay_image.width > target_size[0] or y + overlay_image.height > target_size[1]:
            problems.append(f"overlay {overlay_id} at {x},{y} falls outside base {base_id} {target_size[0]}x{target_size[1]}")
        allow_content_overlap = bool(overlay_data.get("allow_content_overlap") or overlay.get("allow_content_overlap"))
        if not allow_content_overlap and rects_intersect(overlay_rect, content_rect):
            problems.append(f"overlay {overlay_id} rect {list(overlay_rect)} overlaps content rect {list(content_rect)} for {base_id}")
        panel.alpha_composite(overlay_image, (x, y))
        overlay_reports.append(
            {
                "id": overlay_id,
                "anchor": anchor,
                "offset": [int(offset_value[0]), int(offset_value[1])],
                "source_size": list(source_size),
                "render_size": [overlay_image.width, overlay_image.height],
                "resize": resize_report,
                "rect": list(overlay_rect),
                "allow_content_overlap": allow_content_overlap,
            }
        )

    draw = ImageDraw.Draw(panel)
    draw_content_rect(draw, content_rect)
    label = str(item.get("label") or "")
    if label:
        text_width, text_height = text_size(draw, label, text_font)
        content_x, content_y, content_width, content_height = content_rect
        if text_width > content_width - 8 or text_height > content_height - 4:
            problems.append(f"label for {base_id} does not fit content rect {content_width}x{content_height}: {label}")
        text_x = content_x + max(0, (content_width - text_width) // 2)
        text_y = content_y + max(0, (content_height - text_height) // 2)
        draw.text((text_x + 1, text_y + 1), label, font=text_font, fill=(0, 0, 0, 180))
        draw.text((text_x, text_y), label, font=text_font, fill=(245, 239, 220, 255))

    report = {
        "base_id": base_id,
        "size": list(target_size),
        "label": label,
        "content_rect": list(content_rect),
        "overlays": overlay_reports,
        "status": "pass" if not problems else "fail",
        "problems": problems,
    }
    if profile:
        report["timing_ms"] = {"total": round((perf_counter() - started) * 1000, 3)}
    return panel, report


def render_sheet(rendered: list[tuple[dict[str, Any], Image.Image]], title: str) -> Image.Image:
    title_font = font(18)
    label_font = font(12)
    margin = 18
    gutter = 18
    cols = 3
    cell_width = max(260, max((image.width for _report, image in rendered), default=240) + 32)
    cell_height = max(150, max((image.height for _report, image in rendered), default=80) + 54)
    rows = max(1, (len(rendered) + cols - 1) // cols)
    sheet = checkerboard((margin * 2 + cols * cell_width, margin * 2 + 34 + rows * cell_height), 18)
    draw = ImageDraw.Draw(sheet)
    draw.text((margin, margin), title, font=title_font, fill=(245, 239, 220, 255))
    start_y = margin + 34
    for index, (report, image) in enumerate(rendered):
        col = index % cols
        row = index // cols
        x = margin + col * cell_width
        y = start_y + row * cell_height
        outline = (92, 174, 120, 255) if report["status"] == "pass" else (230, 82, 82, 255)
        draw.rectangle((x, y, x + cell_width - 10, y + cell_height - 10), outline=outline, width=2)
        sheet.alpha_composite(image, (x + (cell_width - 10 - image.width) // 2, y + 16))
        label = f"{report['base_id']} {report['size'][0]}x{report['size'][1]}"
        draw.text((x + 10, y + cell_height - 28), label, font=label_font, fill=(235, 232, 222, 255))
    return sheet


def make_markdown(report: dict[str, Any]) -> str:
    lines = [
        "# UI Composition Proof",
        "",
        f"asset_manifest: `{report['asset_manifest']}`",
        f"output: `{report['output']}`",
        f"verdict: **{report['verdict']}**",
        "",
    ]
    if report.get("timing_ms"):
        lines.extend(["## Timing", ""])
        for name, elapsed in report["timing_ms"].items():
            lines.append(f"- {name}: {elapsed} ms")
        lines.append("")
    if report.get("cache_stats"):
        lines.extend(["## Cache", ""])
        for name, value in report["cache_stats"].items():
            lines.append(f"- {name}: {value}")
        lines.append("")
    lines.extend(["## Items", ""])
    for item in report["items"]:
        suffix = f": {'; '.join(item['problems'])}" if item["problems"] else ""
        lines.append(f"- {item['status'].upper()} `{item['base_id']}` {item['size'][0]}x{item['size'][1]}{suffix}")
        for overlay in item.get("overlays", []):
            resize = overlay.get("resize") if isinstance(overlay.get("resize"), dict) else {}
            lines.append(
                "  "
                f"- overlay `{overlay.get('id')}` "
                f"source={overlay.get('source_size')} render={overlay.get('render_size')} "
                f"mode={resize.get('mode', 'source')} rect={overlay.get('rect')} "
                f"anchor={overlay.get('anchor')}"
            )
    return "\n".join(lines) + "\n"


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Render runtime composition proof for slice9 UI assets, overlays, states, and runtime labels.")
    parser.add_argument("--asset-manifest", required=True)
    parser.add_argument("--layout", help="Optional game.ui_composition_proof_layout JSON. Defaults to every slice9 target preview size.")
    parser.add_argument("--output", required=True)
    parser.add_argument("--json-output")
    parser.add_argument("--report")
    parser.add_argument("--no-fail", action="store_true", help="Write proof even when composition problems are found.")
    parser.add_argument("--profile", action="store_true", help="Record composition proof timing in JSON/Markdown and print the slowest item.")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    started = perf_counter()
    args = parse_args(argv)
    manifest_path = project_path(args.asset_manifest)
    if not manifest_path.exists():
        print(f"error: asset manifest not found: {normalize_path(manifest_path)}", file=sys.stderr)
        return 2
    manifest = read_json(manifest_path)
    if manifest.get("schema") != "game.asset_manifest":
        print(f"error: unsupported asset manifest schema: {manifest.get('schema')}", file=sys.stderr)
        return 2
    layout_path = project_path(args.layout) if args.layout else None
    layout = read_json(layout_path) if layout_path else default_layout(manifest)
    items = layout.get("items") or []
    assets = asset_map(manifest)
    proof_font = font(15)
    cache = CompositionRenderCache()
    rendered: list[tuple[dict[str, Any], Image.Image]] = []
    reports: list[dict[str, Any]] = []
    if not items:
        reports.append({"base_id": "(none)", "size": [0, 0], "label": "", "content_rect": [0, 0, 0, 0], "status": "fail", "problems": ["layout has no items"]})
    render_started = perf_counter()
    for item in items:
        image, item_report = render_item(item, assets, proof_font, cache, args.profile)
        reports.append(item_report)
        if image is not None:
            rendered.append((item_report, image))
    render_ms = round((perf_counter() - render_started) * 1000, 3)
    if not rendered:
        rendered.append(({"base_id": "(none)", "size": [320, 80], "status": "fail"}, Image.new("RGBA", (320, 80), (70, 24, 24, 255))))
    output = project_path(args.output)
    sheet_started = perf_counter()
    sheet = render_sheet(rendered, str(layout.get("title") or "Runtime UI Composition Proof"))
    sheet_ms = round((perf_counter() - sheet_started) * 1000, 3)
    save_started = perf_counter()
    save_image_atomic(sheet, output)
    save_ms = round((perf_counter() - save_started) * 1000, 3)
    verdict = "pass" if all(item["status"] == "pass" for item in reports) else "fail"
    report = {
        "schema": "game.ui_composition_proof",
        "version": 1,
        "asset_manifest": normalize_path(manifest_path),
        "layout": normalize_path(layout_path) if layout_path else None,
        "output": normalize_path(output),
        "verdict": verdict,
        "items": reports,
    }
    if args.profile:
        report["timing_ms"] = {
            "render_items": render_ms,
            "render_sheet": sheet_ms,
            "save_output": save_ms,
            "total": round((perf_counter() - started) * 1000, 3),
        }
        report["cache_stats"] = dict(cache.stats)
    if args.json_output:
        write_text(project_path(args.json_output), json.dumps(report, indent=2) + "\n")
    if args.report:
        write_text(project_path(args.report), make_markdown(report))
    print(f"{verdict}: wrote composition proof: {normalize_path(output)} items={len(reports)}")
    if args.profile and reports:
        slowest = max(reports, key=lambda item: item.get("timing_ms", {}).get("total", 0))
        print(f"profile: slowest composition item `{slowest.get('base_id')}` {slowest.get('timing_ms', {}).get('total', 0)} ms")
    if verdict != "pass" and not args.no_fail:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
