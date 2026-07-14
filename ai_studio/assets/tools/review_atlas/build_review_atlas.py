#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path
from time import perf_counter
from typing import Any

from PIL import Image
from PIL import ImageDraw

import sys

REPO_ROOT = Path(__file__).resolve().parents[4]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from ai_studio.assets.tools.lib.atomic_io import save_image_atomic, write_json_atomic, write_text_atomic
from ai_studio.assets.tools.review_atlas.atlas_review_labels import (
    DEFAULT_LABEL_FONT_SIZE,
    LABEL_LINE_GAP_Y,
    LABEL_OUTER_MARGIN,
    LABEL_PAD_X,
    LABEL_PAD_Y,
    label_font,
    measure_label,
    review_label_text,
)


RESAMPLE_NEAREST = getattr(getattr(Image, "Resampling", Image), "NEAREST", Image.NEAREST)
# Label-box constants + font/measure helpers + the review-label text format are
# shared with audit_review_atlas via prep/review_atlas/atlas_review_labels.py.
# LABEL_GAP_Y (tile<->label gap) and the wrap widths below are build-only.
LABEL_GAP_Y = 3
LABEL_MIN_WIDTH = 72
LABEL_MAX_WIDTH = 220
LABEL_PLACEMENTS = {"bottom", "right"}


def fail(message: str) -> None:
    raise SystemExit(f"error: {message}")


def norm_path(path: Path, base_dir: Path) -> str:
    try:
        return path.resolve().relative_to(base_dir.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def project_path(value: str, base_dir: Path) -> Path:
    path = Path(value)
    if path.is_absolute():
        return path
    return base_dir / path


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: dict[str, Any]) -> None:
    write_json_atomic(path, data)


def write_text(path: Path, text: str) -> None:
    write_text_atomic(path, text)


def without_profile_fields(value: Any) -> Any:
    if isinstance(value, dict):
        return {
            key: without_profile_fields(item)
            for key, item in value.items()
            if key not in {"timing_ms", "atlas_efficiency"}
        }
    if isinstance(value, list):
        return [without_profile_fields(item) for item in value]
    return value


def profile_report_from_review_atlas(review_atlas_manifest: dict[str, Any]) -> dict[str, Any]:
    return {
        "schema": "game.review_atlas_profile",
        "version": 1,
        "asset_manifest": review_atlas_manifest.get("asset_manifest"),
        "output_dir": review_atlas_manifest.get("output_dir"),
        "label_overlay": review_atlas_manifest.get("label_overlay"),
        "atlas_efficiency": review_atlas_manifest.get("atlas_efficiency"),
        "timing_ms": review_atlas_manifest.get("timing_ms"),
        "atlases": [
            {
                "pack_group": atlas.get("pack_group"),
                "path": atlas.get("path"),
                "labeled_preview_path": atlas.get("labeled_preview_path"),
                "size": atlas.get("size"),
                "entry_count": atlas.get("entry_count"),
                "physical_entry_count": atlas.get("physical_entry_count"),
                "alias_count": atlas.get("alias_count"),
                "occupancy_ratio": atlas.get("occupancy_ratio"),
                "padded_asset_ratio": atlas.get("padded_asset_ratio"),
                "timing_ms": atlas.get("timing_ms"),
            }
            for atlas in review_atlas_manifest.get("atlases", [])
            if isinstance(atlas, dict)
        ],
    }


def clean_name(value: str) -> str:
    return re.sub(r"[^a-zA-Z0-9_.-]+", "-", value).strip("-") or "ui_atlas"


def positive_int(value: Any, fallback: int) -> int:
    try:
        parsed = int(value)
    except (TypeError, ValueError):
        return fallback
    return parsed if parsed >= 0 else fallback


def wrap_long_piece(piece: str, max_width: int, font_size: int = DEFAULT_LABEL_FONT_SIZE) -> list[str]:
    lines: list[str] = []
    current = ""
    for char in piece:
        candidate = current + char
        if current and measure_label(candidate, font_size)[0] > max_width:
            lines.append(current)
            current = char
        else:
            current = candidate
    if current:
        lines.append(current)
    return lines or [piece]


def identifier_pieces(text: str) -> list[str]:
    raw = re.split(r"([_-])", text)
    pieces: list[str] = []
    index = 0
    while index < len(raw):
        part = raw[index]
        if not part:
            index += 1
            continue
        if index + 1 < len(raw) and raw[index + 1] in {"_", "-"}:
            pieces.append(part + raw[index + 1])
            index += 2
        else:
            pieces.append(part)
            index += 1
    return pieces or [text]


def wrap_identifier_line(text: str, max_width: int, font_size: int = DEFAULT_LABEL_FONT_SIZE) -> list[str]:
    lines: list[str] = []
    current = ""
    for piece in identifier_pieces(text):
        candidates = wrap_long_piece(piece, max_width, font_size) if measure_label(piece, font_size)[0] > max_width else [piece]
        for candidate_piece in candidates:
            candidate = current + candidate_piece
            if current and measure_label(candidate, font_size)[0] > max_width:
                lines.append(current)
                current = candidate_piece
            else:
                current = candidate
    if current:
        lines.append(current)
    return lines or [text]


def label_display_lines(label: str, max_width: int, font_size: int = DEFAULT_LABEL_FONT_SIZE) -> list[str]:
    source_lines = [label]
    alias_match = re.match(r"^(.+) \(\+(.+)\)$", label)
    if alias_match:
        source_lines = [alias_match.group(1), *[f"+{alias}" for alias in alias_match.group(2).split(",") if alias]]
    lines: list[str] = []
    for source_line in source_lines:
        lines.extend(wrap_identifier_line(source_line, max_width, font_size))
    return lines or [label]


def measure_label_lines(lines: list[str], font_size: int = DEFAULT_LABEL_FONT_SIZE) -> tuple[int, int]:
    line_sizes = [measure_label(line, font_size) for line in lines]
    width = max((size[0] for size in line_sizes), default=0)
    line_height = max((size[1] for size in line_sizes), default=0)
    height = line_height * len(lines) + LABEL_LINE_GAP_Y * max(0, len(lines) - 1)
    return width, height


def item_label(item: dict[str, Any], aliases_by_target: dict[str, list[str]] | None = None) -> str:
    target_id = str(item["asset"]["id"])
    aliases = aliases_by_target.get(target_id, []) if aliases_by_target else []
    return review_label_text(target_id, aliases)


def prepare_review_labels(
    items: list[dict[str, Any]],
    alias_items: list[dict[str, Any]],
    label_review: bool,
    label_font_size: int,
) -> None:
    if not label_review:
        return
    aliases_by_target: dict[str, list[str]] = {}
    for item in alias_items:
        alias_of = item["asset"].get("alias_of")
        if alias_of:
            aliases_by_target.setdefault(str(alias_of), []).append(str(item["asset"]["id"]))
    for item in items:
        label = item_label(item, aliases_by_target)
        padded_width = int(item["image"].width) + int(item["extrude"]) * 2
        max_text_width = min(LABEL_MAX_WIDTH, max(LABEL_MIN_WIDTH, padded_width))
        lines = label_display_lines(label, max_text_width, label_font_size)
        label_width, label_height = measure_label_lines(lines, label_font_size)
        label_box_width = label_width + LABEL_PAD_X * 2
        label_box_height = label_height + LABEL_PAD_Y * 2
        placement, tile_width, tile_height = choose_label_placement(
            padded_width,
            int(item["image"].height) + int(item["extrude"]) * 2,
            label_box_width,
            label_box_height,
            LABEL_GAP_Y,
        )
        item["review_label"] = {
            "text": label,
            "lines": lines,
            "font_size": label_font_size,
            "width": label_box_width,
            "height": label_box_height,
            "gap_y": LABEL_GAP_Y,
            "placement": placement,
            "tile_width": tile_width,
            "tile_height": tile_height,
        }


def choose_label_placement(
    padded_width: int,
    padded_height: int,
    label_width: int,
    label_height: int,
    gap: int,
) -> tuple[str, int, int]:
    bottom = {
        "placement": "bottom",
        "tile_width": max(padded_width, label_width),
        "tile_height": padded_height + gap + label_height,
    }
    right = {
        "placement": "right",
        "tile_width": padded_width + gap + label_width,
        "tile_height": max(padded_height, label_height),
    }
    bottom_area = bottom["tile_width"] * bottom["tile_height"]
    right_area = right["tile_width"] * right["tile_height"]
    if right_area < bottom_area:
        selected = right
    else:
        selected = bottom
    return str(selected["placement"]), int(selected["tile_width"]), int(selected["tile_height"])


def load_assets(manifest: dict[str, Any], base_dir: Path) -> list[dict[str, Any]]:
    if manifest.get("schema") != "game.asset_manifest":
        fail("asset manifest schema must be game.asset_manifest")
    assets = manifest.get("assets")
    if not isinstance(assets, list) or not assets:
        fail("asset manifest needs non-empty assets")
    loaded: list[dict[str, Any]] = []
    for asset in assets:
        if not isinstance(asset, dict):
            fail("asset entries must be objects")
        asset_id = asset.get("id")
        if not isinstance(asset_id, str) or not asset_id:
            fail("asset entry needs id")
        path_value = asset.get("path")
        if not isinstance(path_value, str) or not path_value:
            fail(f"asset {asset_id} needs path")
        pack_group = asset.get("pack_group")
        if not isinstance(pack_group, str) or not pack_group:
            fail(f"asset {asset_id} needs pack_group")
        policy = asset.get("atlas_policy")
        if not isinstance(policy, dict):
            fail(f"asset {asset_id} needs atlas_policy")
        source_path = project_path(path_value, base_dir)
        if not source_path.exists():
            fail(f"asset image missing: {path_value}")
        image = Image.open(source_path).convert("RGBA")
        loaded.append(
            {
                "asset": asset,
                "image": image,
                "source_path": path_value,
                "pack_group": pack_group,
                "extrude": max(1, positive_int(policy.get("extrude"), 1)),
                "shape_padding": max(0, positive_int(policy.get("shape_padding"), 2)),
                "border_padding": max(0, positive_int(policy.get("border_padding"), 1)),
            }
        )
    return loaded


def paste_extruded(atlas: Image.Image, image: Image.Image, x: int, y: int, extrude: int) -> None:
    width, height = image.size
    atlas.alpha_composite(image, (x + extrude, y + extrude))
    if extrude <= 0:
        return

    top = image.crop((0, 0, width, 1)).resize((width, extrude), RESAMPLE_NEAREST)
    bottom = image.crop((0, height - 1, width, height)).resize((width, extrude), RESAMPLE_NEAREST)
    left = image.crop((0, 0, 1, height)).resize((extrude, height), RESAMPLE_NEAREST)
    right = image.crop((width - 1, 0, width, height)).resize((extrude, height), RESAMPLE_NEAREST)
    atlas.alpha_composite(top, (x + extrude, y))
    atlas.alpha_composite(bottom, (x + extrude, y + extrude + height))
    atlas.alpha_composite(left, (x, y + extrude))
    atlas.alpha_composite(right, (x + extrude + width, y + extrude))

    corners = [
        ((0, 0, 1, 1), (x, y)),
        ((width - 1, 0, width, 1), (x + extrude + width, y)),
        ((0, height - 1, 1, height), (x, y + extrude + height)),
        ((width - 1, height - 1, width, height), (x + extrude + width, y + extrude + height)),
    ]
    for crop_box, target in corners:
        corner = image.crop(crop_box).resize((extrude, extrude), RESAMPLE_NEAREST)
        atlas.alpha_composite(corner, target)


def packed_tile_size(item: dict[str, Any]) -> tuple[int, int]:
    image = item["image"]
    extrude = item["extrude"]
    padded_width = image.width + extrude * 2
    padded_height = image.height + extrude * 2
    review_label = item.get("review_label") if isinstance(item.get("review_label"), dict) else None
    label_width = int(review_label.get("width", 0)) if review_label else 0
    label_height = int(review_label.get("height", 0)) if review_label else 0
    label_gap_y = int(review_label.get("gap_y", 0)) if review_label else 0
    if review_label:
        placement = str(review_label.get("placement") or "bottom")
        if placement == "right":
            tile_width = padded_width + label_gap_y + label_width
            tile_height = max(padded_height, label_height)
        else:
            tile_width = max(padded_width, label_width)
            tile_height = padded_height + label_gap_y + label_height
    else:
        tile_width = padded_width
        tile_height = padded_height
    return tile_width, tile_height


def try_pack(items: list[dict[str, Any]], target_width: int, border: int, shape_padding: int, max_size: int) -> tuple[list[dict[str, Any]], int, int] | None:
    x = border
    y = border
    row_height = 0
    used_width = border
    placements: list[dict[str, Any]] = []
    for item in items:
        tile_width, tile_height = packed_tile_size(item)
        if tile_width + border * 2 > max_size or tile_height + border * 2 > max_size:
            return None
        if x > border and x + tile_width + border > target_width:
            y += row_height + shape_padding
            x = border
            row_height = 0
        placements.append(
            {
                **item,
                "x": x,
                "y": y,
                "tile_width": tile_width,
                "tile_height": tile_height,
            }
        )
        used_width = max(used_width, x + tile_width + border)
        x += tile_width + shape_padding
        row_height = max(row_height, tile_height)
    used_height = y + row_height + border
    if used_width > max_size or used_height > max_size:
        return None
    return placements, used_width, used_height


def make_entry(item: dict[str, Any], x: int, y: int, image: Image.Image) -> dict[str, Any]:
    asset = item["asset"]
    extrude = item["extrude"]
    entry = {
        "id": asset["id"],
        "kind": asset.get("kind"),
        "source_path": item["source_path"],
        "atlas_rect": [x + extrude, y + extrude, image.width, image.height],
        "padded_rect": [x, y, image.width + extrude * 2, image.height + extrude * 2],
        "extrude": extrude,
        "shape_padding": item["shape_padding"],
        "border_padding": item["border_padding"],
        "trim_rect": asset.get("trim_rect"),
        "original_size": asset.get("original_size"),
        "slice9": asset.get("slice9"),
        "content": asset.get("content") or asset.get("content_rect"),
        "usage_policy": asset.get("usage_policy"),
        "role": asset.get("role"),
        "state": asset.get("state"),
    }
    if asset.get("alias_of"):
        entry["alias_of"] = asset["alias_of"]
    review_label = item.get("review_label")
    if isinstance(review_label, dict):
        placement = str(review_label.get("placement") or "bottom")
        if placement not in LABEL_PLACEMENTS:
            placement = "bottom"
        if placement == "right":
            label_x = x + image.width + extrude * 2 + int(review_label.get("gap_y", 0))
            label_y = y
        else:
            label_x = x
            label_y = y + image.height + extrude * 2 + int(review_label.get("gap_y", 0))
        entry["review_label"] = {
            "text": review_label["text"],
            "lines": list(review_label.get("lines") or [review_label["text"]]),
            "font_size": int(review_label.get("font_size") or DEFAULT_LABEL_FONT_SIZE),
            "rect": [label_x, label_y, int(review_label["width"]), int(review_label["height"])],
            "placement": placement,
        }
    return entry


def draw_review_labels(atlas: Image.Image, entries: list[dict[str, Any]]) -> None:
    for entry in entries:
        if entry.get("alias_of"):
            continue
        review_label = entry.get("review_label")
        if isinstance(review_label, dict) and isinstance(review_label.get("rect"), list):
            label = str(review_label.get("text") or entry["id"])
            lines = review_label.get("lines")
            if not isinstance(lines, list) or not all(isinstance(line, str) for line in lines):
                lines = [label]
            font_size = int(review_label.get("font_size") or DEFAULT_LABEL_FONT_SIZE)
            label_x, label_y, label_width, label_height = [int(value) for value in review_label["rect"]]
            label_layer = Image.new("RGBA", (label_width, label_height), (0, 0, 0, 0))
            draw = ImageDraw.Draw(label_layer)
            draw.rectangle([0, 0, label_width - 1, label_height - 1], fill=(0, 0, 0, 170))
            line_y = LABEL_PAD_Y
            line_height = max((measure_label(line, font_size)[1] for line in lines), default=0)
            font = label_font(font_size)
            for line in lines:
                draw.text((LABEL_PAD_X, line_y), line, fill=(255, 255, 255, 255), font=font)
                line_y += line_height + LABEL_LINE_GAP_Y
            atlas.alpha_composite(label_layer, (label_x, label_y))


def pack_group(
    group: str,
    items: list[dict[str, Any]],
    output_dir: Path,
    max_size: int,
    label_review: bool,
    label_font_size: int,
    base_dir: Path,
    profile: bool = False,
) -> dict[str, Any]:
    started = perf_counter()
    timings: dict[str, float] = {}
    canonical_items = [item for item in items if not item["asset"].get("alias_of")]
    alias_items = [item for item in items if item["asset"].get("alias_of")]
    if not canonical_items:
        fail(f"pack group {group} has only aliases and no physical source asset")
    ids_in_group = {item["asset"]["id"] for item in items}
    for item in alias_items:
        alias_of = item["asset"].get("alias_of")
        if not isinstance(alias_of, str) or not alias_of:
            fail(f"asset {item['asset']['id']} has invalid alias_of")
        if alias_of not in ids_in_group:
            fail(f"asset {item['asset']['id']} aliases missing asset {alias_of} in pack group {group}")

    layout_started = perf_counter()
    sorted_items = sorted(canonical_items, key=lambda item: (-item["image"].height, -item["image"].width, item["asset"]["id"]))
    prepare_review_labels(sorted_items, alias_items, label_review, label_font_size)
    border = max(item["border_padding"] for item in sorted_items)
    if label_review:
        border = max(border, LABEL_OUTER_MARGIN)
    shape_padding = max(item["shape_padding"] for item in sorted_items)
    tile_sizes = [packed_tile_size(item) for item in sorted_items]
    total_area = sum((tile_width + shape_padding) * (tile_height + shape_padding) for tile_width, tile_height in tile_sizes)
    widest = max(tile_width for tile_width, _ in tile_sizes) + border * 2
    target_width = min(max_size, max(widest, 256, int(math.sqrt(total_area) * 1.35)))
    placement_result = None
    while target_width <= max_size:
        placement_result = try_pack(sorted_items, target_width, border, shape_padding, max_size)
        if placement_result is not None:
            break
        if target_width == max_size:
            break
        target_width = min(max_size, max(target_width * 2, widest))
    if placement_result is None:
        fail(f"pack group {group} exceeds max atlas size {max_size}")
    if profile:
        timings["layout"] = round((perf_counter() - layout_started) * 1000, 3)

    placements, width, height = placement_result
    compose_started = perf_counter()
    atlas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    entries: list[dict[str, Any]] = []
    entries_by_id: dict[str, dict[str, Any]] = {}
    for placement in placements:
        image = placement["image"]
        x = int(placement["x"])
        y = int(placement["y"])
        paste_extruded(atlas, image, x, y, placement["extrude"])
        entry = make_entry(placement, x, y, image)
        entries.append(entry)
        entries_by_id[entry["id"]] = entry
    if profile:
        timings["compose"] = round((perf_counter() - compose_started) * 1000, 3)

    item_by_id = {item["asset"]["id"]: item for item in items}
    for item in sorted(alias_items, key=lambda value: value["asset"]["id"]):
        asset = item["asset"]
        target_id = asset["alias_of"]
        target_entry = entries_by_id.get(target_id)
        target_item = item_by_id.get(target_id)
        if target_entry is None or target_item is None:
            fail(f"asset {asset['id']} aliases missing physical entry {target_id} in pack group {group}")
        if item["image"].size != target_item["image"].size:
            fail(f"asset {asset['id']} alias image size must match {target_id}")
        entry = make_entry(item, target_entry["padded_rect"][0], target_entry["padded_rect"][1], target_item["image"])
        entry["atlas_rect"] = list(target_entry["atlas_rect"])
        entry["padded_rect"] = list(target_entry["padded_rect"])
        entry["extrude"] = target_entry["extrude"]
        entries.append(entry)

    path = output_dir / f"{clean_name(group)}.png"
    path.parent.mkdir(parents=True, exist_ok=True)
    sorted_entries = sorted(entries, key=lambda entry: entry["id"])
    save_started = perf_counter()
    save_image_atomic(atlas, path)
    if profile:
        timings["save_atlas"] = round((perf_counter() - save_started) * 1000, 3)
    labeled_preview_path = None
    if label_review:
        label_started = perf_counter()
        preview = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        preview.alpha_composite(atlas, (0, 0))
        draw_review_labels(preview, sorted_entries)
        preview_path = output_dir / f"{clean_name(group)}-labeled.png"
        save_image_atomic(preview, preview_path)
        labeled_preview_path = norm_path(preview_path, base_dir)
        if profile:
            timings["save_labeled_preview"] = round((perf_counter() - label_started) * 1000, 3)
    atlas_area = width * height
    reserved_tile_area = sum(int(placement["tile_width"]) * int(placement["tile_height"]) for placement in placements)
    padded_asset_area = sum(int(entry["padded_rect"][2]) * int(entry["padded_rect"][3]) for entry in sorted_entries if not entry.get("alias_of"))
    result = {
        "pack_group": group,
        "purpose": "review_validation_atlas",
        "label_overlay": label_review,
        "path": norm_path(path, base_dir),
        "size": [width, height],
        "entry_count": len(entries),
        "physical_entry_count": len(placements),
        "alias_count": len(alias_items),
        "atlas_area": atlas_area,
        "reserved_tile_area": reserved_tile_area,
        "padded_asset_area": padded_asset_area,
        "occupancy_ratio": round(reserved_tile_area / atlas_area, 4) if atlas_area else 0,
        "padded_asset_ratio": round(padded_asset_area / atlas_area, 4) if atlas_area else 0,
        "entries": sorted_entries,
    }
    if labeled_preview_path:
        result["labeled_preview_path"] = labeled_preview_path
        result["labeled_preview_policy"] = {
            "mode": "label_overlay_only",
            "allowed_delta": "review_label_rects_only",
            "debug_outlines": False,
        }
    if profile:
        timings["total"] = round((perf_counter() - started) * 1000, 3)
        result["timing_ms"] = timings
    return result


def build_review_atlas(
    asset_manifest: Path,
    output_dir: Path,
    json_output: Path,
    report_path: Path | None,
    max_size: int,
    label_review: bool,
    label_font_size: int = DEFAULT_LABEL_FONT_SIZE,
    profile: bool = False,
    profile_output: Path | None = None,
    profile_inline: bool = True,
    base_dir: Path | None = None,
) -> dict[str, Any]:
    base_dir = Path.cwd() if base_dir is None else base_dir
    started = perf_counter()
    read_started = started
    manifest = read_json(asset_manifest)
    load_started = perf_counter()
    loaded = load_assets(manifest, base_dir)
    timings: dict[str, float] = {}
    if profile:
        timings["read_manifest"] = round((load_started - read_started) * 1000, 3)
        timings["load_assets"] = round((perf_counter() - load_started) * 1000, 3)
    groups: dict[str, list[dict[str, Any]]] = {}
    for item in loaded:
        groups.setdefault(item["pack_group"], []).append(item)
    pack_started = perf_counter()
    atlases = [pack_group(group, items, output_dir, max_size, label_review, label_font_size, base_dir, profile) for group, items in sorted(groups.items())]
    if profile:
        timings["pack_groups"] = round((perf_counter() - pack_started) * 1000, 3)
    review_atlas_manifest = {
        "schema": "game.review_atlas",
        "version": 1,
        "purpose": "review_validation_atlas",
        "label_overlay": label_review,
        "asset_manifest": norm_path(asset_manifest, base_dir),
        "output_dir": norm_path(output_dir, base_dir),
        "max_size": max_size,
        "atlases": atlases,
    }
    if label_review:
        review_atlas_manifest["labeled_preview_policy"] = {
            "mode": "label_overlay_only",
            "allowed_delta": "review_label_rects_only",
            "debug_outlines": False,
        }
        review_atlas_manifest["label_review_options"] = {"font_size": label_font_size, "outer_margin": LABEL_OUTER_MARGIN}
    if profile:
        efficiency = {
            "atlas_area": sum(int(atlas.get("atlas_area", 0)) for atlas in atlases),
            "reserved_tile_area": sum(int(atlas.get("reserved_tile_area", 0)) for atlas in atlases),
            "padded_asset_area": sum(int(atlas.get("padded_asset_area", 0)) for atlas in atlases),
        }
        atlas_area = int(efficiency["atlas_area"])
        if atlas_area:
            efficiency["occupancy_ratio"] = round(int(efficiency["reserved_tile_area"]) / atlas_area, 4)
            efficiency["padded_asset_ratio"] = round(int(efficiency["padded_asset_area"]) / atlas_area, 4)
        timings["total"] = round((perf_counter() - started) * 1000, 3)
        review_atlas_manifest["atlas_efficiency"] = efficiency
        review_atlas_manifest["timing_ms"] = timings
    profile_report = profile_report_from_review_atlas(review_atlas_manifest) if profile else None
    if profile_output and profile_report:
        write_json(profile_output, profile_report)
    output_manifest = review_atlas_manifest if profile_inline else without_profile_fields(review_atlas_manifest)
    write_json(json_output, output_manifest)
    if report_path:
        lines = [
            "# Review Atlas",
            "",
            "purpose: review/validation atlas, not engine integration output",
            f"asset_manifest: `{output_manifest['asset_manifest']}`",
            f"output_dir: `{output_manifest['output_dir']}`",
            f"atlases: **{len(output_manifest['atlases'])}**",
            "",
        ]
        if output_manifest.get("labeled_preview_policy"):
            policy = output_manifest["labeled_preview_policy"]
            lines.extend(
                [
                    "## Labeled Preview Policy",
                    "",
                    "Names are drawn only on labeled preview PNGs, in reserved free-space label rects outside asset padded rects.",
                    f"- mode: `{policy.get('mode', '-')}`",
                    f"- allowed_delta: `{policy.get('allowed_delta', '-')}`",
                    f"- debug_outlines: `{str(policy.get('debug_outlines', '-')).lower()}`",
                    f"- font_size: `{output_manifest.get('label_review_options', {}).get('font_size', '-')}`",
                    f"- outer_margin: `{output_manifest.get('label_review_options', {}).get('outer_margin', '-')}`",
                    "",
                ]
            )
        if output_manifest.get("atlas_efficiency"):
            efficiency = output_manifest["atlas_efficiency"]
            lines.extend(
                [
                    "## Atlas Efficiency",
                    "",
                    f"- occupancy_ratio: {efficiency.get('occupancy_ratio', '-')}",
                    f"- padded_asset_ratio: {efficiency.get('padded_asset_ratio', '-')}",
                    f"- atlas_area: {efficiency.get('atlas_area', '-')}",
                    f"- reserved_tile_area: {efficiency.get('reserved_tile_area', '-')}",
                    f"- padded_asset_area: {efficiency.get('padded_asset_area', '-')}",
                    "",
                ]
            )
        if output_manifest.get("timing_ms"):
            lines.extend(["## Timing", ""])
            for name, elapsed in output_manifest["timing_ms"].items():
                lines.append(f"- {name}: {elapsed} ms")
            lines.append("")
        lines.extend(["## Atlases", ""])
        for atlas in output_manifest["atlases"]:
            line = f"- `{atlas['pack_group']}` -> `{atlas['path']}` {atlas['size'][0]}x{atlas['size'][1]}, entries={atlas['entry_count']}, physical={atlas['physical_entry_count']}, aliases={atlas['alias_count']}, occupancy={atlas['occupancy_ratio']}"
            if atlas.get("labeled_preview_path"):
                line += f", labeled_preview=`{atlas['labeled_preview_path']}`"
            if atlas.get("labeled_preview_policy"):
                policy = atlas["labeled_preview_policy"]
                line += (
                    ", labels="
                    f"{policy.get('mode', '-')}/"
                    f"{policy.get('allowed_delta', '-')}/"
                    f"debug_outlines={str(policy.get('debug_outlines', '-')).lower()}"
                )
            lines.append(line)
        lines.append("")
        lines.extend(["## Asset Id Index", ""])
        for atlas in output_manifest["atlases"]:
            lines.append(f"### {atlas['pack_group']}")
            lines.append("")
            if atlas.get("labeled_preview_path"):
                lines.append(f"labeled_preview: `{atlas['labeled_preview_path']}`")
                lines.append("")
            for entry in atlas["entries"]:
                details = [
                    f"kind={entry.get('kind') or '-'}",
                    f"source=`{entry.get('source_path') or '-'}`",
                    f"atlas_rect={entry.get('atlas_rect')}",
                    f"padded_rect={entry.get('padded_rect')}",
                ]
                if entry.get("alias_of"):
                    details.append(f"alias_of=`{entry['alias_of']}`")
                review_label = entry.get("review_label") if isinstance(entry.get("review_label"), dict) else None
                if review_label:
                    details.append(f"label_rect={review_label.get('rect')}")
                    details.append(f"label_placement={review_label.get('placement') or '-'}")
                    details.append(f"label_lines={review_label.get('lines') or []}")
                else:
                    details.append("label=alias_reuses_physical_label")
                lines.append(f"- `{entry['id']}`: " + ", ".join(details))
            lines.append("")
        write_text(report_path, "\n".join(lines))
    return review_atlas_manifest


def main(argv: list[str] | None = None, *, project_root: Path | None = None) -> int:
    parser = argparse.ArgumentParser(description="Build review UI atlas PNGs from a prepared asset manifest.")
    parser.add_argument("--asset-manifest", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--json-output")
    parser.add_argument("--report")
    parser.add_argument("--max-size", type=int, default=2048)
    parser.add_argument("--label-review", action="store_true", help="Draw id labels in padding/free space for human review. Do not use this image as a runtime texture.")
    parser.add_argument("--label-font-size", type=int, default=DEFAULT_LABEL_FONT_SIZE, help="Font size for --label-review id labels.")
    parser.add_argument("--profile", action="store_true", help="Record atlas build timing and efficiency metrics in JSON/Markdown and print the slowest atlas group.")
    parser.add_argument("--profile-output", help="Write timing/efficiency telemetry to a sidecar JSON file. When set, profile fields are not embedded in the atlas JSON/Markdown unless --profile-inline is also set.")
    parser.add_argument("--profile-inline", action="store_true", help="Embed profile timing fields in the atlas JSON/Markdown even when --profile-output is used.")
    args = parser.parse_args(argv)
    base_dir = Path.cwd() if project_root is None else project_root

    asset_manifest = project_path(args.asset_manifest, base_dir)
    output_dir = project_path(args.output_dir, base_dir)
    json_output = project_path(args.json_output, base_dir) if args.json_output else output_dir / "review-atlas.json"
    report_path = project_path(args.report, base_dir) if args.report else None
    profile_output = project_path(args.profile_output, base_dir) if args.profile_output else None
    profile_enabled = args.profile or profile_output is not None
    profile_inline = args.profile_inline or profile_output is None
    if args.max_size < 64:
        fail("--max-size must be >= 64")
    if args.label_font_size < 8:
        fail("--label-font-size must be >= 8")
    review_atlas = build_review_atlas(
        asset_manifest,
        output_dir,
        json_output,
        report_path,
        args.max_size,
        args.label_review,
        args.label_font_size,
        profile_enabled,
        profile_output,
        profile_inline,
        base_dir,
    )
    total_entries = sum(atlas["entry_count"] for atlas in review_atlas["atlases"])
    print(f"pass: wrote {total_entries} UI asset id(s) into {len(review_atlas['atlases'])} review atlas image(s)")
    print(f"wrote review atlas manifest: {norm_path(json_output, base_dir)}")
    if profile_output:
        print(f"wrote profile telemetry: {norm_path(profile_output, base_dir)}")
    if profile_enabled and review_atlas["atlases"]:
        slowest = max(review_atlas["atlases"], key=lambda atlas: atlas.get("timing_ms", {}).get("total", 0))
        print(f"profile: slowest atlas group `{slowest['pack_group']}` {slowest.get('timing_ms', {}).get('total', 0)} ms occupancy={slowest.get('occupancy_ratio')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
