#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path
from typing import Any

from PIL import Image
from PIL import ImageDraw


ROOT = Path.cwd()
RESAMPLE_NEAREST = getattr(getattr(Image, "Resampling", Image), "NEAREST", Image.NEAREST)


def fail(message: str) -> None:
    raise SystemExit(f"error: {message}")


def norm_path(path: Path) -> str:
    try:
        return path.resolve().relative_to(ROOT.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def project_path(value: str) -> Path:
    path = Path(value)
    if path.is_absolute():
        return path
    return ROOT / path


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def clean_name(value: str) -> str:
    return re.sub(r"[^a-zA-Z0-9_.-]+", "-", value).strip("-") or "ui_atlas"


def positive_int(value: Any, fallback: int) -> int:
    try:
        parsed = int(value)
    except (TypeError, ValueError):
        return fallback
    return parsed if parsed >= 0 else fallback


def load_assets(manifest: dict[str, Any]) -> list[dict[str, Any]]:
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
        source_path = project_path(path_value)
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


def try_pack(items: list[dict[str, Any]], target_width: int, border: int, shape_padding: int, max_size: int) -> tuple[list[dict[str, Any]], int, int] | None:
    x = border
    y = border
    row_height = 0
    used_width = border
    placements: list[dict[str, Any]] = []
    for item in items:
        image = item["image"]
        extrude = item["extrude"]
        tile_width = image.width + extrude * 2
        tile_height = image.height + extrude * 2
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
    return entry


def draw_review_labels(atlas: Image.Image, entries: list[dict[str, Any]]) -> None:
    draw = ImageDraw.Draw(atlas)
    aliases_by_target: dict[str, list[str]] = {}
    for entry in entries:
        alias_of = entry.get("alias_of")
        if alias_of:
            aliases_by_target.setdefault(str(alias_of), []).append(str(entry["id"]))
    for entry in entries:
        if entry.get("alias_of"):
            continue
        x, y, width, height = rect = [int(value) for value in entry["padded_rect"]]
        label = str(entry["id"])
        aliases = aliases_by_target.get(label, [])
        if aliases:
            label = f"{label} (+{','.join(aliases)})"
        label_width = int(draw.textlength(label))
        label_height = 10
        label_x = x
        label_y = y + height + 2
        if label_y + label_height > atlas.height:
            label_y = max(0, y - label_height - 2)
        if label_x + label_width + 4 > atlas.width:
            label_x = max(0, atlas.width - label_width - 4)
        draw.rectangle([label_x, label_y, label_x + label_width + 4, label_y + label_height + 2], fill=(0, 0, 0, 170))
        draw.text((label_x + 2, label_y + 1), label, fill=(255, 255, 255, 255))
        draw.rectangle([rect[0], rect[1], rect[0] + rect[2] - 1, rect[1] + rect[3] - 1], outline=(255, 255, 255, 110))


def pack_group(group: str, items: list[dict[str, Any]], output_dir: Path, max_size: int, label_review: bool) -> dict[str, Any]:
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

    sorted_items = sorted(canonical_items, key=lambda item: (-item["image"].height, -item["image"].width, item["asset"]["id"]))
    border = max(item["border_padding"] for item in sorted_items)
    shape_padding = max(item["shape_padding"] for item in sorted_items)
    total_area = sum((item["image"].width + item["extrude"] * 2 + shape_padding) * (item["image"].height + item["extrude"] * 2 + shape_padding) for item in sorted_items)
    widest = max(item["image"].width + item["extrude"] * 2 for item in sorted_items) + border * 2
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

    placements, width, height = placement_result
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
    atlas.save(path)
    labeled_preview_path = None
    if label_review:
        preview_height = min(max_size, height + 16)
        preview = Image.new("RGBA", (width, preview_height), (0, 0, 0, 0))
        preview.alpha_composite(atlas, (0, 0))
        draw_review_labels(preview, sorted_entries)
        preview_path = output_dir / f"{clean_name(group)}-labeled.png"
        preview.save(preview_path)
        labeled_preview_path = norm_path(preview_path)
    result = {
        "pack_group": group,
        "purpose": "review_validation_atlas_not_engine_runtime_pack",
        "label_overlay": label_review,
        "path": norm_path(path),
        "size": [width, height],
        "entry_count": len(entries),
        "physical_entry_count": len(placements),
        "alias_count": len(alias_items),
        "entries": sorted_entries,
    }
    if labeled_preview_path:
        result["labeled_preview_path"] = labeled_preview_path
    return result


def build_pack(asset_manifest: Path, output_dir: Path, json_output: Path, report_path: Path | None, max_size: int, label_review: bool) -> dict[str, Any]:
    manifest = read_json(asset_manifest)
    loaded = load_assets(manifest)
    groups: dict[str, list[dict[str, Any]]] = {}
    for item in loaded:
        groups.setdefault(item["pack_group"], []).append(item)
    atlases = [pack_group(group, items, output_dir, max_size, label_review) for group, items in sorted(groups.items())]
    pack_manifest = {
        "schema": "game.ui_atlas_pack",
        "version": 1,
        "purpose": "review_validation_atlas_not_engine_runtime_pack",
        "label_overlay": label_review,
        "asset_manifest": norm_path(asset_manifest),
        "output_dir": norm_path(output_dir),
        "max_size": max_size,
        "atlases": atlases,
    }
    write_json(json_output, pack_manifest)
    if report_path:
        lines = [
            "# UI Atlas Review Pack",
            "",
            "purpose: review/validation atlas, not the engine runtime pack",
            f"asset_manifest: `{pack_manifest['asset_manifest']}`",
            f"output_dir: `{pack_manifest['output_dir']}`",
            f"atlases: **{len(atlases)}**",
            "",
            "## Atlases",
            "",
        ]
        for atlas in atlases:
            line = f"- `{atlas['pack_group']}` -> `{atlas['path']}` {atlas['size'][0]}x{atlas['size'][1]}, entries={atlas['entry_count']}, physical={atlas['physical_entry_count']}, aliases={atlas['alias_count']}"
            if atlas.get("labeled_preview_path"):
                line += f", labeled_preview=`{atlas['labeled_preview_path']}`"
            lines.append(line)
        lines.append("")
        write_text(report_path, "\n".join(lines))
    return pack_manifest


def main() -> None:
    parser = argparse.ArgumentParser(description="Build review UI atlas PNGs from a runtime asset manifest.")
    parser.add_argument("--asset-manifest", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--json-output")
    parser.add_argument("--report")
    parser.add_argument("--max-size", type=int, default=2048)
    parser.add_argument("--label-review", action="store_true", help="Draw id labels in padding/free space for human review. Do not use this image as a runtime texture.")
    args = parser.parse_args()

    asset_manifest = project_path(args.asset_manifest)
    output_dir = project_path(args.output_dir)
    json_output = project_path(args.json_output) if args.json_output else output_dir / "ui-atlas-pack.json"
    report_path = project_path(args.report) if args.report else None
    if args.max_size < 64:
        fail("--max-size must be >= 64")
    pack = build_pack(asset_manifest, output_dir, json_output, report_path, args.max_size, args.label_review)
    total_entries = sum(atlas["entry_count"] for atlas in pack["atlases"])
    print(f"pass: packed {total_entries} UI asset id(s) into {len(pack['atlases'])} review atlas image(s)")
    print(f"wrote atlas manifest: {norm_path(json_output)}")


if __name__ == "__main__":
    main()
