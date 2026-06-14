#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path
import sys
from typing import Any

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.assets.chroma_key_alpha import key_to_alpha, resize_rgba_premultiplied

PROJECT_DIR = ROOT / "gamedesign/projects/rune-marches"
SOURCE = PROJECT_DIR / "art/source_sheets/rune-marches-blank-ui-bases-source-v2-chroma-clean.png"
ART_JOB = "gamedesign/projects/rune-marches/art_requests/rune-marches-ui-map-rescue-v2.json"
OUT_DIR = ROOT / "assets/runtime/rune-marches-ui-bases-v2"
PREVIEW_DIR = PROJECT_DIR / "art/previews"
CROP_MANIFEST = PROJECT_DIR / "data/rune-marches-ui-bases-v2-crop_manifest.json"
RUNTIME_MANIFEST = PROJECT_DIR / "data/rune-marches-ui-bases-v2-asset_manifest.json"


CROPS: list[dict[str, Any]] = [
    {
        "id": "modal_panel_v2",
        "kind": "slice9",
        "rect": [49, 121, 709, 592],
        "output": "assets/runtime/rune-marches-ui-bases-v2/modal_panel_v2_slice9.png",
        "slice9": {"left": 104, "top": 100, "right": 104, "bottom": 100},
        "min_edge_padding": 10,
        "content": {"x": 132, "y": 116, "w": 445, "h": 360},
        "target_preview_sizes": [[360, 240], [520, 320], [760, 420]],
        "stretch_policy": {
            "center": "plain_texture",
            "horizontal_edges": "straight_frame",
            "vertical_edges": "straight_frame",
            "corners": "decorative_fixed",
            "non_stretch_ornaments": "corner_only",
        },
        "usage_policy": {
            "size_class": "flexible",
            "min_size": [360, 240],
            "disallowed_uses": [],
        },
        "role": "large objective or modal panel",
        "state": "default",
    },
    {
        "id": "journal_panel_v2",
        "kind": "slice9",
        "rect": [845, 121, 357, 639],
        "output": "assets/runtime/rune-marches-ui-bases-v2/journal_panel_v2_slice9.png",
        "slice9": {"left": 84, "top": 104, "right": 84, "bottom": 104},
        "min_edge_padding": 10,
        "content": {"x": 94, "y": 120, "w": 169, "h": 399},
        "target_preview_sizes": [[220, 280], [300, 380], [360, 460]],
        "stretch_policy": {
            "center": "plain_texture",
            "horizontal_edges": "straight_frame",
            "vertical_edges": "straight_frame",
            "corners": "decorative_fixed",
            "non_stretch_ornaments": "corner_only",
        },
        "usage_policy": {
            "size_class": "flexible",
            "min_size": [220, 280],
            "disallowed_uses": [],
        },
        "role": "quest journal side panel",
        "state": "default",
    },
    {
        "id": "button_idle_v2",
        "kind": "slice9",
        "rect": [61, 893, 504, 246],
        "output": "assets/runtime/rune-marches-ui-bases-v2/button_idle_v2_slice9.png",
        "slice9": {"left": 96, "top": 44, "right": 96, "bottom": 44},
        "min_edge_padding": 10,
        "content": {"x": 120, "y": 78, "w": 264, "h": 90},
        "target_preview_sizes": [[280, 104], [360, 120], [480, 140]],
        "stretch_policy": {
            "center": "plain_texture",
            "horizontal_edges": "straight_frame",
            "vertical_edges": "straight_frame",
            "corners": "decorative_fixed",
            "non_stretch_ornaments": "corner_only",
        },
        "usage_policy": {
            "size_class": "large_only",
            "min_size": [280, 104],
            "disallowed_uses": ["compact_secondary_button", "mobile_dense_button_row"],
        },
        "role": "large primary action button; not suitable for compact secondary buttons",
        "state": "idle",
    },
    {
        "id": "button_disabled_v2",
        "kind": "slice9",
        "rect": [687, 893, 502, 246],
        "output": "assets/runtime/rune-marches-ui-bases-v2/button_disabled_v2_slice9.png",
        "slice9": {"left": 96, "top": 44, "right": 96, "bottom": 44},
        "min_edge_padding": 10,
        "content": {"x": 120, "y": 78, "w": 262, "h": 90},
        "target_preview_sizes": [[280, 104], [360, 120], [480, 140]],
        "stretch_policy": {
            "center": "plain_texture",
            "horizontal_edges": "straight_frame",
            "vertical_edges": "straight_frame",
            "corners": "decorative_fixed",
            "non_stretch_ornaments": "corner_only",
        },
        "usage_policy": {
            "size_class": "large_only",
            "min_size": [280, 104],
            "disallowed_uses": ["compact_secondary_button", "mobile_dense_button_row"],
        },
        "role": "large disabled action button; not suitable for compact secondary buttons",
        "state": "disabled",
    },
]


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def crop_asset(source: Image.Image, crop: dict[str, Any]) -> Image.Image:
    x, y, width, height = [int(value) for value in crop["rect"]]
    return key_to_alpha(source.crop((x, y, x + width, y + height)), aggressive_visible_decontaminate=True)


def validate_crop(crop: dict[str, Any], image: Image.Image) -> None:
    if crop["kind"] != "slice9":
        return
    margins = crop["slice9"]
    left = int(margins["left"])
    top = int(margins["top"])
    right = int(margins["right"])
    bottom = int(margins["bottom"])
    if left + right >= image.width or top + bottom >= image.height:
        raise ValueError(f"{crop['id']} slice9 margins exceed source size")
    for width, height in crop["target_preview_sizes"]:
        if left + right >= width or top + bottom >= height:
            raise ValueError(f"{crop['id']} target preview {width}x{height} is smaller than slice margins")


def nine_slice_resize(image: Image.Image, margins: dict[str, int], size: tuple[int, int]) -> Image.Image:
    left = int(margins["left"])
    top = int(margins["top"])
    right = int(margins["right"])
    bottom = int(margins["bottom"])
    width, height = image.size
    out_width, out_height = size
    result = Image.new("RGBA", size, (0, 0, 0, 0))

    src_x = [0, left, width - right, width]
    src_y = [0, top, height - bottom, height]
    dst_x = [0, left, out_width - right, out_width]
    dst_y = [0, top, out_height - bottom, out_height]

    for row in range(3):
        for col in range(3):
            src_box = (src_x[col], src_y[row], src_x[col + 1], src_y[row + 1])
            dst_box = (dst_x[col], dst_y[row], dst_x[col + 1], dst_y[row + 1])
            tile = image.crop(src_box)
            dst_w = max(1, dst_box[2] - dst_box[0])
            dst_h = max(1, dst_box[3] - dst_box[1])
            if tile.size != (dst_w, dst_h):
                tile = resize_rgba_premultiplied(tile, (dst_w, dst_h))
            result.alpha_composite(tile, (dst_box[0], dst_box[1]))
    return result


def checkerboard(size: tuple[int, int], cell: int = 16) -> Image.Image:
    image = Image.new("RGBA", size, (38, 35, 40, 255))
    draw = ImageDraw.Draw(image)
    for y in range(0, size[1], cell):
        for x in range(0, size[0], cell):
            if (x // cell + y // cell) % 2 == 0:
                draw.rectangle((x, y, x + cell - 1, y + cell - 1), fill=(52, 48, 54, 255))
    return image


def font(size: int = 14) -> ImageFont.ImageFont:
    try:
        return ImageFont.truetype("arial.ttf", size)
    except OSError:
        return ImageFont.load_default()


def draw_label(draw: ImageDraw.ImageDraw, xy: tuple[int, int], text: str) -> None:
    draw.text(xy, text, fill=(238, 232, 214, 255), font=font(14))


def make_contact_sheet(outputs: list[tuple[dict[str, Any], Image.Image]]) -> Image.Image:
    cell_w, cell_h = 310, 230
    cols = 2
    rows = (len(outputs) + cols - 1) // cols
    sheet = checkerboard((cols * cell_w, rows * cell_h), 18)
    draw = ImageDraw.Draw(sheet)
    for index, (crop, image) in enumerate(outputs):
        col = index % cols
        row = index // cols
        x = col * cell_w
        y = row * cell_h
        draw.rectangle((x + 8, y + 8, x + cell_w - 8, y + cell_h - 8), outline=(122, 104, 66, 255), width=2)
        max_w = cell_w - 32
        max_h = cell_h - 52
        scale = min(max_w / image.width, max_h / image.height, 1.0)
        preview = resize_rgba_premultiplied(image, (max(1, round(image.width * scale)), max(1, round(image.height * scale))))
        sheet.alpha_composite(preview, (x + (cell_w - preview.width) // 2, y + 24))
        draw_label(draw, (x + 16, y + cell_h - 24), crop["id"])
    return sheet


def make_slice_preview(outputs_by_id: dict[str, Image.Image]) -> Image.Image:
    rows: list[Image.Image] = []
    for crop in CROPS:
        image = outputs_by_id[crop["id"]]
        variants = [nine_slice_resize(image, crop["slice9"], tuple(size)) for size in crop["target_preview_sizes"]]
        row_w = sum(item.width for item in variants) + 24 * (len(variants) + 1)
        row_h = max(item.height for item in variants) + 54
        row = checkerboard((row_w, row_h), 18)
        draw = ImageDraw.Draw(row)
        draw_label(draw, (14, 8), f"{crop['id']}  slice9={crop['slice9']}")
        x = 24
        for item in variants:
            row.alpha_composite(item, (x, 42))
            x += item.width + 24
        rows.append(row)

    width = max(row.width for row in rows)
    height = sum(row.height for row in rows)
    sheet = checkerboard((width, height), 18)
    y = 0
    for row in rows:
        sheet.alpha_composite(row, (0, y))
        y += row.height
    return sheet


def write_manifests() -> None:
    crop_manifest = {
        "schema": "game.art_crop_manifest",
        "version": 1,
        "art_job": ART_JOB,
        "output_dir": rel(OUT_DIR),
        "green_screen": {
            "mode": "chroma_key",
            "key": "#ff00ff",
            "notes": "Builder removes only border-connected chroma pixels from a real generated source sheet. It does not redraw or procedurally replace the UI art.",
        },
        "sources": [
            {
                "id": "rune_marches_blank_ui_bases_source_v2",
                "path": rel(SOURCE),
                "generator": "built-in image generation",
                "source_role": "blank generated UI bases: panels and buttons only, no baked text",
                "crops": CROPS,
            }
        ],
        "preview_evidence": [
            "gamedesign/projects/rune-marches/art/previews/rune-marches-ui-bases-v2-contact-sheet.png",
            "gamedesign/projects/rune-marches/art/previews/rune-marches-ui-bases-v2-slice9-preview.png",
        ],
    }
    CROP_MANIFEST.write_text(json.dumps(crop_manifest, indent=2) + "\n", encoding="utf-8")

    runtime_manifest = {
        "schema": "game.asset_manifest",
        "version": 1,
        "art_job": ART_JOB,
        "crop_manifest": rel(CROP_MANIFEST),
        "runtime_dir": rel(OUT_DIR),
        "source_art": rel(SOURCE),
        "source_policy": "real generated bitmap source; no procedural replacement",
        "commands": {
            "slice_assets": "py -3.12 tools/assets/build_rune_marches_ui_bases_v2.py",
            "asset_audit": "py -3.12 tools/assets/audit_generated_ui_assets.py --crop-manifest gamedesign/projects/rune-marches/data/rune-marches-ui-bases-v2-crop_manifest.json --json-output gamedesign/projects/rune-marches/reviews/rune-marches-ui-bases-v2-asset-audit.json --report gamedesign/projects/rune-marches/reviews/rune-marches-ui-bases-v2-asset-audit.md",
            "source_derivation_audit": "py -3.12 tools/assets/audit_generated_source_derivation.py --crop-manifest gamedesign/projects/rune-marches/data/rune-marches-ui-bases-v2-crop_manifest.json --json-output gamedesign/projects/rune-marches/reviews/rune-marches-ui-bases-v2-source-derivation-audit.json --report gamedesign/projects/rune-marches/reviews/rune-marches-ui-bases-v2-source-derivation-audit.md",
        },
        "assets": [
            {
                "id": crop["id"],
                "kind": crop["kind"],
                "path": crop["output"],
                "slice9": crop["slice9"],
                "content": crop["content"],
                "target_preview_sizes": crop["target_preview_sizes"],
                "stretch_policy": crop["stretch_policy"],
                "usage_policy": crop["usage_policy"],
                "role": crop["role"],
                "state": crop["state"],
            }
            for crop in CROPS
        ],
        "preview_evidence": [
            "gamedesign/projects/rune-marches/art/previews/rune-marches-ui-bases-v2-contact-sheet.png",
            "gamedesign/projects/rune-marches/art/previews/rune-marches-ui-bases-v2-slice9-preview.png",
        ],
        "asset_audit": [
            "gamedesign/projects/rune-marches/reviews/rune-marches-ui-bases-v2-asset-audit.md",
            "gamedesign/projects/rune-marches/reviews/rune-marches-ui-bases-v2-asset-audit.json",
        ],
        "source_derivation_audit": [
            "gamedesign/projects/rune-marches/reviews/rune-marches-ui-bases-v2-source-derivation-audit.md",
            "gamedesign/projects/rune-marches/reviews/rune-marches-ui-bases-v2-source-derivation-audit.json",
        ],
    }
    RUNTIME_MANIFEST.write_text(json.dumps(runtime_manifest, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    if not SOURCE.exists():
        raise SystemExit(f"missing source sheet: {SOURCE}")
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    CROP_MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    RUNTIME_MANIFEST.parent.mkdir(parents=True, exist_ok=True)

    source = Image.open(SOURCE).convert("RGBA")
    outputs: list[tuple[dict[str, Any], Image.Image]] = []
    outputs_by_id: dict[str, Image.Image] = {}
    for crop in CROPS:
        image = crop_asset(source, crop)
        validate_crop(crop, image)
        output_path = ROOT / crop["output"]
        output_path.parent.mkdir(parents=True, exist_ok=True)
        image.save(output_path)
        outputs.append((crop, image))
        outputs_by_id[crop["id"]] = image

    make_contact_sheet(outputs).save(PREVIEW_DIR / "rune-marches-ui-bases-v2-contact-sheet.png")
    make_slice_preview(outputs_by_id).save(PREVIEW_DIR / "rune-marches-ui-bases-v2-slice9-preview.png")
    write_manifests()

    print(f"wrote {len(outputs)} generated-source assets to {rel(OUT_DIR)}")
    print(f"wrote crop manifest: {rel(CROP_MANIFEST)}")
    print(f"wrote runtime manifest: {rel(RUNTIME_MANIFEST)}")
    print(f"wrote previews to {rel(PREVIEW_DIR)}")


if __name__ == "__main__":
    main()
