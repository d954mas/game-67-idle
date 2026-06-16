#!/usr/bin/env python3
"""Import selected generated Critter Corral source art into runtime sprite slots.

This is the corrective T0070 path: the major visible sprites are cut from
project-saved image generation outputs, then packed by the existing native asset
pipeline. Supporting FX/icons can still come from generate_sprites.py until
their own generated source families exist.
"""
from __future__ import annotations

import json
from pathlib import Path
import sys
from typing import Any

from PIL import Image, ImageEnhance, ImageOps
import numpy as np


ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.assets.chroma_key_alpha import (  # noqa: E402
    bleed_transparent_rgb,
    remove_edge_fringe,
    remove_green_screen_spill,
    remove_source_key_spill,
    repair_visible_halo,
    repair_transparent_edge_rgb,
    zero_fully_transparent_rgb,
)
from tools.assets.atomic_io import save_image_atomic, write_json_atomic  # noqa: E402


PROJECT = ROOT / "gamedesign" / "projects" / "critter-corral"
SOURCE_DIR = PROJECT / "art" / "generated" / "T0070"
SPRITE_DIR = PROJECT / "art" / "sprites"
DATA_DIR = PROJECT / "data"
REVIEW_DIR = PROJECT / "reviews"
ART_JOB = "gamedesign/projects/critter-corral/art_requests/t0070_generated_casual_core.json"
CROP_MANIFEST = DATA_DIR / "t0070_generated_casual_core-crop_manifest.json"
ASSET_MANIFEST = DATA_DIR / "t0070_generated_casual_core-asset_manifest.json"
CONTACT_SHEET = REVIEW_DIR / "t0070_generated_casual_core-runtime_contact_sheet.png"
KEY = (255, 0, 255)


ASSETS: list[dict[str, Any]] = [
    {
        "id": "generated_upgrade_card",
        "source": "generated-card-horizontal-source-v2.png",
        "output": "card.png",
        "kind": "slice9",
        "size": [256, 128],
        "padding": 6,
        "semantic_role": "upgrade_card_background",
        "slice9": [52, 42, 52, 36],
        "content": [56, 36, 144, 56],
        "pack_group": "ui_common",
    },
    {
        "id": "generated_critter_neutral",
        "source": "generated-critter-source-v1.png",
        "output": "critter.png",
        "kind": "sprite",
        "size": [112, 112],
        "padding": 7,
        "semantic_role": "neutral_tintable_critter",
        "preserve_green_edges": True,
        "pack_group": "gameplay_common",
    },
    {
        "id": "generated_critter_a",
        "source": "generated-critter-source-v1.png",
        "output": "critter_a.png",
        "kind": "sprite",
        "size": [112, 112],
        "padding": 7,
        "tint": [255, 116, 78],
        "semantic_role": "warm_critter_variant",
        "pack_group": "gameplay_common",
    },
    {
        "id": "generated_critter_b",
        "source": "generated-critter-source-v1.png",
        "output": "critter_b.png",
        "kind": "sprite",
        "size": [112, 112],
        "padding": 7,
        "tint": [84, 166, 255],
        "semantic_role": "cool_critter_variant",
        "pack_group": "gameplay_common",
    },
    {
        "id": "generated_pen",
        "source": "generated-pen-source-v1.png",
        "output": "pen.png",
        "kind": "sprite",
        "size": [256, 200],
        "padding": 8,
        "semantic_role": "toy_fence_pen",
        "preserve_green_edges": True,
        "pack_group": "gameplay_common",
    },
    {
        "id": "generated_icon_radius",
        "source": "generated-upgrade-icons-source-v1.png",
        "slot": [0, 6],
        "output": "icon_radius.png",
        "kind": "icon",
        "size": [96, 96],
        "padding": 4,
        "semantic_role": "upgrade_lure_radius_icon",
        "preserve_green_edges": True,
        "pack_group": "ui_common",
    },
    {
        "id": "generated_icon_pull",
        "source": "generated-upgrade-icons-source-v1.png",
        "slot": [1, 6],
        "output": "icon_pull.png",
        "kind": "icon",
        "size": [96, 96],
        "padding": 4,
        "semantic_role": "upgrade_lure_pull_icon",
        "preserve_green_edges": True,
        "pack_group": "ui_common",
    },
    {
        "id": "generated_icon_second_lure",
        "source": "generated-upgrade-icons-source-v1.png",
        "slot": [2, 6],
        "output": "icon_second_lure.png",
        "kind": "icon",
        "size": [96, 96],
        "padding": 4,
        "semantic_role": "upgrade_second_lure_icon",
        "preserve_green_edges": True,
        "pack_group": "ui_common",
    },
    {
        "id": "generated_icon_gate",
        "source": "generated-upgrade-icons-source-v1.png",
        "slot": [3, 6],
        "output": "icon_gate.png",
        "kind": "icon",
        "size": [96, 96],
        "padding": 4,
        "semantic_role": "upgrade_wider_gates_icon",
        "preserve_green_edges": True,
        "pack_group": "ui_common",
    },
    {
        "id": "generated_icon_calm",
        "source": "generated-upgrade-icons-source-v1.png",
        "slot": [4, 6],
        "output": "icon_calm.png",
        "kind": "icon",
        "size": [96, 96],
        "padding": 4,
        "semantic_role": "upgrade_calm_icon",
        "preserve_green_edges": True,
        "pack_group": "ui_common",
    },
    {
        "id": "generated_icon_chain",
        "source": "generated-upgrade-icons-source-v1.png",
        "slot": [5, 6],
        "output": "icon_chain.png",
        "kind": "icon",
        "size": [96, 96],
        "padding": 4,
        "semantic_role": "upgrade_longer_chain_icon",
        "preserve_green_edges": True,
        "pack_group": "ui_common",
    },
]


def rel(path: Path) -> str:
    return path.resolve().relative_to(ROOT.resolve()).as_posix()


def alpha_bbox(image: Image.Image, threshold: int = 10) -> tuple[int, int, int, int]:
    bbox = image.getchannel("A").point(lambda value: 255 if value > threshold else 0).getbbox()
    if bbox is None:
        raise RuntimeError("generated source produced empty alpha")
    return bbox


def fast_key_to_alpha(image: Image.Image) -> Image.Image:
    """Fast chroma removal for the T0070 generated sources.

    The generated files use flat #ff00ff backgrounds. We clear border-style
    magenta with tolerance and leave subject pixels untouched until the small
    runtime canvas cleanup pass.
    """
    rgba = image.convert("RGBA")
    arr = np.array(rgba, dtype=np.int16)
    red = arr[..., 0]
    green = arr[..., 1]
    blue = arr[..., 2]
    exact = (np.abs(red - KEY[0]) <= 34) & (np.abs(green - KEY[1]) <= 34) & (np.abs(blue - KEY[2]) <= 34)
    fringe = (red > 170) & (blue > 170) & (green < 90) & ((red + blue) > green * 4 + 260)
    arr[..., 3] = np.where(exact | fringe, 0, arr[..., 3])
    arr[..., :3] = np.where((arr[..., 3:4] == 0), 0, arr[..., :3])
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGBA")


def source_region(source: Image.Image, spec: dict[str, Any]) -> Image.Image:
    slot = spec.get("slot")
    if isinstance(slot, list) and len(slot) == 2:
        index = int(slot[0])
        count = int(slot[1])
        if count <= 0 or index < 0 or index >= count:
            raise RuntimeError(f"invalid slot for {spec['id']}: {slot}")
        cell_w = source.width / float(count)
        x0 = round(cell_w * float(index))
        x1 = round(cell_w * float(index + 1))
        return source.crop((x0, 0, x1, source.height))
    return source


def fit_to_canvas(image: Image.Image, size: tuple[int, int], padding: int) -> Image.Image:
    bbox = alpha_bbox(image)
    crop = image.crop(bbox)
    max_w = max(1, size[0] - padding * 2)
    max_h = max(1, size[1] - padding * 2)
    scale = min(max_w / crop.width, max_h / crop.height)
    resized = crop.resize(
        (max(1, round(crop.width * scale)), max(1, round(crop.height * scale))),
        Image.Resampling.LANCZOS,
    )
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.alpha_composite(resized, ((size[0] - resized.width) // 2, (size[1] - resized.height) // 2))
    remove_edge_fringe(canvas)
    remove_source_key_spill(canvas, KEY)
    remove_green_screen_spill(canvas)
    repair_visible_halo(canvas)
    bleed_transparent_rgb(canvas, key=KEY)
    repair_transparent_edge_rgb(canvas, key=KEY)
    zero_fully_transparent_rgb(canvas)
    return canvas


def apply_tint(image: Image.Image, tint: list[int] | None) -> Image.Image:
    if not tint:
        return image
    tint_rgb = Image.new("RGBA", image.size, tuple(tint) + (255,))
    gray = ImageOps.grayscale(image)
    tinted = ImageOps.colorize(gray, black=(38, 30, 36), white=tuple(tint)).convert("RGBA")
    alpha = image.getchannel("A")
    # Restore eye/mouth contrast by blending only the body-colored source while
    # keeping dark/generated facial details visible.
    luminance = ImageEnhance.Contrast(gray).enhance(1.25)
    mixed = Image.blend(tinted, tint_rgb, 0.08)
    mixed.putalpha(alpha)
    composited = Image.composite(image, mixed, luminance.point(lambda value: 170 if value > 92 else 0))
    remove_edge_fringe(composited)
    remove_source_key_spill(composited, KEY)
    remove_green_screen_spill(composited)
    repair_visible_halo(composited)
    bleed_transparent_rgb(composited, key=KEY)
    repair_transparent_edge_rgb(composited, key=KEY)
    zero_fully_transparent_rgb(composited)
    return composited


def write_contact_sheet(items: list[tuple[str, Image.Image]]) -> None:
    cell_w, cell_h = 220, 190
    sheet = Image.new("RGBA", (cell_w * len(items), cell_h), (36, 35, 42, 255))
    for index, (label, image) in enumerate(items):
        bg = Image.new("RGBA", (cell_w, cell_h), (36, 35, 42, 255))
        for y in range(0, cell_h, 16):
            for x in range(0, cell_w, 16):
                if (x // 16 + y // 16) % 2 == 0:
                    bg.paste((52, 49, 57, 255), (x, y, x + 16, y + 16))
        max_w, max_h = cell_w - 36, cell_h - 48
        scale = min(max_w / image.width, max_h / image.height, 1.0)
        preview = image.resize(
            (max(1, round(image.width * scale)), max(1, round(image.height * scale))),
            Image.Resampling.LANCZOS,
        )
        bg.alpha_composite(preview, ((cell_w - preview.width) // 2, 12))
        sheet.alpha_composite(bg, (index * cell_w, 0))
    save_image_atomic(sheet, CONTACT_SHEET)


def main() -> None:
    SPRITE_DIR.mkdir(parents=True, exist_ok=True)
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    REVIEW_DIR.mkdir(parents=True, exist_ok=True)

    crop_sources: dict[str, dict[str, Any]] = {}
    runtime_assets: list[dict[str, Any]] = []
    contact_items: list[tuple[str, Image.Image]] = []
    keyed_source_cache: dict[str, Image.Image] = {}

    for spec in ASSETS:
        source_path = SOURCE_DIR / spec["source"]
        if not source_path.exists():
            raise SystemExit(f"missing generated source: {source_path}")
        source_key = spec["source"]
        keyed_source = keyed_source_cache.get(source_key)
        if keyed_source is None:
            source = Image.open(source_path).convert("RGBA")
            keyed_source = fast_key_to_alpha(source)
            keyed_source_cache[source_key] = keyed_source
        region = source_region(keyed_source, spec)
        trimmed = fit_to_canvas(region, tuple(spec["size"]), int(spec.get("padding", 0)))
        final = apply_tint(trimmed, spec.get("tint"))
        output_path = SPRITE_DIR / spec["output"]
        save_image_atomic(final, output_path)
        contact_items.append((spec["id"], final))

        bbox = alpha_bbox(final)
        crop_entry: dict[str, Any] = {
            "id": spec["id"],
            "kind": spec["kind"],
            "rect": [0, 0, region.width, region.height],
            "output": rel(output_path),
            "source_rect": [0, 0, region.width, region.height],
            "trim_rect": [bbox[0], bbox[1], bbox[2] - bbox[0], bbox[3] - bbox[1]],
            "trim_padding": int(spec.get("padding", 0)),
            "chroma_key": {"key": "#ff00ff", "mode": "border_connected"},
            "semantic_role": spec["semantic_role"],
            "atlas": {
                "pack_group": spec["pack_group"],
                "trim_mode": "alpha",
                "alpha_bleed": True,
                "premultiply_alpha": True,
                "extrude": 2,
                "shape_padding": 2,
                "border_padding": 1,
                "allow_rotation": False,
            },
        }
        if "slice9" in spec:
            crop_entry["slice9"] = spec["slice9"]
            crop_entry["content"] = spec["content"]
            crop_entry["target_preview_sizes"] = [[160, 200], [192, 240], [224, 280]]
        if spec.get("preserve_green_edges"):
            crop_entry["preserve_green_edges"] = True

        source_record = crop_sources.setdefault(
            source_key,
            {
                "id": source_key.replace(".png", ""),
                "path": rel(source_path),
            "source_role": "generated source cut for T0070 casual rescue pass",
                "crops": [],
            },
        )
        source_record["crops"].append(crop_entry)

        runtime_entry = {
            "id": spec["id"],
            "kind": spec["kind"],
            "path": rel(output_path),
            "pack_group": spec["pack_group"],
            "source_crop": spec["id"],
            "original_size": spec["size"],
            "trim_rect": crop_entry["trim_rect"],
            "semantic_role": spec["semantic_role"],
            "anchor": "center",
            "atlas_policy": crop_entry["atlas"],
        }
        if "slice9" in spec:
            runtime_entry["slice9"] = spec["slice9"]
            runtime_entry["content"] = spec["content"]
        runtime_assets.append(runtime_entry)

    crop_manifest = {
        "schema": "game.art_crop_manifest",
        "version": 1,
        "art_job": ART_JOB,
        "output_dir": rel(SPRITE_DIR),
        "green_screen": {
            "mode": "chroma_key",
            "key": "#ff00ff",
            "notes": "T0070 generated source art cut with border-connected chroma-key removal.",
        },
        "sources": list(crop_sources.values()),
    }
    runtime_manifest = {
        "schema": "game.asset_manifest",
        "version": 1,
        "art_job": ART_JOB,
        "crop_manifest": rel(CROP_MANIFEST),
        "runtime_dir": rel(SPRITE_DIR),
        "source_policy": "real generated bitmap sources, locally chroma-keyed/cropped into native runtime sprite slots",
        "assets": runtime_assets,
    }

    write_json_atomic(CROP_MANIFEST, crop_manifest)
    write_json_atomic(ASSET_MANIFEST, runtime_manifest)
    write_contact_sheet(contact_items)
    for asset in runtime_assets:
        print(f"wrote {asset['path']} from generated source")
    print(f"wrote {rel(CROP_MANIFEST)}")
    print(f"wrote {rel(ASSET_MANIFEST)}")
    print(f"wrote {rel(CONTACT_SHEET)}")


if __name__ == "__main__":
    main()
