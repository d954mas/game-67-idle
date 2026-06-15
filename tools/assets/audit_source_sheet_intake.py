#!/usr/bin/env python3
from __future__ import annotations

import argparse
import colorsys
import json
import math
import sys
from collections import deque
from pathlib import Path
from time import perf_counter

from PIL import Image

try:
    import numpy as np
except ImportError:  # pragma: no cover - fallback keeps the tool portable.
    np = None


ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.assets.atomic_io import write_json_atomic, write_text_atomic
from tools.assets.chroma_key_alpha import is_exact_key_like, is_key_fringe_like, is_purple_halo_like


DEFAULT_CANDIDATE_KEY_COLORS = "#00ff00,#00ffff,#ff00ff,#ffff00,#ff0000,#0000ff"


def parse_color(value: str) -> tuple[int, int, int]:
    text = value.strip().lstrip("#")
    if len(text) != 6:
        raise argparse.ArgumentTypeError("color must be #rrggbb")
    return tuple(int(text[index : index + 2], 16) for index in (0, 2, 4))


def parse_color_list(value: str) -> list[tuple[int, int, int]]:
    colors = [parse_color(item) for item in value.split(",") if item.strip()]
    if not colors:
        raise argparse.ArgumentTypeError("at least one color is required")
    return colors


def format_color(value: tuple[int, int, int]) -> str:
    return "#{:02x}{:02x}{:02x}".format(*value)


def is_key(pixel: tuple[int, int, int, int], key: tuple[int, int, int], tolerance: int) -> bool:
    red, green, blue, alpha = pixel
    return alpha == 0 or max(abs(red - key[0]), abs(green - key[1]), abs(blue - key[2])) <= tolerance


def find_components(
    image: Image.Image,
    key: tuple[int, int, int],
    tolerance: int,
    array: object | None = None,
) -> list[dict[str, object]]:
    if np is not None:
        return find_components_numpy(image, key, tolerance, array)
    return find_components_python(image, key, tolerance)


def find_components_python(image: Image.Image, key: tuple[int, int, int], tolerance: int) -> list[dict[str, object]]:
    rgba = image.convert("RGBA")
    width, height = rgba.size
    pixels = rgba.load()
    visited = bytearray(width * height)
    components: list[dict[str, object]] = []

    def offset(x: int, y: int) -> int:
        return y * width + x

    for y in range(height):
        for x in range(width):
            index = offset(x, y)
            if visited[index] or is_key(pixels[x, y], key, tolerance):
                visited[index] = 1
                continue
            queue: deque[tuple[int, int]] = deque([(x, y)])
            visited[index] = 1
            min_x = max_x = x
            min_y = max_y = y
            area = 0
            pixel_offsets: list[int] = []
            while queue:
                cx, cy = queue.popleft()
                area += 1
                pixel_offsets.append(offset(cx, cy))
                min_x = min(min_x, cx)
                max_x = max(max_x, cx)
                min_y = min(min_y, cy)
                max_y = max(max_y, cy)
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if nx < 0 or ny < 0 or nx >= width or ny >= height:
                        continue
                    ni = offset(nx, ny)
                    if visited[ni]:
                        continue
                    visited[ni] = 1
                    if not is_key(pixels[nx, ny], key, tolerance):
                        queue.append((nx, ny))
            components.append(
                {
                    "id": f"component_{len(components) + 1}",
                    "bbox": [min_x, min_y, max_x - min_x + 1, max_y - min_y + 1],
                    "area_px": area,
                    "_pixel_offsets": pixel_offsets,
                }
            )
    return components


def find_components_numpy(
    image: Image.Image,
    key: tuple[int, int, int],
    tolerance: int,
    array: object | None = None,
) -> list[dict[str, object]]:
    if np is None:
        raise RuntimeError("numpy is required for find_components_numpy")
    array = np.asarray(image.convert("RGBA")) if array is None else array
    height, width = array.shape[:2]
    art_mask = np.asarray(~key_like_mask(array, key, tolerance), dtype=bool)
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
        row = art_mask[y]
        padded = np.empty(width + 2, dtype=bool)
        padded[0] = False
        padded[-1] = False
        padded[1:-1] = row
        changes = np.flatnonzero(padded[1:] != padded[:-1])
        current_row: list[tuple[int, int, int]] = []
        previous_index = 0
        for start, end in zip(changes[0::2], changes[1::2]):
            run_index = make_set()
            run_start = int(start)
            run_end = int(end)
            runs.append((y, run_start, run_end, run_index))
            current_row.append((run_start, run_end, run_index))
            while previous_index < len(previous_row) and previous_row[previous_index][1] <= run_start:
                previous_index += 1
            overlap_index = previous_index
            while overlap_index < len(previous_row) and previous_row[overlap_index][0] < run_end:
                previous_start, previous_end, previous_run_index = previous_row[overlap_index]
                if previous_end > run_start and previous_start < run_end:
                    union(run_index, previous_run_index)
                overlap_index += 1
        previous_row = current_row

    grouped: dict[int, dict[str, object]] = {}
    for y, start, end, run_index in runs:
        root = find(run_index)
        area = end - start
        row_offset = y * width
        component = grouped.get(root)
        if component is None:
            component = {
                "min_x": start,
                "min_y": y,
                "max_x": end - 1,
                "max_y": y,
                "area_px": area,
                "_pixel_offsets": list(range(row_offset + start, row_offset + end)),
            }
            grouped[root] = component
            continue
        component["min_x"] = min(int(component["min_x"]), start)
        component["min_y"] = min(int(component["min_y"]), y)
        component["max_x"] = max(int(component["max_x"]), end - 1)
        component["max_y"] = max(int(component["max_y"]), y)
        component["area_px"] = int(component["area_px"]) + area
        offsets = component["_pixel_offsets"]
        if isinstance(offsets, list):
            offsets.extend(range(row_offset + start, row_offset + end))

    components: list[dict[str, object]] = []
    for component in sorted(grouped.values(), key=lambda item: (int(item["min_y"]), int(item["min_x"]))):
        min_x = int(component["min_x"])
        min_y = int(component["min_y"])
        max_x = int(component["max_x"])
        max_y = int(component["max_y"])
        components.append(
            {
                "id": f"component_{len(components) + 1}",
                "bbox": [min_x, min_y, max_x - min_x + 1, max_y - min_y + 1],
                "area_px": int(component["area_px"]),
                "_pixel_offsets": component["_pixel_offsets"],
            }
        )
    return components


def find_border_connected_key(image: Image.Image, key: tuple[int, int, int], tolerance: int) -> bytearray:
    width, height = image.size
    pixels = image.load()
    connected = bytearray(width * height)
    queue: deque[tuple[int, int]] = deque()

    def offset(x: int, y: int) -> int:
        return y * width + x

    def push_if_key(x: int, y: int) -> None:
        index = offset(x, y)
        if connected[index] or not is_key(pixels[x, y], key, tolerance):
            return
        connected[index] = 1
        queue.append((x, y))

    for x in range(width):
        push_if_key(x, 0)
        push_if_key(x, height - 1)
    for y in range(height):
        push_if_key(0, y)
        push_if_key(width - 1, y)

    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < width and 0 <= ny < height:
                push_if_key(nx, ny)
    return connected


def gap_between(a: dict[str, object], b: dict[str, object]) -> int:
    ax, ay, aw, ah = a["bbox"]
    bx, by, bw, bh = b["bbox"]
    horizontal = max(bx - (ax + aw), ax - (bx + bw), 0)
    vertical = max(by - (ay + ah), ay - (by + bh), 0)
    if horizontal == 0 or vertical == 0:
        return max(horizontal, vertical)
    return int(math.hypot(horizontal, vertical))


def merge_component_pair(a: dict[str, object], b: dict[str, object], merged_id: str) -> dict[str, object]:
    ax, ay, aw, ah = a["bbox"]
    bx, by, bw, bh = b["bbox"]
    min_x = min(ax, bx)
    min_y = min(ay, by)
    max_x = max(ax + aw, bx + bw)
    max_y = max(ay + ah, by + bh)
    merged: dict[str, object] = {
        "id": merged_id,
        "bbox": [min_x, min_y, max_x - min_x, max_y - min_y],
        "area_px": int(a["area_px"]) + int(b["area_px"]),
        "merged_from": [*a.get("merged_from", [a["id"]]), *b.get("merged_from", [b["id"]])],
    }
    a_offsets = a.get("_pixel_offsets")
    b_offsets = b.get("_pixel_offsets")
    if isinstance(a_offsets, list) and isinstance(b_offsets, list):
        if len(a_offsets) >= len(b_offsets):
            a_offsets.extend(b_offsets)
            merged["_pixel_offsets"] = a_offsets
        else:
            b_offsets.extend(a_offsets)
            merged["_pixel_offsets"] = b_offsets
    return merged


def merge_small_fragments(
    components: list[dict[str, object]],
    distance: int,
    max_fragment_ratio: float,
) -> list[dict[str, object]]:
    if distance <= 0 or max_fragment_ratio <= 0:
        return components
    merged = list(components)
    changed = True
    while changed:
        changed = False
        best: tuple[int, int] | None = None
        best_gap: int | None = None
        for index, component in enumerate(merged):
            for other_index in range(index + 1, len(merged)):
                other = merged[other_index]
                larger = max(int(component["area_px"]), int(other["area_px"]))
                smaller = min(int(component["area_px"]), int(other["area_px"]))
                if larger <= 0 or smaller / larger > max_fragment_ratio:
                    continue
                gap = gap_between(component, other)
                if gap > distance:
                    continue
                if best_gap is None or gap < best_gap:
                    best = (index, other_index)
                    best_gap = gap
                    if best_gap == 0:
                        break
            if best_gap == 0:
                break
        if best is None:
            continue
        left, right = best
        combined = merge_component_pair(merged[left], merged[right], f"component_{left + 1}")
        next_components = []
        for index, component in enumerate(merged):
            if index not in best:
                next_components.append(component)
        next_components.append(combined)
        next_components.sort(key=lambda item: (item["bbox"][1], item["bbox"][0]))
        merged = next_components
        changed = True
    for index, component in enumerate(merged, start=1):
        component["id"] = f"component_{index}"
    return merged


def hue_distance(a: float, b: float) -> float:
    distance = abs(a - b)
    return min(distance, 1.0 - distance)


def is_generic_key_hue_like(red: int, green: int, blue: int, key: tuple[int, int, int]) -> bool:
    key_hue, key_sat, key_value = colorsys.rgb_to_hsv(key[0] / 255, key[1] / 255, key[2] / 255)
    hue, sat, value = colorsys.rgb_to_hsv(red / 255, green / 255, blue / 255)
    if key_sat < 0.35 or sat < 0.25 or value < 0.12 or key_value < 0.35:
        return False
    return hue_distance(hue, key_hue) <= 0.05


def hsv_arrays(red: object, green: object, blue: object) -> tuple[object, object, object]:
    if np is None:
        raise RuntimeError("numpy is required for hsv_arrays")
    red_f = red.astype(np.float32) / 255.0
    green_f = green.astype(np.float32) / 255.0
    blue_f = blue.astype(np.float32) / 255.0
    max_channel = np.maximum(np.maximum(red_f, green_f), blue_f)
    min_channel = np.minimum(np.minimum(red_f, green_f), blue_f)
    delta = max_channel - min_channel
    hue = np.zeros_like(max_channel, dtype=np.float32)
    nonzero = delta != 0
    red_max = nonzero & (max_channel == red_f)
    green_max = nonzero & (max_channel == green_f)
    blue_max = nonzero & (max_channel == blue_f)
    hue[red_max] = ((green_f[red_max] - blue_f[red_max]) / delta[red_max]) % 6
    hue[green_max] = ((blue_f[green_max] - red_f[green_max]) / delta[green_max]) + 2
    hue[blue_max] = ((red_f[blue_max] - green_f[blue_max]) / delta[blue_max]) + 4
    hue /= 6.0
    saturation = np.zeros_like(max_channel, dtype=np.float32)
    nonblack = max_channel != 0
    saturation[nonblack] = delta[nonblack] / max_channel[nonblack]
    return hue, saturation, max_channel


def exact_key_mask(red: object, green: object, blue: object, key: tuple[int, int, int], tolerance: int) -> object:
    if np is None:
        raise RuntimeError("numpy is required for exact_key_mask")
    return (
        (np.abs(red.astype(np.int16) - key[0]) <= tolerance)
        & (np.abs(green.astype(np.int16) - key[1]) <= tolerance)
        & (np.abs(blue.astype(np.int16) - key[2]) <= tolerance)
    )


def key_like_mask(array: object, key: tuple[int, int, int], tolerance: int) -> object:
    if np is None:
        raise RuntimeError("numpy is required for key_like_mask")
    red = array[..., 0]
    green = array[..., 1]
    blue = array[..., 2]
    alpha = array[..., 3]
    return (alpha == 0) | exact_key_mask(red, green, blue, key, tolerance)


def local_border_connected_key_mask(crop: object, key: tuple[int, int, int], tolerance: int) -> object:
    if np is None:
        raise RuntimeError("numpy is required for local_border_connected_key_mask")
    key_mask = key_like_mask(crop, key, tolerance)
    height, width = key_mask.shape
    connected = np.zeros((height, width), dtype=bool)
    queue: deque[tuple[int, int]] = deque()

    def push(x: int, y: int) -> None:
        if connected[y, x] or not key_mask[y, x]:
            return
        connected[y, x] = True
        queue.append((x, y))

    for x in range(width):
        push(x, 0)
        push(x, height - 1)
    for y in range(height):
        push(0, y)
        push(width - 1, y)

    while queue:
        x, y = queue.popleft()
        if x > 0:
            push(x - 1, y)
        if x + 1 < width:
            push(x + 1, y)
        if y > 0:
            push(x, y - 1)
        if y + 1 < height:
            push(x, y + 1)
    return connected


def key_fringe_mask(red: object, green: object, blue: object) -> object:
    if np is None:
        raise RuntimeError("numpy is required for key_fringe_mask")
    red_i = red.astype(np.int16)
    green_i = green.astype(np.int16)
    blue_i = blue.astype(np.int16)
    return (red_i > 115) & (blue_i > 120) & (green_i < 145) & (red_i + blue_i > 300) & (red_i + blue_i > green_i * 3)


def purple_halo_mask(red: object, green: object, blue: object) -> object:
    if np is None:
        raise RuntimeError("numpy is required for purple_halo_mask")
    red_i = red.astype(np.int16)
    green_i = green.astype(np.int16)
    blue_i = blue.astype(np.int16)
    return (red_i > 75) & (blue_i > 75) & (green_i < 120) & (np.minimum(red_i, blue_i) - green_i > 20) & (red_i + blue_i > green_i * 2 + 80)


def visible_component_arrays(
    array: object,
    components: list[dict[str, object]],
    current_key: tuple[int, int, int],
    tolerance: int,
) -> tuple[object, object, object]:
    if np is None:
        raise RuntimeError("numpy is required for visible_component_arrays")
    chunks = []
    flat = array.reshape((-1, 4))
    for component in components:
        pixel_offsets = component.get("_pixel_offsets")
        if isinstance(pixel_offsets, list) and pixel_offsets:
            pixels = flat[np.asarray(pixel_offsets, dtype=np.int64)]
            visible = (pixels[..., 3] > 12) & ~key_like_mask(pixels, current_key, tolerance)
            if np.any(visible):
                chunks.append(pixels[..., :3][visible])
            continue
        x, y, width, height = component["bbox"]
        crop = array[y : y + height, x : x + width]
        visible = (crop[..., 3] > 12) & ~key_like_mask(crop, current_key, tolerance)
        if np.any(visible):
            chunks.append(crop[..., :3][visible])
    if not chunks:
        empty = np.array([], dtype=np.uint8)
        return empty, empty, empty
    colors = np.concatenate(chunks, axis=0)
    return colors[:, 0], colors[:, 1], colors[:, 2]


def score_candidate_key_colors(
    image: Image.Image,
    components: list[dict[str, object]],
    current_key: tuple[int, int, int],
    tolerance: int,
    candidates: list[tuple[int, int, int]],
) -> list[dict[str, object]]:
    if np is not None:
        array = np.asarray(image.convert("RGBA"))
        red_values, green_values, blue_values = visible_component_arrays(array, components, current_key, tolerance)
        return score_candidate_key_colors_from_rgb(red_values, green_values, blue_values, candidates)

    pixels = image.load()
    visible_colors: list[tuple[int, int, int, float, float, float]] = []
    for component in components:
        x, y, width, height = component["bbox"]
        for yy in range(y, y + height):
            for xx in range(x, x + width):
                red, green, blue, alpha = pixels[xx, yy]
                if alpha <= 12 or is_key((red, green, blue, alpha), current_key, tolerance):
                    continue
                hue, sat, value = colorsys.rgb_to_hsv(red / 255, green / 255, blue / 255)
                visible_colors.append((red, green, blue, hue, sat, value))
    visible = len(visible_colors)
    scores: list[dict[str, object]] = []
    for candidate in candidates:
        exact = 0
        hue_band = 0
        key_hue, key_sat, key_value = colorsys.rgb_to_hsv(candidate[0] / 255, candidate[1] / 255, candidate[2] / 255)
        for red, green, blue, hue, sat, value in visible_colors:
            if is_exact_key_like(red, green, blue, key=candidate, tolerance=24):
                exact += 1
            if key_sat >= 0.35 and sat >= 0.25 and value >= 0.12 and key_value >= 0.35 and hue_distance(hue, key_hue) <= 0.05:
                hue_band += 1
        hue_ratio = hue_band / max(1, visible)
        exact_ratio = exact / max(1, visible)
        scores.append(
            {
                "key_color": format_color(candidate),
                "visible_px": visible,
                "exact_conflict_px": exact,
                "exact_conflict_ratio": round(exact_ratio, 6),
                "hue_band_px": hue_band,
                "hue_band_ratio": round(hue_ratio, 6),
                "score": round(exact * 1000 + hue_band + hue_ratio * 100, 6),
            }
        )
    scores.sort(key=lambda item: (item["score"], item["exact_conflict_px"], item["hue_band_ratio"], item["key_color"]))
    return scores


def score_candidate_key_colors_from_rgb(
    red_values: object,
    green_values: object,
    blue_values: object,
    candidates: list[tuple[int, int, int]],
) -> list[dict[str, object]]:
    if np is None:
        raise RuntimeError("numpy is required for score_candidate_key_colors_from_rgb")
    visible = int(red_values.size)
    if visible:
        hue, sat, value = hsv_arrays(red_values, green_values, blue_values)
        red_i = red_values.astype(np.int16)
        green_i = green_values.astype(np.int16)
        blue_i = blue_values.astype(np.int16)
        hue_eligible = (sat >= 0.25) & (value >= 0.12)
    scores: list[dict[str, object]] = []
    for candidate in candidates:
        if visible:
            exact = int(
                np.count_nonzero(
                    (np.abs(red_i - candidate[0]) <= 24)
                    & (np.abs(green_i - candidate[1]) <= 24)
                    & (np.abs(blue_i - candidate[2]) <= 24)
                )
            )
            key_hue, key_sat, key_value = colorsys.rgb_to_hsv(candidate[0] / 255, candidate[1] / 255, candidate[2] / 255)
            if key_sat >= 0.35 and key_value >= 0.35:
                hue_delta = np.abs(hue - key_hue)
                hue_band = int(np.count_nonzero(hue_eligible & (np.minimum(hue_delta, 1.0 - hue_delta) <= 0.05)))
            else:
                hue_band = 0
        else:
            exact = 0
            hue_band = 0
        hue_ratio = hue_band / max(1, visible)
        exact_ratio = exact / max(1, visible)
        scores.append(
            {
                "key_color": format_color(candidate),
                "visible_px": visible,
                "exact_conflict_px": exact,
                "exact_conflict_ratio": round(exact_ratio, 6),
                "hue_band_px": hue_band,
                "hue_band_ratio": round(hue_ratio, 6),
                "score": round(exact * 1000 + hue_band + hue_ratio * 100, 6),
            }
        )
    scores.sort(key=lambda item: (item["score"], item["exact_conflict_px"], item["hue_band_ratio"], item["key_color"]))
    return scores


def add_key_conflict_metrics(
    image: Image.Image,
    components: list[dict[str, object]],
    key: tuple[int, int, int],
    tolerance: int,
    array: object | None = None,
) -> tuple[object, object, object] | None:
    if np is not None:
        array = np.asarray(image.convert("RGBA")) if array is None else array
        flat = array.reshape((-1, 4))
        visible_chunks = []
        for component in components:
            x, y, width, height = component["bbox"]
            crop = array[y : y + height, x : x + width]
            red = crop[..., 0]
            green = crop[..., 1]
            blue = crop[..., 2]
            alpha = crop[..., 3]
            visible_alpha = alpha > 12
            exact_candidates = visible_alpha & exact_key_mask(red, green, blue, key, 24)
            if np.any(exact_candidates):
                crop_border = local_border_connected_key_mask(crop, key, tolerance)
                exact_key_conflicts = int(np.count_nonzero(exact_candidates & ~crop_border))
            else:
                exact_key_conflicts = 0
            pixel_offsets = component.get("_pixel_offsets")
            if isinstance(pixel_offsets, list) and pixel_offsets:
                component_pixels = flat[np.asarray(pixel_offsets, dtype=np.int64)]
                component_red = component_pixels[..., 0]
                component_green = component_pixels[..., 1]
                component_blue = component_pixels[..., 2]
                component_visible = (component_pixels[..., 3] > 12) & ~key_like_mask(component_pixels, key, tolerance)
                visible = int(np.count_nonzero(component_visible))
                if visible:
                    visible_chunks.append(component_pixels[..., :3][component_visible])
                key_fringe_conflicts = int(
                    np.count_nonzero(component_visible & key_fringe_mask(component_red, component_green, component_blue))
                )
                purple_halo_conflicts = int(
                    np.count_nonzero(component_visible & purple_halo_mask(component_red, component_green, component_blue))
                )
            else:
                non_key_visible = visible_alpha & ~key_like_mask(crop, key, tolerance)
                visible = int(np.count_nonzero(non_key_visible))
                if visible:
                    visible_chunks.append(crop[..., :3][non_key_visible])
                key_fringe_conflicts = int(np.count_nonzero(non_key_visible & key_fringe_mask(red, green, blue)))
                purple_halo_conflicts = int(np.count_nonzero(non_key_visible & purple_halo_mask(red, green, blue)))
            component["visible_px"] = visible
            component["exact_key_conflict_px"] = exact_key_conflicts
            component["key_fringe_hue_px"] = key_fringe_conflicts
            component["purple_halo_hue_px"] = purple_halo_conflicts
            component["key_hue_conflict_ratio"] = round(
                (exact_key_conflicts + key_fringe_conflicts + purple_halo_conflicts) / max(1, visible),
                6,
            )
        if not visible_chunks:
            empty = np.array([], dtype=np.uint8)
            return empty, empty, empty
        visible_colors = np.concatenate(visible_chunks, axis=0)
        return visible_colors[:, 0], visible_colors[:, 1], visible_colors[:, 2]

    image_width, _image_height = image.size
    border_key = find_border_connected_key(image, key, tolerance)
    pixels = image.load()
    for component in components:
        x, y, width, height = component["bbox"]
        visible = 0
        exact_key_conflicts = 0
        key_fringe_conflicts = 0
        purple_halo_conflicts = 0
        for yy in range(y, y + height):
            for xx in range(x, x + width):
                red, green, blue, alpha = pixels[xx, yy]
                if alpha <= 12:
                    continue
                if is_exact_key_like(red, green, blue, key=key, tolerance=24) and not border_key[yy * image_width + xx]:
                    exact_key_conflicts += 1
                if is_key((red, green, blue, alpha), key, tolerance):
                    continue
                visible += 1
                if is_key_fringe_like(red, green, blue):
                    key_fringe_conflicts += 1
                if is_purple_halo_like(red, green, blue):
                    purple_halo_conflicts += 1
        component["visible_px"] = visible
        component["exact_key_conflict_px"] = exact_key_conflicts
        component["key_fringe_hue_px"] = key_fringe_conflicts
        component["purple_halo_hue_px"] = purple_halo_conflicts
        component["key_hue_conflict_ratio"] = round(
            (exact_key_conflicts + key_fringe_conflicts + purple_halo_conflicts) / max(1, visible),
            6,
        )
    return None


def audit(args: argparse.Namespace) -> dict[str, object]:
    started = perf_counter()
    phase_started = started
    timings: dict[str, float] = {}

    def mark_timing(name: str) -> None:
        nonlocal phase_started
        if not getattr(args, "profile", False):
            return
        now = perf_counter()
        timings[name] = round((now - phase_started) * 1000, 3)
        phase_started = now

    image = Image.open(args.source).convert("RGBA")
    mark_timing("load_image")
    width, height = image.size
    image_array = np.asarray(image) if np is not None else None
    components = [
        component
        for component in find_components(image, args.key_color, args.key_tolerance, image_array)
        if component["area_px"] >= args.min_area
    ]
    mark_timing("find_components")
    components = merge_small_fragments(components, args.merge_fragments_distance, args.merge_fragment_area_ratio)
    mark_timing("merge_fragments")
    visible_component_rgb = add_key_conflict_metrics(image, components, args.key_color, args.key_tolerance, image_array)
    mark_timing("key_conflicts")
    if visible_component_rgb is not None and np is not None:
        candidate_scores = score_candidate_key_colors_from_rgb(*visible_component_rgb, args.candidate_key_colors)
    else:
        candidate_scores = score_candidate_key_colors(
            image,
            components,
            args.key_color,
            args.key_tolerance,
            args.candidate_key_colors,
        )
    mark_timing("candidate_key_scores")
    problems: list[str] = []
    key_color_conflict_count = 0
    if len(components) < args.min_components:
        problems.append(f"component_count {len(components)} is below required {args.min_components}")

    for component in components:
        x, y, w, h = component["bbox"]
        border_gap = min(x, y, width - (x + w), height - (y + h))
        component["border_gap_px"] = border_gap
        if border_gap < args.min_border:
            problems.append(f"{component['id']} border gap {border_gap}px is below required {args.min_border}px")
        exact_conflicts = int(component["exact_key_conflict_px"])
        if exact_conflicts > args.max_exact_key_conflict_px:
            key_color_conflict_count += 1
            problems.append(
                f"{component['id']} contains {exact_conflicts}px of exact key-color-like art "
                f"> allowed {args.max_exact_key_conflict_px}px"
            )
        key_hue_ratio = float(component["key_hue_conflict_ratio"])
        if key_hue_ratio > args.max_key_hue_conflict_ratio:
            key_color_conflict_count += 1
            problems.append(
                f"{component['id']} key/halo hue conflict ratio {key_hue_ratio:.3f} "
                f"> allowed {args.max_key_hue_conflict_ratio:.3f}; choose a safer background or split/preserve this art"
            )
    mark_timing("component_rules")

    min_gap = None
    closest_pair = None
    for index, component in enumerate(components):
        for other in components[index + 1 :]:
            gap = gap_between(component, other)
            if min_gap is None or gap < min_gap:
                min_gap = gap
                closest_pair = [component["id"], other["id"]]
    if min_gap is not None and min_gap < args.min_gutter:
        problems.append(f"closest component gap {min_gap}px is below required {args.min_gutter}px")
    mark_timing("gutter_scan")

    key_color_text = format_color(args.key_color)
    suggested_key_color = candidate_scores[0]["key_color"] if candidate_scores else None
    current_key_score = next((score for score in candidate_scores if score["key_color"] == key_color_text), None)
    if key_color_conflict_count > 0 and isinstance(suggested_key_color, str) and suggested_key_color != key_color_text:
        key_color_action = "regenerate_with_next_prompt_key_color"
        next_prompt_key_color = suggested_key_color
    elif key_color_conflict_count > 0:
        key_color_action = "split_preserve_or_dual_plate_alpha"
        next_prompt_key_color = None
    else:
        key_color_action = "keep_current_key_color"
        next_prompt_key_color = key_color_text
    components_with_border_gap = [
        component for component in components if int(component.get("border_gap_px") or 0) < args.min_border
    ]
    components_with_exact_key_conflict = [
        component for component in components if int(component.get("exact_key_conflict_px") or 0) > args.max_exact_key_conflict_px
    ]
    components_with_key_hue_conflict = [
        component for component in components if float(component.get("key_hue_conflict_ratio") or 0.0) > args.max_key_hue_conflict_ratio
    ]
    worst_key_hue_component = max(
        components,
        key=lambda component: float(component.get("key_hue_conflict_ratio") or 0.0),
        default=None,
    )
    total_exact_key_conflict_px = sum(int(component.get("exact_key_conflict_px") or 0) for component in components)
    total_key_fringe_hue_px = sum(int(component.get("key_fringe_hue_px") or 0) for component in components)
    total_purple_halo_hue_px = sum(int(component.get("purple_halo_hue_px") or 0) for component in components)
    problem_summary = {
        "components_with_border_gap": len(components_with_border_gap),
        "components_with_exact_key_conflict": len(components_with_exact_key_conflict),
        "components_with_key_hue_conflict": len(components_with_key_hue_conflict),
        "total_exact_key_conflict_px": total_exact_key_conflict_px,
        "total_key_fringe_hue_px": total_key_fringe_hue_px,
        "total_purple_halo_hue_px": total_purple_halo_hue_px,
        "gutter_below_min": bool(min_gap is not None and min_gap < args.min_gutter),
        "worst_key_hue_component": None
        if worst_key_hue_component is None
        else {
            "id": worst_key_hue_component["id"],
            "ratio": worst_key_hue_component["key_hue_conflict_ratio"],
            "bbox": worst_key_hue_component["bbox"],
        },
    }
    if key_color_action == "regenerate_with_next_prompt_key_color":
        recommended_next_step = {
            "action": "regenerate_source_sheet_with_safer_key_color",
            "reason": "current key color conflicts with visible component art or halo colors",
            "key_color": suggested_key_color,
        }
    elif key_color_action == "split_preserve_or_dual_plate_alpha":
        recommended_next_step = {
            "action": "split_preserve_or_dual_plate_alpha",
            "reason": "key color conflicts exist but no safer candidate key color was found",
            "key_color": None,
        }
    elif len(components) < args.min_components:
        recommended_next_step = {
            "action": "regenerate_source_sheet_with_clearer_separation",
            "reason": "detected component count is below the expected minimum",
            "key_color": key_color_text,
        }
    elif components_with_border_gap or problem_summary["gutter_below_min"]:
        recommended_next_step = {
            "action": "regenerate_source_sheet_with_more_gutter_and_safe_border",
            "reason": "components are too close to sheet borders or each other for reliable slicing",
            "key_color": key_color_text,
        }
    else:
        recommended_next_step = {
            "action": "slice_ready",
            "reason": "source sheet passed intake checks",
            "key_color": key_color_text,
        }

    public_components = [{key: value for key, value in component.items() if not str(key).startswith("_")} for component in components]

    result = {
        "schema": "game.source_sheet_intake_audit",
        "version": 1,
        "source": str(args.source).replace("\\", "/"),
        "analysis_engine": "numpy" if np is not None else "python",
        "size": [width, height],
        "key_color": key_color_text,
        "key_tolerance": args.key_tolerance,
        "component_count": len(components),
        "min_component_area_px": args.min_area,
        "min_border_px": args.min_border,
        "min_gutter_px": args.min_gutter,
        "max_exact_key_conflict_px": args.max_exact_key_conflict_px,
        "max_key_hue_conflict_ratio": args.max_key_hue_conflict_ratio,
        "closest_gap_px": min_gap,
        "closest_pair": closest_pair,
        "candidate_key_scores": candidate_scores,
        "current_key_score": current_key_score,
        "suggested_key_color": suggested_key_color,
        "key_color_conflict_count": key_color_conflict_count,
        "key_color_action": key_color_action,
        "next_prompt_key_color": next_prompt_key_color,
        "problem_summary": problem_summary,
        "recommended_next_step": recommended_next_step,
        "components": public_components,
        "problems": problems,
        "status": "pass" if not problems else "fail",
    }
    if getattr(args, "profile", False):
        timings["total"] = round((perf_counter() - started) * 1000, 3)
        result["timing_ms"] = timings
    return result


def write_report(path: Path, result: dict[str, object]) -> None:
    lines = [
        f"# Source Sheet Intake Audit: {Path(result['source']).name}",
        "",
        f"status: {result['status']}",
        f"analysis_engine: {result['analysis_engine']}",
        f"size: {result['size'][0]}x{result['size'][1]}",
        f"component_count: {result['component_count']}",
        f"closest_gap_px: {result['closest_gap_px']}",
        f"max_exact_key_conflict_px: {result['max_exact_key_conflict_px']}",
        f"max_key_hue_conflict_ratio: {result['max_key_hue_conflict_ratio']}",
        f"suggested_key_color: {result['suggested_key_color']}",
        f"key_color_action: {result['key_color_action']}",
        f"next_prompt_key_color: {result['next_prompt_key_color']}",
        f"recommended_next_step: {result['recommended_next_step']['action']}",
        "",
        "## Problems",
    ]
    problems = result["problems"]
    if problems:
        lines.extend(f"- {problem}" for problem in problems)
    else:
        lines.append("- none")
    summary = result["problem_summary"]
    lines.extend(
        [
            "",
            "## Problem Summary",
            f"- components_with_border_gap: {summary['components_with_border_gap']}",
            f"- components_with_exact_key_conflict: {summary['components_with_exact_key_conflict']}",
            f"- components_with_key_hue_conflict: {summary['components_with_key_hue_conflict']}",
            f"- total_exact_key_conflict_px: {summary['total_exact_key_conflict_px']}",
            f"- total_key_fringe_hue_px: {summary['total_key_fringe_hue_px']}",
            f"- total_purple_halo_hue_px: {summary['total_purple_halo_hue_px']}",
            f"- gutter_below_min: {str(summary['gutter_below_min']).lower()}",
            f"- worst_key_hue_component: {summary['worst_key_hue_component']}",
            "",
            "## Recommended Next Step",
            f"- action: {result['recommended_next_step']['action']}",
            f"- reason: {result['recommended_next_step']['reason']}",
            f"- key_color: {result['recommended_next_step']['key_color']}",
        ]
    )
    lines.extend(["", "## Components"])
    for component in result["components"]:
        lines.append(
            f"- {component['id']}: bbox={component['bbox']} area={component['area_px']} "
            f"border_gap={component['border_gap_px']} visible={component['visible_px']} "
            f"exact_key={component['exact_key_conflict_px']} "
            f"key_fringe_hue={component['key_fringe_hue_px']} "
            f"purple_halo_hue={component['purple_halo_hue_px']} "
            f"key_hue_ratio={component['key_hue_conflict_ratio']}"
        )
    lines.extend(["", "## Candidate Key Colors"])
    for score in result["candidate_key_scores"]:
        lines.append(
            f"- {score['key_color']}: exact={score['exact_conflict_px']} "
            f"exact_ratio={score['exact_conflict_ratio']} hue_band={score['hue_band_px']} "
            f"hue_ratio={score['hue_band_ratio']} score={score['score']}"
        )
    if result.get("timing_ms"):
        lines.extend(["", "## Timing"])
        for name, elapsed in result["timing_ms"].items():
            lines.append(f"- {name}: {elapsed} ms")
    write_text_atomic(path, "\n".join(lines) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit generated source sheet gross cut-readiness before slicing.")
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--key-color", type=parse_color, default=parse_color("#ff00ff"))
    parser.add_argument("--key-tolerance", type=int, default=8)
    parser.add_argument("--min-components", type=int, default=1)
    parser.add_argument("--min-area", type=int, default=128)
    parser.add_argument("--min-border", type=int, default=24)
    parser.add_argument("--min-gutter", type=int, default=24)
    parser.add_argument("--merge-fragments-distance", type=int, default=24)
    parser.add_argument("--merge-fragment-area-ratio", type=float, default=0.20)
    parser.add_argument("--max-exact-key-conflict-px", type=int, default=0)
    parser.add_argument("--max-key-hue-conflict-ratio", type=float, default=0.05)
    parser.add_argument("--candidate-key-colors", type=parse_color_list, default=parse_color_list(DEFAULT_CANDIDATE_KEY_COLORS))
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--profile", action="store_true", help="Record per-stage timing in JSON/Markdown and print the slowest stage.")
    args = parser.parse_args()

    result = audit(args)
    if args.json_output:
        write_json_atomic(args.json_output, result)
    if args.report:
        write_report(args.report, result)
    print(
        f"{result['status']}: {result['component_count']} component(s), "
        f"closest_gap={result['closest_gap_px']}, next_prompt_key={result['next_prompt_key_color']}"
    )
    for problem in result["problems"]:
        print(f"problem: {problem}")
    if args.profile and result.get("timing_ms"):
        timed_stages = {key: value for key, value in result["timing_ms"].items() if key != "total"}
        if timed_stages:
            slowest_name, slowest_ms = max(timed_stages.items(), key=lambda item: item[1])
            print(f"profile: slowest stage `{slowest_name}` {slowest_ms} ms")
    return 0 if result["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
