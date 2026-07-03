#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[5]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import save_image_atomic, write_json_atomic
from ai_studio.assets.tools.lib.color import format_hex, key_distance, parse_hex

RGB = tuple[int, int, int]
Rect = tuple[int, int, int, int]

# T0254: local hex parse/format duplication -> shared lib/color (parse_hex,
# format_hex). Kept as local aliases so this module's public names don't churn.
parse_color = parse_hex
format_color = format_hex


def rel(path: Path) -> str:
    resolved = path.resolve()
    try:
        return resolved.relative_to(ROOT.resolve()).as_posix()
    except ValueError:
        return resolved.as_posix()


def foreground_mask(
    image: Image.Image,
    *,
    key_color: RGB,
    key_tolerance: int,
    alpha_threshold: int,
) -> np.ndarray:
    array = np.asarray(image.convert("RGBA"), dtype=np.uint8)
    rgb = array[..., :3].astype(np.int16)
    alpha = array[..., 3]
    key_like = (alpha <= alpha_threshold) | (key_distance(rgb, key_color) <= key_tolerance)
    return np.asarray(~key_like, dtype=bool)


def connected_components(mask: np.ndarray) -> list[dict[str, Any]]:
    height, width = mask.shape
    runs: list[tuple[int, int, int, int]] = []
    parents: list[int] = []
    ranks: list[int] = []

    def make_set() -> int:
        index = len(parents)
        parents.append(index)
        ranks.append(0)
        return index

    def find(index: int) -> int:
        parent = parents[index]
        if parent != index:
            parent = find(parent)
            parents[index] = parent
        return parent

    def union(left: int, right: int) -> None:
        left_root = find(left)
        right_root = find(right)
        if left_root == right_root:
            return
        if ranks[left_root] < ranks[right_root]:
            left_root, right_root = right_root, left_root
        parents[right_root] = left_root
        if ranks[left_root] == ranks[right_root]:
            ranks[left_root] += 1

    previous_row: list[tuple[int, int, int]] = []
    for y in range(height):
        padded = np.empty(width + 2, dtype=bool)
        padded[0] = False
        padded[-1] = False
        padded[1:-1] = mask[y]
        changes = np.flatnonzero(padded[1:] != padded[:-1])
        current_row: list[tuple[int, int, int]] = []
        previous_index = 0
        for start, end in zip(changes[0::2], changes[1::2]):
            run_index = make_set()
            run_start = int(start)
            run_end = int(end)
            runs.append((y, run_start, run_end, run_index))
            current_row.append((run_start, run_end, run_index))
            while previous_index < len(previous_row) and previous_row[previous_index][1] < run_start:
                previous_index += 1
            overlap_index = previous_index
            while overlap_index < len(previous_row) and previous_row[overlap_index][0] <= run_end:
                previous_start, previous_end, previous_run_index = previous_row[overlap_index]
                if previous_end >= run_start and previous_start <= run_end:
                    union(run_index, previous_run_index)
                overlap_index += 1
        previous_row = current_row

    grouped: dict[int, dict[str, int]] = {}
    for y, start, end, run_index in runs:
        root = find(run_index)
        area = end - start
        component = grouped.get(root)
        if component is None:
            grouped[root] = {
                "min_x": start,
                "min_y": y,
                "max_x": end - 1,
                "max_y": y,
                "area_px": area,
            }
            continue
        component["min_x"] = min(component["min_x"], start)
        component["min_y"] = min(component["min_y"], y)
        component["max_x"] = max(component["max_x"], end - 1)
        component["max_y"] = max(component["max_y"], y)
        component["area_px"] += area

    components: list[dict[str, Any]] = []
    for component in grouped.values():
        min_x = component["min_x"]
        min_y = component["min_y"]
        max_x = component["max_x"]
        max_y = component["max_y"]
        components.append(
            {
                "content_bbox": [min_x, min_y, max_x - min_x + 1, max_y - min_y + 1],
                "area_px": component["area_px"],
            }
        )
    return components


def rect_gap(left: Rect, right: Rect) -> int:
    ax, ay, aw, ah = left
    bx, by, bw, bh = right
    dx = max(bx - (ax + aw), ax - (bx + bw), 0)
    dy = max(by - (ay + ah), ay - (by + bh), 0)
    if dx == 0 or dy == 0:
        return max(dx, dy)
    return int(math.ceil(math.hypot(dx, dy)))


def merge_rects(left: Rect, right: Rect) -> Rect:
    ax, ay, aw, ah = left
    bx, by, bw, bh = right
    x0 = min(ax, bx)
    y0 = min(ay, by)
    x1 = max(ax + aw, bx + bw)
    y1 = max(ay + ah, by + bh)
    return (x0, y0, x1 - x0, y1 - y0)


def merge_close_components(components: list[dict[str, Any]], distance: int) -> list[dict[str, Any]]:
    if distance <= 0:
        return components
    merged = [dict(component) for component in components]
    changed = True
    while changed:
        changed = False
        best_pair: tuple[int, int] | None = None
        best_gap: int | None = None
        for left_index, left in enumerate(merged):
            left_rect = tuple(left["content_bbox"])  # type: ignore[assignment]
            for right_index in range(left_index + 1, len(merged)):
                right = merged[right_index]
                right_rect = tuple(right["content_bbox"])  # type: ignore[assignment]
                gap = rect_gap(left_rect, right_rect)
                if gap > distance:
                    continue
                if best_gap is None or gap < best_gap:
                    best_gap = gap
                    best_pair = (left_index, right_index)
        if best_pair is None:
            continue
        left_index, right_index = best_pair
        left = merged[left_index]
        right = merged[right_index]
        rect = merge_rects(tuple(left["content_bbox"]), tuple(right["content_bbox"]))  # type: ignore[arg-type]
        combined: dict[str, Any] = {
            "content_bbox": list(rect),
            "area_px": int(left["area_px"]) + int(right["area_px"]),
        }
        merged_from = [
            *left.get("merged_from", []),
            *right.get("merged_from", []),
        ]
        if merged_from:
            combined["merged_from"] = merged_from
        merged = [component for index, component in enumerate(merged) if index not in best_pair]
        merged.append(combined)
        changed = True
    return merged


def sort_row_major(components: list[dict[str, Any]], row_tolerance: int) -> list[dict[str, Any]]:
    rows: list[list[dict[str, Any]]] = []
    row_centers: list[float] = []
    for component in sorted(components, key=lambda item: item["content_bbox"][1]):
        x, y, _width, height = component["content_bbox"]
        center_y = y + height / 2
        placed = False
        for index, row_center in enumerate(row_centers):
            if abs(center_y - row_center) <= row_tolerance:
                rows[index].append(component)
                count = len(rows[index])
                row_centers[index] = ((row_center * (count - 1)) + center_y) / count
                placed = True
                break
        if not placed:
            rows.append([component])
            row_centers.append(center_y)
    ordered: list[dict[str, Any]] = []
    for _center, row in sorted(zip(row_centers, rows), key=lambda item: item[0]):
        ordered.extend(sorted(row, key=lambda item: item["content_bbox"][0]))
    return ordered


def expand_rect(rect: Rect, padding: int, image_size: tuple[int, int]) -> list[int]:
    x, y, width, height = rect
    image_width, image_height = image_size
    left = max(0, x - padding)
    top = max(0, y - padding)
    right = min(image_width, x + width + padding)
    bottom = min(image_height, y + height + padding)
    return [left, top, right - left, bottom - top]


def detect_regions(
    source: Path,
    *,
    key_color: RGB,
    key_tolerance: int,
    alpha_threshold: int,
    min_area: int,
    merge_distance: int,
    padding: int,
    row_tolerance: int,
) -> dict[str, Any]:
    image = Image.open(source).convert("RGBA")
    mask = foreground_mask(image, key_color=key_color, key_tolerance=key_tolerance, alpha_threshold=alpha_threshold)
    components = [component for component in connected_components(mask) if int(component["area_px"]) >= min_area]
    components = merge_close_components(components, merge_distance)
    components = sort_row_major(components, row_tolerance)
    regions: list[dict[str, Any]] = []
    for index, component in enumerate(components, start=1):
        content_bbox = tuple(component["content_bbox"])
        region: dict[str, Any] = {
            "id": f"region_{index:03d}",
            "rect": expand_rect(content_bbox, padding, image.size),
            "content_bbox": list(content_bbox),
            "area_px": int(component["area_px"]),
        }
        if component.get("merged_from"):
            region["merged_from"] = component["merged_from"]
        regions.append(region)
    return {
        "schema": "ai_studio.raster2d.detected_regions.v1",
        "version": 1,
        "source": rel(source),
        "image": {
            "width": image.width,
            "height": image.height,
        },
        "detection": {
            "mode": "chroma_key",
            "key_color": format_color(key_color),
            "key_tolerance": key_tolerance,
            "alpha_threshold": alpha_threshold,
            "min_area": min_area,
            "merge_distance": merge_distance,
            "padding": padding,
            "row_tolerance": row_tolerance,
        },
        "region_count": len(regions),
        "regions": regions,
    }


def write_overlay(source: Path, output: Path, regions: list[dict[str, Any]]) -> None:
    image = Image.open(source).convert("RGBA")
    draw = ImageDraw.Draw(image)
    colors = [
        (0, 255, 255, 255),
        (255, 255, 0, 255),
        (0, 255, 0, 255),
        (255, 128, 0, 255),
        (255, 255, 255, 255),
    ]
    for index, region in enumerate(regions, start=1):
        x, y, width, height = region["rect"]
        color = colors[(index - 1) % len(colors)]
        draw.rectangle([x, y, x + width - 1, y + height - 1], outline=color, width=3)
        draw.text((x + 4, y + 4), str(index), fill=color)
    save_image_atomic(image, output)


def main() -> int:
    parser = argparse.ArgumentParser(description="Detect isolated raster regions on a chroma-key source sheet.")
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--key-color", type=parse_color, default=parse_color("#ff00ff"))
    parser.add_argument("--key-tolerance", type=int, default=16)
    parser.add_argument("--alpha-threshold", type=int, default=0)
    parser.add_argument("--min-area", type=int, default=128)
    parser.add_argument("--merge-distance", type=int, default=0)
    parser.add_argument("--padding", type=int, default=0)
    parser.add_argument("--row-tolerance", type=int, default=32)
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--overlay-output", type=Path)
    args = parser.parse_args()
    if min(args.key_tolerance, args.alpha_threshold, args.min_area, args.merge_distance, args.padding, args.row_tolerance) < 0:
        raise SystemExit("numeric thresholds must be >= 0")

    result = detect_regions(
        args.source,
        key_color=args.key_color,
        key_tolerance=args.key_tolerance,
        alpha_threshold=args.alpha_threshold,
        min_area=args.min_area,
        merge_distance=args.merge_distance,
        padding=args.padding,
        row_tolerance=args.row_tolerance,
    )
    if args.json_output:
        write_json_atomic(args.json_output, result)
    if args.overlay_output:
        write_overlay(args.source, args.overlay_output, result["regions"])
    if args.json_output:
        print(f"pass: detected {result['region_count']} region(s)")
    else:
        print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
