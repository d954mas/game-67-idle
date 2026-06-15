#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import sys
from typing import Any

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.assets.atomic_io import save_image_atomic, write_json_atomic
from tools.assets.chroma_key_alpha import (
    bleed_transparent_rgb,
    key_to_alpha,
    repair_transparent_edge_rgb,
    resize_rgba_premultiplied,
    zero_fully_transparent_rgb,
)


PROJECT_DIR = ROOT / "gamedesign/projects/rune-marches"
SOURCE = PROJECT_DIR / "art/source_sheets/rune-marches-compact-ui-bases-source-v5-chroma-clean.png"
ART_JOB = "gamedesign/projects/rune-marches/art_requests/rune-marches-ui-map-rescue-v2.json"
OUT_DIR = ROOT / "assets/runtime/rune-marches-ui-compact-bases-v5"
PREVIEW_DIR = PROJECT_DIR / "art/previews"
CROP_MANIFEST = PROJECT_DIR / "data/rune-marches-ui-compact-bases-v5-crop_manifest.json"
RUNTIME_MANIFEST = PROJECT_DIR / "data/rune-marches-ui-compact-bases-v5-asset_manifest.json"
ATLAS_AUDIT_JSON = PROJECT_DIR / "reviews/rune-marches-ui-compact-bases-v5-atlas-metadata-audit.json"
ATLAS_AUDIT_MD = PROJECT_DIR / "reviews/rune-marches-ui-compact-bases-v5-atlas-metadata-audit.md"
ATLAS_PACK_JSON = PROJECT_DIR / "data/rune-marches-ui-compact-bases-v5-atlas_pack.json"
ATLAS_PACK_MD = PROJECT_DIR / "reviews/rune-marches-ui-compact-bases-v5-atlas-pack.md"
ATLAS_PACK_AUDIT_JSON = PROJECT_DIR / "reviews/rune-marches-ui-compact-bases-v5-atlas-pack-audit.json"
ATLAS_PACK_AUDIT_MD = PROJECT_DIR / "reviews/rune-marches-ui-compact-bases-v5-atlas-pack-audit.md"
ATLAS_PACK_DIR = ROOT / "assets/runtime/rune-marches-ui-compact-bases-v5-atlas"
EDGE_PROOF_PNG = PROJECT_DIR / "art/previews/rune-marches-ui-compact-bases-v5-edge-proof.png"
EDGE_PROOF_JSON = PROJECT_DIR / "reviews/rune-marches-ui-compact-bases-v5-edge-proof.json"
EDGE_PROOF_MD = PROJECT_DIR / "reviews/rune-marches-ui-compact-bases-v5-edge-proof.md"


BUTTON_STRETCH = {
    "center": "plain_texture",
    "horizontal_edges": "straight_frame",
    "vertical_edges": "straight_frame",
    "corners": "decorative_fixed",
    "non_stretch_ornaments": "corner_only",
}


ATLAS_POLICY_SLICE9 = {
    "trim_mode": "alpha",
    "alpha_bleed": True,
    "premultiply_alpha": True,
    "extrude": 2,
    "shape_padding": 2,
    "border_padding": 1,
    "scale_variant": "1x",
    "allow_rotation": False,
    "trim_preserves_slice9": True,
}


CROPS: list[dict[str, Any]] = [
    {
        "id": "compact_button_idle_short_v5",
        "kind": "slice9",
        "rect": [59, 200, 211, 118],
        "output": "assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_idle_short_v5_slice9.png",
        "slice9": {"left": 48, "top": 22, "right": 48, "bottom": 22},
        "min_edge_padding": 4,
        "content": {"x": 58, "y": 34, "w": 95, "h": 50},
        "target_preview_sizes": [[160, 72], [184, 72], [203, 72]],
        "stretch_policy": BUTTON_STRETCH,
        "usage_policy": {"size_class": "compact_only", "min_size": [160, 72], "disallowed_uses": ["large_primary_button"]},
        "role": "compact short idle button for dense secondary actions",
        "state": "idle",
    },
    {
        "id": "compact_button_idle_medium_v5",
        "kind": "slice9",
        "rect": [307, 199, 298, 119],
        "output": "assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_idle_medium_v5_slice9.png",
        "slice9": {"left": 48, "top": 22, "right": 48, "bottom": 22},
        "min_edge_padding": 4,
        "content": {"x": 58, "y": 34, "w": 182, "h": 51},
        "target_preview_sizes": [[220, 72], [256, 72], [290, 72]],
        "stretch_policy": BUTTON_STRETCH,
        "usage_policy": {"size_class": "compact_only", "min_size": [220, 72], "disallowed_uses": ["large_primary_button"]},
        "role": "compact medium idle button for portrait primary and secondary actions",
        "state": "idle",
    },
    {
        "id": "compact_button_idle_long_v5",
        "kind": "slice9",
        "rect": [638, 199, 444, 119],
        "output": "assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_idle_long_v5_slice9.png",
        "slice9": {"left": 48, "top": 22, "right": 48, "bottom": 22},
        "min_edge_padding": 4,
        "content": {"x": 58, "y": 34, "w": 328, "h": 51},
        "target_preview_sizes": [[320, 72], [376, 72], [436, 72]],
        "stretch_policy": BUTTON_STRETCH,
        "usage_policy": {"size_class": "compact_only", "min_size": [320, 72], "disallowed_uses": ["large_primary_button"]},
        "role": "compact long idle button for full-width portrait action",
        "state": "idle",
    },
    {
        "id": "compact_button_disabled_short_v5",
        "kind": "slice9",
        "rect": [59, 385, 211, 119],
        "output": "assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_disabled_short_v5_slice9.png",
        "slice9": {"left": 48, "top": 22, "right": 48, "bottom": 22},
        "min_edge_padding": 4,
        "content": {"x": 58, "y": 34, "w": 95, "h": 51},
        "target_preview_sizes": [[160, 72], [184, 72], [203, 72]],
        "stretch_policy": BUTTON_STRETCH,
        "usage_policy": {"size_class": "compact_only", "min_size": [160, 72], "disallowed_uses": ["large_primary_button"]},
        "role": "compact short disabled button for dense secondary actions",
        "state": "disabled",
    },
    {
        "id": "compact_button_disabled_medium_v5",
        "kind": "slice9",
        "rect": [307, 385, 298, 119],
        "output": "assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_disabled_medium_v5_slice9.png",
        "slice9": {"left": 48, "top": 22, "right": 48, "bottom": 22},
        "min_edge_padding": 4,
        "content": {"x": 58, "y": 34, "w": 182, "h": 51},
        "target_preview_sizes": [[220, 72], [256, 72], [290, 72]],
        "stretch_policy": BUTTON_STRETCH,
        "usage_policy": {"size_class": "compact_only", "min_size": [220, 72], "disallowed_uses": ["large_primary_button"]},
        "role": "compact medium disabled button for portrait primary and secondary actions",
        "state": "disabled",
    },
    {
        "id": "compact_button_disabled_long_v5",
        "kind": "slice9",
        "rect": [637, 385, 445, 119],
        "output": "assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_disabled_long_v5_slice9.png",
        "slice9": {"left": 48, "top": 22, "right": 48, "bottom": 22},
        "min_edge_padding": 4,
        "content": {"x": 58, "y": 34, "w": 329, "h": 51},
        "target_preview_sizes": [[320, 72], [376, 72], [437, 72]],
        "stretch_policy": BUTTON_STRETCH,
        "usage_policy": {"size_class": "compact_only", "min_size": [320, 72], "disallowed_uses": ["large_primary_button"]},
        "role": "compact long disabled button for full-width portrait action",
        "state": "disabled",
    },
    {
        "id": "compact_journal_panel_v5",
        "kind": "slice9",
        "rect": [1145, 127, 346, 722],
        "output": "assets/runtime/rune-marches-ui-compact-bases-v5/compact_journal_panel_v5_slice9.png",
        "slice9": {"left": 76, "top": 76, "right": 76, "bottom": 76},
        "min_edge_padding": 4,
        "content": {"x": 86, "y": 90, "w": 174, "h": 540},
        "target_preview_sizes": [[220, 280], [300, 380], [338, 520]],
        "stretch_policy": {
            "center": "plain_texture",
            "horizontal_edges": "straight_frame",
            "vertical_edges": "straight_frame",
            "corners": "decorative_fixed",
            "non_stretch_ornaments": "corner_only",
        },
        "usage_policy": {"size_class": "compact_only", "min_size": [220, 280], "disallowed_uses": ["wide_desktop_modal"]},
        "role": "compact portrait journal/objective panel",
        "state": "default",
    },
]


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def atlas_fields(crop: dict[str, Any]) -> dict[str, Any]:
    width = int(crop["rect"][2])
    height = int(crop["rect"][3])
    return {
        "pack_group": "ui_rune_marches_compact_bases_v5",
        "source_crop": crop["id"],
        "original_size": [width, height],
        "trim_rect": [0, 0, width, height],
        "atlas_policy": ATLAS_POLICY_SLICE9,
    }


def crop_asset(source: Image.Image, crop: dict[str, Any]) -> Image.Image:
    x, y, width, height = [int(value) for value in crop["rect"]]
    image = key_to_alpha(source.crop((x, y, x + width, y + height)), key=(0, 255, 0))
    pixels = image.load()
    for py in range(image.height):
        for px in range(image.width):
            red, green, blue, alpha = pixels[px, py]
            if alpha <= 12 and green > 210 and red < 80 and blue < 80:
                pixels[px, py] = (0, 0, 0, 0)
    bleed_transparent_rgb(image, key=(0, 255, 0))
    repair_transparent_edge_rgb(image, key=(0, 255, 0))
    zero_fully_transparent_rgb(image)
    return image


def validate_crop(crop: dict[str, Any], image: Image.Image) -> None:
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


def make_contact_sheet(outputs: list[tuple[dict[str, Any], Image.Image]]) -> Image.Image:
    cell_w, cell_h = 290, 210
    cols = 3
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
        max_h = cell_h - 48
        scale = min(max_w / image.width, max_h / image.height, 1.0)
        preview = resize_rgba_premultiplied(image, (max(1, round(image.width * scale)), max(1, round(image.height * scale))))
        sheet.alpha_composite(preview, (x + (cell_w - preview.width) // 2, y + 22))
        draw.text((x + 14, y + cell_h - 24), crop["id"], fill=(238, 232, 214, 255), font=font(12))
    return sheet


def make_slice_preview(outputs_by_id: dict[str, Image.Image]) -> Image.Image:
    preview_specs = [
        ("compact_button_idle_short_v5", (160, 72)),
        ("compact_button_idle_medium_v5", (220, 72)),
        ("compact_button_idle_long_v5", (320, 72)),
        ("compact_button_disabled_short_v5", (160, 72)),
        ("compact_button_disabled_medium_v5", (220, 72)),
        ("compact_button_disabled_long_v5", (320, 72)),
        ("compact_journal_panel_v5", (220, 280)),
        ("compact_journal_panel_v5", (300, 380)),
    ]
    sheet = checkerboard((700, 620), 16)
    draw = ImageDraw.Draw(sheet)
    y = 20
    x = 24
    for asset_id, size in preview_specs:
        crop = next(item for item in CROPS if item["id"] == asset_id)
        image = nine_slice_resize(outputs_by_id[asset_id], crop["slice9"], size)
        if x + size[0] > sheet.width - 24:
            x = 24
            y += 100
        sheet.alpha_composite(image, (x, y))
        draw.text((x, y + size[1] + 5), f"{asset_id} {size[0]}x{size[1]}", fill=(238, 232, 214, 255), font=font(12))
        x += size[0] + 28
        if size[1] > 120:
            y += size[1] + 34
            x = 24
    return sheet


def write_manifests() -> None:
    crop_manifest = {
        "schema": "game.art_crop_manifest",
        "version": 1,
        "art_job": ART_JOB,
        "output_dir": rel(OUT_DIR),
        "green_screen": {
            "mode": "chroma_key",
            "key": "#00ff00",
            "notes": "Builder removes only border-connected chroma pixels from a real generated compact UI source sheet.",
        },
        "sources": [
            {
                "id": "rune_marches_compact_ui_bases_source_v5",
                "path": rel(SOURCE),
                "generator": "built-in image generation",
                "source_role": "compact/mobile generated UI bases: buttons and portrait panel only",
                "crops": CROPS,
            }
        ],
        "preview_evidence": [
            "gamedesign/projects/rune-marches/art/previews/rune-marches-ui-compact-bases-v5-contact-sheet.png",
            "gamedesign/projects/rune-marches/art/previews/rune-marches-ui-compact-bases-v5-slice9-preview.png",
            "gamedesign/projects/rune-marches/art/previews/rune-marches-ui-compact-bases-v5-composition-proof.png",
            rel(EDGE_PROOF_PNG),
        ],
    }
    write_json_atomic(CROP_MANIFEST, crop_manifest)
    runtime_manifest = {
        "schema": "game.asset_manifest",
        "version": 1,
        "art_job": ART_JOB,
        "crop_manifest": rel(CROP_MANIFEST),
        "runtime_dir": rel(OUT_DIR),
        "source_art": rel(SOURCE),
        "source_policy": "real generated bitmap source; no procedural replacement",
        "commands": {
            "slice_assets": "py -3.12 tools/assets/build_rune_marches_ui_compact_bases_v5.py",
            "asset_audit": "py -3.12 tools/assets/audit_generated_ui_assets.py --crop-manifest gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-crop_manifest.json --json-output gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-asset-audit.json --report gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-asset-audit.md",
            "source_derivation_audit": "py -3.12 tools/assets/audit_generated_source_derivation.py --crop-manifest gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-crop_manifest.json --json-output gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-source-derivation-audit.json --report gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-source-derivation-audit.md",
            "slice9_design_audit": "node tools/assets/audit_slice9_design_policy.mjs --crop-manifest gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-crop_manifest.json --runtime-manifest gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-asset_manifest.json --json-output gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-slice9-design-audit.json --report gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-slice9-design-audit.md --profile",
            "composition_proof": "py -3.12 tools/assets/render_ui_composition_proof.py --asset-manifest gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-asset_manifest.json --output gamedesign/projects/rune-marches/art/previews/rune-marches-ui-compact-bases-v5-composition-proof.png --json-output gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-composition-proof.json --report gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-composition-proof.md --profile",
            "edge_proof": "py -3.12 tools/assets/render_ui_asset_edge_proof.py --crop-manifest gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-crop_manifest.json --output gamedesign/projects/rune-marches/art/previews/rune-marches-ui-compact-bases-v5-edge-proof.png --json-output gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-edge-proof.json --report gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-edge-proof.md --profile",
            "atlas_metadata_audit": "node tools/assets/audit_atlas_metadata.mjs --asset-manifest gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-asset_manifest.json --json-output gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-atlas-metadata-audit.json --report gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-atlas-metadata-audit.md",
            "build_atlas_pack": "py -3.12 tools/assets/build_ui_atlas_pack.py --asset-manifest gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-asset_manifest.json --output-dir assets/runtime/rune-marches-ui-compact-bases-v5-atlas --json-output gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-atlas_pack.json --report gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-atlas-pack.md --label-review --profile",
            "atlas_pack_audit": "py -3.12 tools/assets/audit_ui_atlas_pack.py --atlas-pack gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-atlas_pack.json --asset-manifest gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-asset_manifest.json --json-output gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-atlas-pack-audit.json --report gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-atlas-pack-audit.md --profile",
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
                **atlas_fields(crop),
            }
            for crop in CROPS
        ],
        "preview_evidence": [
            "gamedesign/projects/rune-marches/art/previews/rune-marches-ui-compact-bases-v5-contact-sheet.png",
            "gamedesign/projects/rune-marches/art/previews/rune-marches-ui-compact-bases-v5-slice9-preview.png",
            "gamedesign/projects/rune-marches/art/previews/rune-marches-ui-compact-bases-v5-composition-proof.png",
            rel(EDGE_PROOF_PNG),
        ],
        "asset_audit": [
            "gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-asset-audit.md",
            "gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-asset-audit.json",
        ],
        "source_derivation_audit": [
            "gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-source-derivation-audit.md",
            "gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-source-derivation-audit.json",
        ],
        "composition_proof": [
            "gamedesign/projects/rune-marches/art/previews/rune-marches-ui-compact-bases-v5-composition-proof.png",
            "gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-composition-proof.md",
            "gamedesign/projects/rune-marches/reviews/rune-marches-ui-compact-bases-v5-composition-proof.json",
        ],
        "edge_proofs": [
            rel(EDGE_PROOF_PNG),
        ],
        "edge_proof_reports": [
            rel(EDGE_PROOF_MD),
            rel(EDGE_PROOF_JSON),
        ],
        "atlas_metadata_audit": [
            rel(ATLAS_AUDIT_MD),
            rel(ATLAS_AUDIT_JSON),
        ],
        "atlas_pack": [
            rel(ATLAS_PACK_JSON),
            rel(ATLAS_PACK_MD),
            f"{rel(ATLAS_PACK_DIR)}/ui_rune_marches_compact_bases_v5.png",
            f"{rel(ATLAS_PACK_DIR)}/ui_rune_marches_compact_bases_v5-labeled.png",
        ],
        "atlas_pack_audit": [
            rel(ATLAS_PACK_AUDIT_MD),
            rel(ATLAS_PACK_AUDIT_JSON),
        ],
    }
    write_json_atomic(RUNTIME_MANIFEST, runtime_manifest)


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
        save_image_atomic(image, output_path)
        outputs.append((crop, image))
        outputs_by_id[crop["id"]] = image
    save_image_atomic(
        make_contact_sheet(outputs),
        PREVIEW_DIR / "rune-marches-ui-compact-bases-v5-contact-sheet.png",
    )
    save_image_atomic(
        make_slice_preview(outputs_by_id),
        PREVIEW_DIR / "rune-marches-ui-compact-bases-v5-slice9-preview.png",
    )
    write_manifests()
    print(f"wrote {len(outputs)} generated-source compact UI assets to {rel(OUT_DIR)}")
    print(f"wrote crop manifest: {rel(CROP_MANIFEST)}")
    print(f"wrote runtime manifest: {rel(RUNTIME_MANIFEST)}")
    print(f"wrote previews to {rel(PREVIEW_DIR)}")


if __name__ == "__main__":
    main()
