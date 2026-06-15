#!/usr/bin/env python3
from __future__ import annotations

import argparse
import colorsys
import json
from pathlib import Path

from PIL import Image, ImageEnhance

from atomic_io import save_image_atomic, write_json_atomic
from chroma_key_alpha import (
    is_any_purple_halo_like,
    key_to_alpha,
    remove_edge_fringe,
    remove_green_screen_spill,
    remove_source_key_spill,
    repair_visible_halo,
    resize_rgba_premultiplied,
    zero_fully_transparent_rgb,
)


FISHING_UI_ASSETS = [
    {
        "id": "fishing_primary_button_slice9",
        "crop": (67, 61, 606, 279),
        "size": (320, 128),
        "kind": "slice9",
        "slice9": [76, 40, 76, 40],
        "semantic_role": "fishing_primary_action_button",
        "preserve_green_edges": True,
    },
    {
        "id": "fishing_secondary_button_slice9",
        "crop": (664, 63, 1187, 280),
        "size": (300, 112),
        "kind": "slice9",
        "slice9": [72, 34, 72, 34],
        "semantic_role": "fishing_secondary_action_button",
    },
    {
        "id": "fishing_upgrade_button_slice9",
        "crop": (69, 316, 591, 506),
        "size": (300, 112),
        "kind": "slice9",
        "slice9": [72, 34, 72, 34],
        "semantic_role": "fishing_upgrade_action_button",
        "source_note": "partial runtime slice; purple/magenta chroma edge halo is not accepted",
        "remap_purple_material": True,
    },
    {
        "id": "fishing_status_pill_slice9",
        "crop": (647, 315, 1198, 508),
        "size": (360, 104),
        "kind": "slice9",
        "slice9": [82, 32, 82, 32],
        "semantic_role": "fishing_status_hud_pill",
    },
    {
        "id": "fishing_catch_card_slice9",
        "crop": (61, 536, 617, 750),
        "size": (340, 132),
        "kind": "slice9",
        "slice9": [58, 44, 58, 44],
        "semantic_role": "fishing_catch_reward_card",
    },
    {
        "id": "fishing_meter_frame_slice9",
        "crop": (682, 574, 1171, 719),
        "size": (320, 72),
        "kind": "slice9",
        "slice9": [56, 24, 56, 24],
        "semantic_role": "fishing_reel_meter_frame",
    },
    {
        "id": "fishing_coin_icon",
        "crop": (50, 783, 235, 969),
        "size": (96, 96),
        "kind": "icon",
        "semantic_role": "fishing_coin_currency",
    },
    {
        "id": "fishing_backpack_icon",
        "crop": (280, 780, 475, 978),
        "size": (96, 96),
        "kind": "icon",
        "semantic_role": "fishing_backpack",
    },
    {
        "id": "fishing_fish_icon",
        "crop": (502, 773, 727, 974),
        "size": (112, 96),
        "kind": "icon",
        "semantic_role": "fishing_index_fish",
    },
    {
        "id": "fishing_rod_icon",
        "crop": (748, 761, 956, 981),
        "size": (112, 112),
        "kind": "icon",
        "semantic_role": "fishing_rod_action",
    },
]

HEADER = """#ifndef ROBLOX_FISHING_UI_ASSETS_GEN_H
#define ROBLOX_FISHING_UI_ASSETS_GEN_H

#include <stdint.h>

typedef enum FishingAssetId {
"""


def enum_name(asset_id: str) -> str:
    return "FISHING_ASSET_" + asset_id.removeprefix("fishing_").upper()


def sanitize(asset_id: str) -> str:
    return "fishing_asset_" + asset_id.removeprefix("fishing_")


def write_c_array(handle, name: str, data: bytes) -> None:
    handle.write(f"static const uint8_t {name}[] = {{\n")
    for index in range(0, len(data), 16):
        chunk = data[index : index + 16]
        handle.write("    ")
        handle.write(", ".join(str(byte) for byte in chunk))
        handle.write(",\n")
    handle.write("};\n\n")


def write_generated_sources(generated_dir: Path, built: list[dict]) -> tuple[Path, Path]:
    generated_dir.mkdir(parents=True, exist_ok=True)
    header_path = generated_dir / "roblox_fishing_ui_assets.gen.h"
    source_path = generated_dir / "roblox_fishing_ui_assets.gen.c"

    with header_path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(HEADER)
        for spec in built:
            handle.write(f"    {enum_name(spec['id'])},\n")
        handle.write("""    FISHING_ASSET_COUNT
} FishingAssetId;

typedef struct FishingAssetImage {
    const char *id;
    uint16_t width;
    uint16_t height;
    const uint8_t *rgba;
    uint32_t rgba_size;
} FishingAssetImage;

extern const FishingAssetImage g_fishing_assets[FISHING_ASSET_COUNT];

#endif
""")

    with source_path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write('#include "generated/roblox_fishing_ui_assets.gen.h"\n\n')
        for spec in built:
            write_c_array(handle, sanitize(spec["id"]), spec["image"].tobytes())
        handle.write("const FishingAssetImage g_fishing_assets[FISHING_ASSET_COUNT] = {\n")
        for spec in built:
            handle.write(
                f'    [{enum_name(spec["id"])}] = {{"{spec["id"]}", {spec["size"][0]}, {spec["size"][1]}, '
                f'{sanitize(spec["id"])}, sizeof({sanitize(spec["id"])})}},\n'
            )
        handle.write("};\n")
    return header_path, source_path


def repo_path(path: Path) -> str:
    try:
        return path.resolve().relative_to(Path.cwd().resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def remap_purple_material_to_ocean_blue(image: Image.Image) -> None:
    pixels = image.load()
    width, height = image.size
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if alpha <= 12:
                continue
            hue, lightness, saturation = colorsys.rgb_to_hls(red / 255.0, green / 255.0, blue / 255.0)
            hue_degrees = hue * 360.0
            purple_material = (
                250 <= hue_degrees <= 325
                and saturation >= 0.18
                and blue >= 44
                and red >= 32
                and blue + red > green * 1.55 + 24
            )
            if not purple_material and not is_any_purple_halo_like(red, green, blue):
                continue
            ocean_red, ocean_green, ocean_blue = colorsys.hls_to_rgb(
                198.0 / 360.0,
                lightness,
                min(1.0, saturation * 1.08),
            )
            pixels[x, y] = (
                max(0, min(255, int(ocean_red * 255))),
                max(0, min(255, int(ocean_green * 255))),
                max(0, min(255, int(ocean_blue * 255))),
                alpha,
            )


def process_fishing_ui_asset(source: Image.Image, spec: dict) -> Image.Image:
    image = source.crop(spec["crop"]).convert("RGBA")
    image = key_to_alpha(
        image,
        key=(255, 0, 255),
        exact_tolerance=10,
        edge_tolerance=42,
        aggressive_visible_decontaminate=True,
    )
    pad = 8
    inner_size = (max(1, spec["size"][0] - pad * 2), max(1, spec["size"][1] - pad * 2))
    image = resize_rgba_premultiplied(image, inner_size)
    padded = Image.new("RGBA", spec["size"], (0, 0, 0, 0))
    padded.alpha_composite(image, (pad, pad))
    image = padded
    remove_edge_fringe(image, passes=4)
    remove_source_key_spill(image, (255, 0, 255), passes=4, radius=3)
    repair_visible_halo(image, radius=8, require_transparent_touch=True)
    zero_fully_transparent_rgb(image)
    image = ImageEnhance.Color(image).enhance(1.06)
    image = ImageEnhance.Contrast(image).enhance(1.04)
    remove_edge_fringe(image, passes=4)
    remove_source_key_spill(image, (255, 0, 255), passes=4, radius=3)
    remove_green_screen_spill(image, passes=3, radius=3)
    repair_visible_halo(image, radius=10, require_transparent_touch=True)
    if spec.get("remap_purple_material"):
        remap_purple_material_to_ocean_blue(image)
        remove_edge_fringe(image, passes=4)
        remove_source_key_spill(image, (255, 0, 255), passes=4, radius=3)
        repair_visible_halo(image, radius=10, require_transparent_touch=True)
    zero_fully_transparent_rgb(image)
    return image


def crop_manifest_entry(spec: dict) -> dict:
    crop = {
        "id": spec["id"],
        "kind": spec["kind"],
        "rect": list(spec["crop"]),
        "output": spec["path"],
        "output_size": list(spec["size"]),
        "semantic_role": spec.get("semantic_role"),
        "source_family": spec["source_family"],
    }
    if spec.get("preserve_green_edges"):
        crop["preserve_green_edges"] = True
    if spec.get("slice9"):
        crop["slice9"] = {
            "left": spec["slice9"][0],
            "top": spec["slice9"][1],
            "right": spec["slice9"][2],
            "bottom": spec["slice9"][3],
        }
        crop["content"] = {
            "x": spec["slice9"][0],
            "y": spec["slice9"][1],
            "w": max(1, spec["size"][0] - spec["slice9"][0] - spec["slice9"][2]),
            "h": max(1, spec["size"][1] - spec["slice9"][1] - spec["slice9"][3]),
        }
        crop["target_preview_sizes"] = [list(spec["size"]), [max(spec["size"][0], 220), max(spec["size"][1], 64)]]
    if spec["kind"] == "icon":
        crop["trim_padding"] = 8
        crop["isolate_component"] = "source crop contains one accepted icon component"
        crop["preview_sizes"] = [[32, 32], [48, 48]]
    return crop


def asset_manifest_entry(spec: dict) -> dict:
    crop = crop_manifest_entry(spec)
    return {
        "id": spec["id"],
        "kind": spec["kind"],
        "path": spec["path"],
        "dimensions": list(spec["size"]),
        "png_size_bytes": spec["png_size_bytes"],
        "slice9": crop.get("slice9"),
        "semantic_role": spec.get("semantic_role"),
        "pack_group": "roblox_fishing_ui_common",
        "source_crop": spec["id"],
        "runtime_usage": "native roblox_fishing_ui_assets generated C texture array",
    }


def build_roblox_fishing_ui_assets(
    source_path: Path,
    runtime_dir: Path,
    crop_manifest_path: Path,
    asset_manifest_path: Path,
    generated_dir: Path | None = None,
) -> list[dict]:
    if not source_path.exists():
        raise FileNotFoundError(f"missing fishing UI source sheet: {source_path}")
    source = Image.open(source_path).convert("RGB")
    runtime_dir.mkdir(parents=True, exist_ok=True)
    built = []
    for spec in FISHING_UI_ASSETS:
        image = process_fishing_ui_asset(source, spec)
        png_path = runtime_dir / f"{spec['id']}.png"
        save_image_atomic(image, png_path, optimize=True)
        built.append({
            **spec,
            "path": repo_path(png_path),
            "png_size_bytes": png_path.stat().st_size,
            "image": image,
            "pivot": [0.5, 0.5],
            "source_family": "roblox-fishing-ui-icons-source-v2-magenta-clean",
        })

    crops = [crop_manifest_entry(spec) for spec in built]
    assets = [asset_manifest_entry(spec) for spec in built]
    crop_manifest = {
        "schema": "game.art_crop_manifest",
        "version": 2,
        "art_job": "gamedesign/projects/roblox-fishing/art_requests/roblox-fishing-first-visual-v1.json",
        "output_dir": repo_path(runtime_dir),
        "green_screen": {
            "mode": "chroma_key",
            "key": "#ff00ff",
            "notes": "Fishing UI source is generated on magenta chroma; runtime assets use key_to_alpha edge cleanup before packing into C arrays.",
        },
        "sources": [
            {
                "id": "splash-rods-ui-icons-source-v2-magenta-clean",
                "path": repo_path(source_path),
                "source_role": "partial generated runtime UI sheet",
                "crops": crops,
            }
        ],
    }
    write_json_atomic(crop_manifest_path, crop_manifest)
    asset_manifest = {
        "schema": "game.asset_manifest",
        "version": 2,
        "art_job": "gamedesign/projects/roblox-fishing/art_requests/roblox-fishing-first-visual-v1.json",
        "crop_manifest": repo_path(crop_manifest_path),
        "runtime_dir": repo_path(runtime_dir),
        "generated_code": [
            repo_path(generated_dir / "roblox_fishing_ui_assets.gen.h"),
            repo_path(generated_dir / "roblox_fishing_ui_assets.gen.c"),
        ] if generated_dir is not None else [],
        "source_art": repo_path(source_path),
        "source_policy": "partial generated bitmap UI source; not final source-family-complete art",
        "assets": assets,
    }
    write_json_atomic(asset_manifest_path, asset_manifest)
    if generated_dir is not None:
        write_generated_sources(generated_dir, built)
    return built


def load_roblox_fishing_ui_assets(runtime_dir: Path) -> list[dict]:
    built = []
    for spec in FISHING_UI_ASSETS:
        path = runtime_dir / f"{spec['id']}.png"
        if not path.exists():
            raise FileNotFoundError(f"missing Roblox Fishing UI runtime asset: {path}")
        image = Image.open(path).convert("RGBA")
        built.append({
            **spec,
            "path": repo_path(path),
            "png_size_bytes": path.stat().st_size,
            "size": image.size,
            "pivot": [0.5, 0.5],
            "image": image,
            "project": "roblox-fishing",
        })
    return built


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", default="gamedesign/projects/roblox-fishing/art/source_sheets/splash-rods-ui-icons-source-v2-magenta-clean.png")
    parser.add_argument("--runtime-dir", default="assets/runtime/roblox-fishing-ui-v1")
    parser.add_argument("--crop-manifest", default="gamedesign/projects/roblox-fishing/data/roblox-fishing-first-visual-v1-crop_manifest.json")
    parser.add_argument("--asset-manifest", default="gamedesign/projects/roblox-fishing/data/roblox-fishing-first-visual-v1-asset_manifest.json")
    parser.add_argument("--generated-dir", default=None)
    args = parser.parse_args()
    built = build_roblox_fishing_ui_assets(
        Path(args.source),
        Path(args.runtime_dir),
        Path(args.crop_manifest),
        Path(args.asset_manifest),
        Path(args.generated_dir) if args.generated_dir else None,
    )
    print(f"wrote {len(built)} Roblox Fishing UI assets to {args.runtime_dir}")
    print(f"wrote crop manifest: {args.crop_manifest}")
    print(f"wrote runtime manifest: {args.asset_manifest}")
    if args.generated_dir:
        print(f"wrote generated C assets to {args.generated_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
