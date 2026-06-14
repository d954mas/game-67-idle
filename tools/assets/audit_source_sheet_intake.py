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


def find_components(image: Image.Image, key: tuple[int, int, int], tolerance: int) -> list[dict[str, object]]:
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
            while queue:
                cx, cy = queue.popleft()
                area += 1
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
    return {
        "id": merged_id,
        "bbox": [min_x, min_y, max_x - min_x, max_y - min_y],
        "area_px": int(a["area_px"]) + int(b["area_px"]),
        "merged_from": [*a.get("merged_from", [a["id"]]), *b.get("merged_from", [b["id"]])],
    }


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
    for component in components:
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
        visible = int(red_values.size)
        if visible:
            hue, sat, value = hsv_arrays(red_values, green_values, blue_values)
        scores: list[dict[str, object]] = []
        for candidate in candidates:
            if visible:
                exact = int(np.count_nonzero(exact_key_mask(red_values, green_values, blue_values, candidate, 24)))
                key_hue, key_sat, key_value = colorsys.rgb_to_hsv(candidate[0] / 255, candidate[1] / 255, candidate[2] / 255)
                hue_delta = np.abs(hue - key_hue)
                hue_band_mask = (
                    (key_sat >= 0.35)
                    & (sat >= 0.25)
                    & (value >= 0.12)
                    & (key_value >= 0.35)
                    & (np.minimum(hue_delta, 1.0 - hue_delta) <= 0.05)
                )
                hue_band = int(np.count_nonzero(hue_band_mask))
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


def add_key_conflict_metrics(
    image: Image.Image,
    components: list[dict[str, object]],
    key: tuple[int, int, int],
    tolerance: int,
) -> None:
    if np is not None:
        array = np.asarray(image.convert("RGBA"))
        image_width, image_height = image.size
        border_key = None
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
                if border_key is None:
                    border_key = np.frombuffer(find_border_connected_key(image, key, tolerance), dtype=np.uint8).reshape((image_height, image_width)).astype(bool)
                crop_border = border_key[y : y + height, x : x + width]
                exact_key_conflicts = int(np.count_nonzero(exact_candidates & ~crop_border))
            else:
                exact_key_conflicts = 0
            non_key_visible = visible_alpha & ~key_like_mask(crop, key, tolerance)
            visible = int(np.count_nonzero(non_key_visible))
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
        return

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
    components = [component for component in find_components(image, args.key_color, args.key_tolerance) if component["area_px"] >= args.min_area]
    mark_timing("find_components")
    components = merge_small_fragments(components, args.merge_fragments_distance, args.merge_fragment_area_ratio)
    mark_timing("merge_fragments")
    add_key_conflict_metrics(image, components, args.key_color, args.key_tolerance)
    mark_timing("key_conflicts")
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
        "components": components,
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
        "",
        "## Problems",
    ]
    problems = result["problems"]
    if problems:
        lines.extend(f"- {problem}" for problem in problems)
    else:
        lines.append("- none")
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
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


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
        args.json_output.parent.mkdir(parents=True, exist_ok=True)
        args.json_output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
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
