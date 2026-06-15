from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageOps

from chroma_key_alpha import key_to_alpha, remove_edge_fringe, remove_source_key_spill, repair_visible_halo, resize_rgba_premultiplied, zero_fully_transparent_rgb


ASSETS = [
    {
        "id": "map_background",
        "crop": (92, 126, 1288, 810),
        "size": (512, 256),
        "kind": "background",
        "slice9": None,
        "pivot": [0.5, 0.5],
    },
    {
        "id": "landmark_miregate",
        "crop": (326, 248, 514, 456),
        "size": (96, 112),
        "kind": "icon",
        "mask": "diamond",
        "slice9": None,
        "pivot": [0.5, 0.86],
    },
    {
        "id": "landmark_wispfen_road",
        "crop": (654, 302, 840, 508),
        "size": (96, 112),
        "kind": "icon",
        "mask": "diamond",
        "slice9": None,
        "pivot": [0.5, 0.86],
    },
    {
        "id": "landmark_old_bell_tower_locked",
        "crop": (964, 256, 1150, 492),
        "size": (96, 128),
        "kind": "icon",
        "mask": "diamond",
        "slice9": None,
        "pivot": [0.5, 0.88],
    },
    {
        "id": "enemy_mire_wisp",
        "crop": (1374, 164, 1568, 342),
        "size": (144, 128),
        "kind": "sprite",
        "mask": "ellipse",
        "slice9": None,
        "pivot": [0.5, 0.55],
    },
    {
        "id": "spell_spark_effect",
        "crop": (1320, 426, 1430, 536),
        "size": (96, 96),
        "kind": "effect",
        "mask": "ellipse",
        "slice9": None,
        "pivot": [0.5, 0.5],
    },
    {
        "id": "ui_panel_slice9",
        "crop": (1290, 148, 1660, 548),
        "size": (192, 192),
        "kind": "slice9",
        "slice9": [24, 24, 24, 24],
        "pivot": [0.5, 0.5],
    },
    {
        "id": "button_idle_slice9",
        "crop": (714, 804, 850, 928),
        "size": (128, 64),
        "kind": "slice9",
        "slice9": [24, 18, 24, 18],
        "pivot": [0.5, 0.5],
    },
    {
        "id": "button_disabled_slice9",
        "crop": (876, 804, 1010, 928),
        "size": (128, 64),
        "kind": "slice9",
        "slice9": [24, 18, 24, 18],
        "pivot": [0.5, 0.5],
        "disabled": True,
    },
]

UI_KIT_V2_ASSETS = [
    {"id": "ui_v2_modal_panel_slice9", "file": "modal_panel_slice9.png", "kind": "slice9", "slice9": [92, 96, 92, 82]},
    {"id": "ui_v2_journal_panel_slice9", "file": "journal_panel_slice9.png", "kind": "slice9", "slice9": [54, 74, 54, 64]},
    {"id": "ui_v2_button_idle_slice9", "file": "button_idle_slice9.png", "kind": "slice9", "slice9": [56, 20, 56, 20]},
    {"id": "ui_v2_button_pressed_slice9", "file": "button_pressed_slice9.png", "kind": "slice9", "slice9": [56, 20, 56, 20]},
    {"id": "ui_v2_button_disabled_slice9", "file": "button_disabled_slice9.png", "kind": "slice9", "slice9": [52, 20, 52, 20]},
    {"id": "ui_v2_status_bar_slice9", "file": "status_bar_slice9.png", "kind": "slice9", "slice9": [72, 20, 72, 20]},
    {"id": "ui_v2_reward_chip_slice9", "file": "reward_chip_slice9.png", "kind": "slice9", "slice9": [44, 20, 44, 20]},
    {"id": "ui_v2_icon_frame_slice9", "file": "icon_frame_slice9.png", "kind": "slice9", "slice9": [24, 24, 24, 24]},
    {"id": "ui_v2_icon_health", "file": "icon_health.png", "kind": "icon", "semantic_role": "health"},
    {"id": "ui_v2_icon_mana", "file": "icon_mana.png", "kind": "icon", "semantic_role": "mana"},
    {"id": "ui_v2_icon_silver", "file": "icon_silver.png", "kind": "icon", "semantic_role": "silver"},
    {"id": "ui_v2_icon_xp", "file": "icon_xp.png", "kind": "icon", "semantic_role": "xp"},
    {"id": "ui_v2_icon_road_safety", "file": "icon_road_safety.png", "kind": "icon", "semantic_role": "road_safety"},
    {"id": "ui_v2_icon_rune_spark", "file": "icon_rune_spark.png", "kind": "icon", "semantic_role": "rune_spark"},
    {"id": "ui_v2_decor_modal_top_gem", "file": "decor_modal_top_gem.png", "kind": "decor", "semantic_role": "panel_top_decor"},
    {"id": "ui_v2_decor_modal_bottom_gem", "file": "decor_modal_bottom_gem.png", "kind": "decor", "semantic_role": "panel_bottom_decor"},
    {"id": "ui_v2_decor_journal_top_gem", "file": "decor_journal_top_gem.png", "kind": "decor", "semantic_role": "panel_top_decor"},
    {"id": "ui_v2_decor_journal_bottom_gem", "file": "decor_journal_bottom_gem.png", "kind": "decor", "semantic_role": "panel_bottom_decor"},
    {"id": "ui_v2_decor_reward_top_gem", "file": "decor_reward_top_gem.png", "kind": "decor", "semantic_role": "chip_top_decor"},
    {"id": "ui_v2_decor_reward_bottom_gem", "file": "decor_reward_bottom_gem.png", "kind": "decor", "semantic_role": "chip_bottom_decor"},
    {"id": "ui_v2_decor_icon_frame_top_gem", "file": "decor_icon_frame_top_gem.png", "kind": "decor", "semantic_role": "icon_frame_top_decor"},
    {"id": "ui_v2_decor_icon_frame_bottom_gem", "file": "decor_icon_frame_bottom_gem.png", "kind": "decor", "semantic_role": "icon_frame_bottom_decor"},
]

UI_BASES_V2_ASSETS = [
    {
        "id": "ui_bases_v2_modal_panel_slice9",
        "file": "modal_panel_v2_slice9.png",
        "kind": "slice9",
        "slice9": [104, 100, 104, 100],
        "semantic_role": "large_modal_panel",
    },
    {
        "id": "ui_bases_v2_journal_panel_slice9",
        "file": "journal_panel_v2_slice9.png",
        "kind": "slice9",
        "slice9": [84, 104, 84, 104],
        "semantic_role": "large_journal_panel",
    },
    {
        "id": "ui_bases_v2_button_idle_slice9",
        "file": "button_idle_v2_slice9.png",
        "kind": "slice9",
        "slice9": [96, 44, 96, 44],
        "semantic_role": "large_primary_button",
    },
    {
        "id": "ui_bases_v2_button_disabled_slice9",
        "file": "button_disabled_v2_slice9.png",
        "kind": "slice9",
        "slice9": [96, 44, 96, 44],
        "semantic_role": "large_primary_button_disabled",
    },
]


UI_COMPACT_BASES_V5_ASSETS = [
    {
        "id": "ui_compact_bases_v5_button_idle_short_slice9",
        "file": "compact_button_idle_short_v5_slice9.png",
        "kind": "slice9",
        "slice9": [48, 22, 48, 22],
        "semantic_role": "compact_button_idle_short",
    },
    {
        "id": "ui_compact_bases_v5_button_idle_medium_slice9",
        "file": "compact_button_idle_medium_v5_slice9.png",
        "kind": "slice9",
        "slice9": [48, 22, 48, 22],
        "semantic_role": "compact_button_idle_medium",
    },
    {
        "id": "ui_compact_bases_v5_button_idle_long_slice9",
        "file": "compact_button_idle_long_v5_slice9.png",
        "kind": "slice9",
        "slice9": [48, 22, 48, 22],
        "semantic_role": "compact_button_idle_long",
    },
    {
        "id": "ui_compact_bases_v5_button_disabled_short_slice9",
        "file": "compact_button_disabled_short_v5_slice9.png",
        "kind": "slice9",
        "slice9": [48, 22, 48, 22],
        "semantic_role": "compact_button_disabled_short",
    },
    {
        "id": "ui_compact_bases_v5_button_disabled_medium_slice9",
        "file": "compact_button_disabled_medium_v5_slice9.png",
        "kind": "slice9",
        "slice9": [48, 22, 48, 22],
        "semantic_role": "compact_button_disabled_medium",
    },
    {
        "id": "ui_compact_bases_v5_button_disabled_long_slice9",
        "file": "compact_button_disabled_long_v5_slice9.png",
        "kind": "slice9",
        "slice9": [48, 22, 48, 22],
        "semantic_role": "compact_button_disabled_long",
    },
    {
        "id": "ui_compact_bases_v5_journal_panel_slice9",
        "file": "compact_journal_panel_v5_slice9.png",
        "kind": "slice9",
        "slice9": [76, 76, 76, 76],
        "semantic_role": "compact_journal_panel",
    },
]

FISHING_UI_ASSETS = [
    {
        "id": "fishing_primary_button_slice9",
        "crop": (67, 61, 606, 279),
        "size": (320, 128),
        "kind": "slice9",
        "slice9": [76, 40, 76, 40],
        "semantic_role": "fishing_primary_action_button",
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
        "source_note": "partial runtime slice; magenta-adjacent purple art is accepted for prototype only",
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


HEADER = """#ifndef RUNE_MARCHES_ASSETS_GEN_H
#define RUNE_MARCHES_ASSETS_GEN_H

#include <stdint.h>

typedef enum RuneAssetId {
"""


def enum_name(asset_id: str) -> str:
    return "RUNE_ASSET_" + asset_id.upper()


def sanitize(asset_id: str) -> str:
    return "rune_asset_" + asset_id


def repo_path(path: Path) -> str:
    try:
        return path.resolve().relative_to(Path.cwd().resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def process_asset(source: Image.Image, spec: dict) -> Image.Image:
    image = source.crop(spec["crop"]).resize(spec["size"], Image.Resampling.LANCZOS).convert("RGBA")
    image = ImageEnhance.Color(image).enhance(1.08)
    image = ImageEnhance.Contrast(image).enhance(1.06)
    mask_kind = spec.get("mask")
    if mask_kind == "diamond":
        w, h = image.size
        mask = Image.new("L", image.size, 0)
        draw = ImageDraw.Draw(mask)
        draw.polygon([(w * 0.50, h * 0.00), (w * 0.98, h * 0.42), (w * 0.50, h * 1.00), (w * 0.02, h * 0.42)], fill=255)
        mask = mask.filter(ImageFilter.GaussianBlur(1.4))
        image.putalpha(mask)
    elif mask_kind == "ellipse":
        w, h = image.size
        mask = Image.new("L", image.size, 0)
        draw = ImageDraw.Draw(mask)
        inset_x = max(2, int(w * 0.04))
        inset_y = max(2, int(h * 0.04))
        draw.ellipse((inset_x, inset_y, w - inset_x, h - inset_y), fill=255)
        mask = mask.filter(ImageFilter.GaussianBlur(2.0))
        image.putalpha(mask)
    if spec.get("disabled"):
        gray = ImageOps.grayscale(image)
        image = ImageOps.colorize(gray, black="#22242a", white="#8c8172").convert("RGBA")
        image.putalpha(210)
    return image


def remove_border_chroma(image: Image.Image, key=(255, 0, 255), tolerance: int = 8) -> Image.Image:
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    w, h = rgba.size
    seen = set()
    stack = []

    def is_key(x: int, y: int) -> bool:
        r, g, b, _a = pixels[x, y]
        return abs(r - key[0]) <= tolerance and abs(g - key[1]) <= tolerance and abs(b - key[2]) <= tolerance

    for x in range(w):
        if is_key(x, 0):
            stack.append((x, 0))
        if is_key(x, h - 1):
            stack.append((x, h - 1))
    for y in range(h):
        if is_key(0, y):
            stack.append((0, y))
        if is_key(w - 1, y):
            stack.append((w - 1, y))

    while stack:
        x, y = stack.pop()
        if (x, y) in seen or not is_key(x, y):
            continue
        seen.add((x, y))
        pixels[x, y] = (0, 0, 0, 0)
        if x > 0:
            stack.append((x - 1, y))
        if x + 1 < w:
            stack.append((x + 1, y))
        if y > 0:
            stack.append((x, y - 1))
        if y + 1 < h:
            stack.append((x, y + 1))
    return rgba


def process_fishing_ui_asset(source: Image.Image, spec: dict) -> Image.Image:
    image = source.crop(spec["crop"]).convert("RGBA")
    image = key_to_alpha(image, key=(255, 0, 255), exact_tolerance=10, edge_tolerance=42, aggressive_visible_decontaminate=True)
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
    zero_fully_transparent_rgb(image)
    return image


def build_fishing_ui_assets(source_path: Path, runtime_dir: Path) -> list[dict]:
    if not source_path.exists():
        return []
    source = Image.open(source_path).convert("RGB")
    runtime_dir.mkdir(parents=True, exist_ok=True)
    built = []
    for spec in FISHING_UI_ASSETS:
        image = process_fishing_ui_asset(source, spec)
        png_path = runtime_dir / f"{spec['id']}.png"
        image.save(png_path, optimize=True)
        built.append({
            **spec,
            "path": repo_path(png_path),
            "png_size_bytes": png_path.stat().st_size,
            "image": image,
            "pivot": [0.5, 0.5],
            "source_family": "roblox-fishing-ui-icons-source-v2-magenta-clean",
        })
    fishing_data_dir = Path("gamedesign/projects/roblox-fishing/data")
    fishing_data_dir.mkdir(parents=True, exist_ok=True)
    crop_manifest_path = fishing_data_dir / "roblox-fishing-first-visual-v1-crop_manifest.json"
    asset_manifest_path = fishing_data_dir / "roblox-fishing-first-visual-v1-asset_manifest.json"
    crops = []
    assets = []
    for spec in built:
        crop = {
            "id": spec["id"],
            "kind": spec["kind"],
            "rect": list(spec["crop"]),
            "output": spec["path"],
            "output_size": list(spec["size"]),
            "semantic_role": spec.get("semantic_role"),
            "source_family": spec["source_family"],
        }
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
        crops.append(crop)
        assets.append({
            "id": spec["id"],
            "kind": spec["kind"],
            "path": spec["path"],
            "dimensions": list(spec["size"]),
            "png_size_bytes": spec["png_size_bytes"],
            "slice9": crop.get("slice9"),
            "semantic_role": spec.get("semantic_role"),
            "pack_group": "roblox_fishing_ui_common",
            "source_crop": spec["id"],
            "runtime_usage": "native generated C texture array",
        })
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
    crop_manifest_path.write_text(json.dumps(crop_manifest, indent=2) + "\n", encoding="utf-8")
    asset_manifest = {
        "schema": "game.asset_manifest",
        "version": 2,
        "art_job": "gamedesign/projects/roblox-fishing/art_requests/roblox-fishing-first-visual-v1.json",
        "crop_manifest": repo_path(crop_manifest_path),
        "runtime_dir": repo_path(runtime_dir),
        "source_art": repo_path(source_path),
        "source_policy": "partial generated bitmap UI source; not final source-family-complete art",
        "assets": assets,
    }
    asset_manifest_path.write_text(json.dumps(asset_manifest, indent=2) + "\n", encoding="utf-8")
    return built


def load_ui_kit_v2(runtime_dir: Path) -> list[dict]:
    built = []
    for spec in UI_KIT_V2_ASSETS:
        path = runtime_dir / spec["file"]
        if not path.exists():
            raise FileNotFoundError(f"missing Rune Marches UI kit v2 runtime asset: {path}")
        image = Image.open(path).convert("RGBA")
        built.append({
            **spec,
            "path": repo_path(path),
            "png_size_bytes": path.stat().st_size,
            "size": image.size,
            "pivot": [0.5, 0.5],
            "image": image,
        })
    return built


def load_ui_bases_v2(runtime_dir: Path) -> list[dict]:
    built = []
    for spec in UI_BASES_V2_ASSETS:
        path = runtime_dir / spec["file"]
        if not path.exists():
            raise FileNotFoundError(f"missing Rune Marches UI bases v2 runtime asset: {path}")
        image = Image.open(path).convert("RGBA")
        built.append({
            **spec,
            "path": repo_path(path),
            "png_size_bytes": path.stat().st_size,
            "size": image.size,
            "pivot": [0.5, 0.5],
            "image": image,
        })
    return built


def load_ui_compact_bases_v5(runtime_dir: Path) -> list[dict]:
    built = []
    for spec in UI_COMPACT_BASES_V5_ASSETS:
        path = runtime_dir / spec["file"]
        if not path.exists():
            raise FileNotFoundError(f"missing Rune Marches UI compact bases v5 runtime asset: {path}")
        image = Image.open(path).convert("RGBA")
        built.append({
            **spec,
            "path": repo_path(path),
            "png_size_bytes": path.stat().st_size,
            "size": image.size,
            "pivot": [0.5, 0.5],
            "image": image,
        })
    return built


def write_c_array(f, name: str, data: bytes) -> None:
    f.write(f"static const uint8_t {name}[] = {{\n")
    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        f.write("    ")
        f.write(", ".join(str(b) for b in chunk))
        f.write(",\n")
    f.write("};\n\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--runtime-dir", required=True)
    parser.add_argument("--generated-dir", required=True)
    parser.add_argument("--crop-manifest", required=True)
    parser.add_argument("--runtime-manifest", required=True)
    parser.add_argument("--ui-kit-v2-dir", default="assets/runtime/rune-marches-ui-map-rescue-v2")
    parser.add_argument("--ui-bases-v2-dir", default="assets/runtime/rune-marches-ui-bases-v2")
    parser.add_argument("--ui-compact-bases-v5-dir", default="assets/runtime/rune-marches-ui-compact-bases-v5")
    parser.add_argument("--fishing-ui-source", default="gamedesign/projects/roblox-fishing/art/source_sheets/splash-rods-ui-icons-source-v2-magenta-clean.png")
    parser.add_argument("--fishing-ui-runtime-dir", default="assets/runtime/roblox-fishing-ui-v1")
    args = parser.parse_args()

    source_path = Path(args.source)
    runtime_dir = Path(args.runtime_dir)
    generated_dir = Path(args.generated_dir)
    crop_manifest_path = Path(args.crop_manifest)
    runtime_manifest_path = Path(args.runtime_manifest)
    ui_kit_v2_dir = Path(args.ui_kit_v2_dir)
    ui_bases_v2_dir = Path(args.ui_bases_v2_dir)
    ui_compact_bases_v5_dir = Path(args.ui_compact_bases_v5_dir)
    fishing_ui_source = Path(args.fishing_ui_source)
    fishing_ui_runtime_dir = Path(args.fishing_ui_runtime_dir)

    runtime_dir.mkdir(parents=True, exist_ok=True)
    generated_dir.mkdir(parents=True, exist_ok=True)

    source = Image.open(source_path).convert("RGB")
    built = []
    for spec in ASSETS:
        image = process_asset(source, spec)
        png_path = runtime_dir / f"{spec['id']}.png"
        image.save(png_path, optimize=True)
        built.append({**spec, "path": repo_path(png_path), "png_size_bytes": png_path.stat().st_size, "image": image})
    built.extend(load_ui_kit_v2(ui_kit_v2_dir))
    built.extend(load_ui_bases_v2(ui_bases_v2_dir))
    built.extend(load_ui_compact_bases_v5(ui_compact_bases_v5_dir))
    built.extend(build_fishing_ui_assets(fishing_ui_source, fishing_ui_runtime_dir))

    header_path = generated_dir / "rune_marches_assets.gen.h"
    source_c_path = generated_dir / "rune_marches_assets.gen.c"

    with header_path.open("w", encoding="utf-8", newline="\n") as f:
        f.write(HEADER)
        for spec in built:
            f.write(f"    {enum_name(spec['id'])},\n")
        f.write("""    RUNE_ASSET_COUNT
} RuneAssetId;

typedef struct RuneAssetImage {
    const char *id;
    uint16_t width;
    uint16_t height;
    const uint8_t *rgba;
    uint32_t rgba_size;
} RuneAssetImage;

extern const RuneAssetImage g_rune_assets[RUNE_ASSET_COUNT];

#endif
""")

    with source_c_path.open("w", encoding="utf-8", newline="\n") as f:
        f.write('#include "generated/rune_marches_assets.gen.h"\n\n')
        for spec in built:
            write_c_array(f, sanitize(spec["id"]), spec["image"].tobytes())
        f.write("const RuneAssetImage g_rune_assets[RUNE_ASSET_COUNT] = {\n")
        for spec in built:
            f.write(
                f'    [{enum_name(spec["id"])}] = {{"{spec["id"]}", {spec["size"][0]}, {spec["size"][1]}, '
                f'{sanitize(spec["id"])}, sizeof({sanitize(spec["id"])})}},\n'
            )
        f.write("};\n")

    crop_manifest = {
        "schema": "game.art_crop_manifest",
        "version": 2,
        "art_job": "gamedesign/projects/rune-marches/art_requests/rune-marches-v1.json",
        "source": repo_path(source_path),
        "output_dir": repo_path(runtime_dir),
        "green_screen": {
            "mode": "source_crop_rgba",
            "notes": "Runtime proof uses selected generated fake-shot crops. Future transparent source sheets should replace panel-backed sprite crops before release.",
        },
        "assets": [
            {
                "id": spec["id"],
                "kind": spec["kind"],
                "source_crop_xyxy": list(spec["crop"]) if "crop" in spec else None,
                "output_size": list(spec["size"]),
                "path": spec["path"],
                "pivot": spec["pivot"],
                "slice9": spec.get("slice9"),
                "semantic_role": spec.get("semantic_role"),
                "status": "runtime_integrated_candidate" if "crop" in spec else "prebuilt_runtime_ui_kit_v2",
            }
            for spec in built
        ],
    }
    crop_manifest_path.write_text(json.dumps(crop_manifest, indent=2) + "\n", encoding="utf-8")

    runtime_manifest = {
        "schema": "game.asset_manifest",
        "version": 2,
        "art_job": "gamedesign/projects/rune-marches/art_requests/rune-marches-v1.json",
        "crop_manifest": repo_path(crop_manifest_path),
        "runtime_dir": repo_path(runtime_dir),
        "generated_code": [
            repo_path(header_path),
            repo_path(source_c_path),
        ],
        "commands": {
            "slice_assets": "py -3.12 tools/assets/build_rune_marches_assets.py --source gamedesign/projects/rune-marches/art/fake_shots/rune-marches-gameplay-v1.png --runtime-dir assets/runtime/rune-marches-v1 --generated-dir src/generated --crop-manifest gamedesign/projects/rune-marches/data/rune-marches-v1-crop_manifest.json --runtime-manifest gamedesign/projects/rune-marches/data/rune-marches-v1-runtime_asset_manifest.json",
            "source_ui_bases_v2": "py -3.12 tools/assets/build_rune_marches_ui_bases_v2.py",
            "source_ui_compact_bases_v5": "py -3.12 tools/assets/build_rune_marches_ui_compact_bases_v5.py",
            "build_pack": "not wired yet; native proof uses generated C texture arrays",
            "native_evidence_desktop": "py -3.12 tmp/rune_marches_scenario.py tmp/rune_marches/native_first_slice_labeled.png 960x540",
            "native_evidence_portrait": "py -3.12 tmp/rune_marches_scenario.py tmp/rune_marches/native_first_slice_portrait_current.png 360x640",
        },
        "web_mobile_size_plan": {
            "first_test": "ship PNG-derived RGBA proof for native, then encode runtime PNG/WebP or engine compressed pack before T0001 web validation",
            "budget_note": "visual quality wins for first audience test; keep individual UI/icon assets small and avoid full-screen baked UI as the dynamic runtime surface matures",
            "current_png_total_bytes": sum(spec["png_size_bytes"] for spec in built),
        },
        "assets": [
            {
                "id": spec["id"],
                "path": spec["path"],
                "kind": spec["kind"],
                "dimensions": list(spec["size"]),
                "png_size_bytes": spec["png_size_bytes"],
                "slice9": spec.get("slice9"),
                "semantic_role": spec.get("semantic_role"),
                "runtime_usage": "native nt_gfx texture",
            }
            for spec in built
        ],
    }
    runtime_manifest_path.write_text(json.dumps(runtime_manifest, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
