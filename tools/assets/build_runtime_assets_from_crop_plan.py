#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any

from PIL import Image, ImageDraw, ImageFont


ROOT = Path.cwd()
SCRIPT_ROOT = Path(__file__).resolve().parents[2]
if str(SCRIPT_ROOT) not in sys.path:
    sys.path.insert(0, str(SCRIPT_ROOT))

from tools.assets.atomic_io import save_image_atomic, write_json_atomic
from tools.assets.chroma_key_alpha import (
    bleed_transparent_rgb,
    decontaminate_source_key_spill_image,
    is_cyan_key,
    key_to_alpha,
    remove_green_screen_spill,
    repair_transparent_edge_rgb,
    zero_fully_transparent_rgb,
)
from tools.assets.key_matte import key_matte_cutout


ATLAS_POLICY = {
    "trim_mode": "alpha",
    "alpha_bleed": True,
    "premultiply_alpha": True,
    "extrude": 2,
    "shape_padding": 2,
    "border_padding": 1,
    "scale_variant": "1x",
    "allow_rotation": False,
}


def project_path(value: str | Path) -> Path:
    path = Path(value)
    if path.is_absolute():
        return path
    return ROOT / path


def rel(path: Path) -> str:
    resolved = path.resolve()
    try:
        return resolved.relative_to(ROOT.resolve()).as_posix()
    except ValueError:
        return resolved.as_posix()


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise SystemExit(f"{rel(path)} must contain a JSON object")
    return data


def parse_hex_color(value: Any, fallback: tuple[int, int, int] = (0, 255, 0)) -> tuple[int, int, int]:
    if not isinstance(value, str):
        return fallback
    text = value.strip().lstrip("#")
    if len(text) != 6:
        return fallback
    try:
        return (int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16))
    except ValueError:
        return fallback


def alpha_bbox(image: Image.Image, threshold: int = 12) -> tuple[int, int, int, int] | None:
    alpha = image.getchannel("A")
    return alpha.point(lambda value: 255 if value > threshold else 0).getbbox()


def crop_trimmed(
    source: Image.Image,
    crop: dict[str, Any],
    keyed_sources: dict[tuple[int, int, int], Image.Image],
) -> tuple[Image.Image, list[int], list[int]]:
    rect = crop.get("rect")
    if not isinstance(rect, list) or len(rect) != 4:
        raise SystemExit(f"{crop.get('id', 'crop')} needs rect [x,y,w,h]")
    x, y, width, height = [int(value) for value in rect]
    if width <= 0 or height <= 0:
        raise SystemExit(f"{crop.get('id', 'crop')} rect must have positive size")
    chroma = crop.get("chroma_key") if isinstance(crop.get("chroma_key"), dict) else {}
    key = parse_hex_color(chroma.get("key"))
    method = str(chroma.get("method", "chroma"))
    if method == "key_matte":
        # Opt-in principled matte (trimap -> closed-form -> ML decontamination).
        # Runs PER CROP, not on the whole sheet, because the closed-form solve is
        # global; for opaque art + flat-key holes it replaces the despill heuristics.
        rgba = key_matte_cutout(source.crop((x, y, x + width, y + height)), key)
    else:
        keyed_source = keyed_sources.get(key)
        if keyed_source is None:
            keyed_source = key_to_alpha(source, key=key)
            keyed_sources[key] = keyed_source
        rgba = keyed_source.crop((x, y, x + width, y + height))
    bbox = alpha_bbox(rgba)
    if bbox is None:
        raise SystemExit(f"{crop.get('id', 'crop')} produced empty alpha after keying")
    trim = crop.get("trim") if isinstance(crop.get("trim"), dict) else {}
    padding = int(trim.get("padding", 8))
    left, top, right, bottom = bbox
    trim_left = max(0, left - padding)
    trim_top = max(0, top - padding)
    trim_right = min(rgba.width, right + padding)
    trim_bottom = min(rgba.height, bottom + padding)
    out = rgba.crop((trim_left, trim_top, trim_right, trim_bottom))
    if is_cyan_key(key):
        decontaminate_source_key_spill_image(out, key=key, require_transparent_touch=False)
    remove_green_screen_spill(out, passes=10, radius=4)
    bleed_transparent_rgb(out, key=key)
    repair_transparent_edge_rgb(out, key=key)
    zero_fully_transparent_rgb(out)
    return out, [trim_left, trim_top, trim_right - trim_left, trim_bottom - trim_top], [x, y, width, height]


def runtime_kind(plan_kind: str) -> str:
    if plan_kind == "decor":
        return "decor_overlay"
    return plan_kind


def anchor_for(asset_id: str, kind: str) -> str:
    if kind != "decor_overlay":
        return "center"
    if "top_left" in asset_id:
        return "top_left"
    if "top_right" in asset_id:
        return "top_right"
    if "bottom_left" in asset_id:
        return "bottom_left"
    if "bottom_right" in asset_id:
        return "bottom_right"
    if "divider" in asset_id:
        return "center"
    return "center"


def asset_entry(crop: dict[str, Any], image: Image.Image, trim_rect: list[int]) -> dict[str, Any]:
    asset_id = str(crop["id"])
    kind = runtime_kind(str(crop.get("kind") or "sprite"))
    atlas = crop.get("atlas") if isinstance(crop.get("atlas"), dict) else {}
    policy = dict(ATLAS_POLICY)
    policy["extrude"] = int(atlas.get("extrude", policy["extrude"]))
    policy["shape_padding"] = int(atlas.get("shape_padding", policy["shape_padding"]))
    policy["allow_rotation"] = bool(atlas.get("allow_rotation", False))
    entry: dict[str, Any] = {
        "id": asset_id,
        "kind": kind,
        "path": crop["output"],
        "pack_group": str(atlas.get("pack_group") or "ui_common"),
        "source_crop": asset_id,
        "original_size": [image.width, image.height],
        "trim_rect": trim_rect,
        "atlas_policy": policy,
        "source_component_id": crop.get("source_component_id"),
        "component_bbox": crop.get("component_bbox"),
        "semantic_role": crop.get("semantic_role") or asset_id,
        "anchor": anchor_for(asset_id, kind),
    }
    if kind == "icon":
        entry["pivot"] = [0.5, 0.5]
        entry["size_class"] = crop.get("size_class", "96px_source")
        entry["preview_sizes"] = crop.get("preview_sizes", [[32, 32], [48, 48]])
    elif kind == "slice9":
        policy["trim_preserves_slice9"] = True
        entry["atlas_policy"] = policy
        for field in (
            "slice9",
            "content",
            "content_rect",
            "target_preview_sizes",
            "preview_sizes",
            "stretch_policy",
            "usage_policy",
            "state_role",
        ):
            if field in crop:
                entry[field] = crop[field]
    elif kind == "decor_overlay":
        entry["z_order"] = 20
        entry["allowed_base_ids"] = ["compact_journal_panel_v5", "compact_button_default_long_v5", "compact_button_default_medium_v5", "compact_button_default_short_v5"]
        entry["offset_bounds"] = {"x": [-512, 512], "y": [-512, 512]}
    return entry


def checkerboard(size: tuple[int, int], cell: int = 16) -> Image.Image:
    image = Image.new("RGBA", size, (38, 35, 40, 255))
    draw = ImageDraw.Draw(image)
    for y in range(0, size[1], cell):
        for x in range(0, size[0], cell):
            if (x // cell + y // cell) % 2 == 0:
                draw.rectangle((x, y, x + cell - 1, y + cell - 1), fill=(52, 48, 54, 255))
    return image


def font(size: int = 12) -> ImageFont.ImageFont:
    for name in ("DejaVuSans.ttf", "Arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def contact_sheet(items: list[tuple[dict[str, Any], Image.Image]]) -> Image.Image:
    cell_w, cell_h = 220, 220
    cols = 4
    rows = max(1, (len(items) + cols - 1) // cols)
    sheet = checkerboard((cols * cell_w, rows * cell_h), 18)
    draw = ImageDraw.Draw(sheet)
    for index, (crop, image) in enumerate(items):
        col = index % cols
        row = index // cols
        x = col * cell_w
        y = row * cell_h
        draw.rectangle((x + 8, y + 8, x + cell_w - 8, y + cell_h - 8), outline=(122, 104, 66, 255), width=2)
        max_w = cell_w - 36
        max_h = cell_h - 58
        scale = min(max_w / image.width, max_h / image.height, 1.0)
        preview = image.resize((max(1, round(image.width * scale)), max(1, round(image.height * scale))), Image.Resampling.LANCZOS)
        sheet.alpha_composite(preview, (x + (cell_w - preview.width) // 2, y + 18))
        draw.text((x + 12, y + cell_h - 34), str(crop["id"]), fill=(238, 232, 214, 255), font=font(11))
    return sheet


def build(args: argparse.Namespace) -> None:
    plan_path = project_path(args.crop_plan)
    plan = load_json(plan_path)
    if plan.get("schema") != "game.runtime_crop_plan":
        raise SystemExit("--crop-plan must point to a game.runtime_crop_plan JSON")
    source_path = project_path(str(plan.get("source") or ""))
    if not source_path.exists():
        raise SystemExit(f"missing source sheet: {rel(source_path)}")
    crops = plan.get("crops")
    if not isinstance(crops, list) or not crops:
        raise SystemExit("crop plan must contain non-empty crops")
    source = Image.open(source_path).convert("RGBA")
    keyed_sources: dict[tuple[int, int, int], Image.Image] = {}
    outputs: list[tuple[dict[str, Any], Image.Image]] = []
    runtime_assets: list[dict[str, Any]] = []
    manifest_crops: list[dict[str, Any]] = []
    for crop in crops:
        if not isinstance(crop, dict) or not isinstance(crop.get("id"), str) or not isinstance(crop.get("output"), str):
            raise SystemExit("each crop plan entry needs id and output")
        image, trim_rect, source_rect = crop_trimmed(source, crop, keyed_sources)
        output_path = project_path(crop["output"])
        output_path.parent.mkdir(parents=True, exist_ok=True)
        save_image_atomic(image, output_path)
        crop_manifest_entry = dict(crop)
        crop_manifest_entry["kind"] = runtime_kind(str(crop.get("kind") or "sprite"))
        crop_manifest_entry["source_rect"] = source_rect
        crop_manifest_entry["trim_rect"] = trim_rect
        crop_manifest_entry["trim_padding"] = int((crop.get("trim") or {}).get("padding", 8)) if isinstance(crop.get("trim"), dict) else 8
        crop_manifest_entry["min_output_padding"] = int(args.min_output_padding)
        crop_manifest_entry["anchor"] = anchor_for(str(crop_manifest_entry["id"]), crop_manifest_entry["kind"])
        if crop_manifest_entry["kind"] == "icon":
            crop_manifest_entry["pivot"] = [0.5, 0.5]
        if crop_manifest_entry["kind"] == "icon":
            crop_manifest_entry["isolate_component"] = str(crop.get("source_component_id") or "source_component_bbox")
        manifest_crops.append(crop_manifest_entry)
        runtime_assets.append(asset_entry(crop_manifest_entry, image, trim_rect))
        outputs.append((crop_manifest_entry, image))
    manifest_key = "#00ff00"
    if isinstance(plan.get("green_screen"), dict) and isinstance(plan["green_screen"].get("key"), str):
        manifest_key = plan["green_screen"]["key"]
    else:
        for crop in manifest_crops:
            if isinstance(crop.get("chroma_key"), dict) and isinstance(crop["chroma_key"].get("key"), str):
                manifest_key = crop["chroma_key"]["key"]
                break
    crop_manifest = {
        "schema": "game.art_crop_manifest",
        "version": 1,
        "art_job": rel(project_path(args.art_job)),
        "output_dir": str(plan.get("output_dir") or "").replace("\\", "/"),
        "green_screen": {"mode": "chroma_key", "key": manifest_key, "notes": "Runtime assets cut from accepted source-sheet intake crop plan."},
        "sources": [
            {
                "id": plan.get("source_id"),
                "path": plan.get("source"),
                "source_role": plan.get("source_role"),
                "crop_plan": rel(plan_path),
                "crops": manifest_crops,
            }
        ],
    }
    crop_manifest_path = project_path(args.crop_manifest)
    write_json_atomic(crop_manifest_path, crop_manifest)
    runtime_manifest = {
        "schema": "game.asset_manifest",
        "version": 1,
        "art_job": rel(project_path(args.art_job)),
        "crop_manifest": rel(crop_manifest_path),
        "runtime_dir": str(plan.get("output_dir") or "").replace("\\", "/"),
        "source_art": plan.get("source"),
        "source_policy": "real generated bitmap source; crop plan generated from source-sheet intake components",
        "assets": runtime_assets,
    }
    runtime_manifest_path = project_path(args.asset_manifest)
    write_json_atomic(runtime_manifest_path, runtime_manifest)
    if args.contact_sheet:
        save_image_atomic(contact_sheet(outputs), project_path(args.contact_sheet))
    print(f"wrote {len(outputs)} runtime assets from {rel(plan_path)}")
    print(f"wrote crop manifest: {rel(crop_manifest_path)}")
    print(f"wrote asset manifest: {rel(runtime_manifest_path)}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Build runtime PNGs and manifests from a named source-sheet crop plan.")
    parser.add_argument("--crop-plan", required=True)
    parser.add_argument("--crop-manifest", required=True)
    parser.add_argument("--asset-manifest", required=True)
    parser.add_argument("--art-job", required=True)
    parser.add_argument("--contact-sheet")
    parser.add_argument("--min-output-padding", type=int, default=4)
    args = parser.parse_args()
    build(args)


if __name__ == "__main__":
    main()
