#!/usr/bin/env python3
"""Import selected generated Critter Corral source art into runtime sprite slots.

This is the corrective T0070 path: the major visible sprites are cut from
project-saved image generation outputs, then packed by the existing native asset
pipeline. Supporting FX/icons can still come from generate_sprites.py until
their own generated source families exist.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Any

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

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
DEFAULT_NATIVE_TOOL = ROOT / "build" / "_cmake" / "native-debug" / "import_generated_core_assets_native.exe"
KEYED_CACHE_DIR = ROOT / "tmp" / "generated_core_keyed_sources"
KEYED_CACHE_VERSION = "t0070-magenta-key-v1"
RUNTIME_CACHE_DIR = ROOT / "tmp" / "generated_core_runtime_assets"
RUNTIME_CACHE_VERSION = "t0070-runtime-cleanup-v2"


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
    import numpy as np

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


def source_region_rect(source_size: tuple[int, int], spec: dict[str, Any]) -> tuple[int, int, int, int]:
    slot = spec.get("slot")
    if isinstance(slot, list) and len(slot) == 2:
        index = int(slot[0])
        count = int(slot[1])
        if count <= 0 or index < 0 or index >= count:
            raise RuntimeError(f"invalid slot for {spec['id']}: {slot}")
        cell_w = source_size[0] / float(count)
        x0 = round(cell_w * float(index))
        x1 = round(cell_w * float(index + 1))
        return (x0, 0, x1, source_size[1])
    return (0, 0, source_size[0], source_size[1])


def source_region(source: Image.Image, spec: dict[str, Any]) -> Image.Image:
    return source.crop(source_region_rect(source.size, spec))


def fit_to_canvas(image: Image.Image, size: tuple[int, int], padding: int) -> Image.Image:
    from tools.assets.chroma_key_alpha import (
        bleed_transparent_rgb,
        remove_edge_fringe,
        remove_green_screen_spill,
        remove_source_key_spill,
        repair_visible_halo,
        repair_transparent_edge_rgb,
        zero_fully_transparent_rgb,
    )

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
    bleed_transparent_rgb(canvas, passes=2, key=KEY)
    repair_transparent_edge_rgb(canvas, key=KEY)
    zero_fully_transparent_rgb(canvas)
    return canvas


def apply_tint(image: Image.Image, tint: list[int] | None) -> Image.Image:
    if not tint:
        return image
    from PIL import ImageEnhance, ImageOps

    from tools.assets.chroma_key_alpha import (
        bleed_transparent_rgb,
        remove_edge_fringe,
        remove_green_screen_spill,
        remove_source_key_spill,
        repair_visible_halo,
        repair_transparent_edge_rgb,
        zero_fully_transparent_rgb,
    )

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
    bleed_transparent_rgb(composited, passes=2, key=KEY)
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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--mode",
        choices=("python", "native"),
        default="python",
        help="python keeps the full PIL path; native uses the C worker only for source chroma-keying.",
    )
    parser.add_argument(
        "--native-tool",
        type=Path,
        default=DEFAULT_NATIVE_TOOL,
        help="Path to import_generated_core_assets_native.exe for --mode native.",
    )
    parser.add_argument(
        "--native-threads",
        type=int,
        default=max(1, min(4, os.cpu_count() or 1)),
        help="Worker threads used by the native source chroma-key step.",
    )
    return parser.parse_args()


def unique_source_keys() -> list[str]:
    source_keys: list[str] = []
    seen: set[str] = set()
    for spec in ASSETS:
        source_key = spec["source"]
        if source_key not in seen:
            source_keys.append(source_key)
            seen.add(source_key)
    return source_keys


def keyed_cache_path(source_key: str) -> Path:
    return KEYED_CACHE_DIR / f"{source_key}.rgba"


def keyed_cache_meta_path(source_key: str) -> Path:
    return KEYED_CACHE_DIR / f"{source_key}.json"


def runtime_cache_meta_path(spec: dict[str, Any]) -> Path:
    return RUNTIME_CACHE_DIR / f"{spec['output']}.json"


def source_fingerprint(source_key: str) -> dict[str, Any]:
    source_path = SOURCE_DIR / source_key
    stat = source_path.stat()
    return {
        "source": rel(source_path),
        "source_mtime_ns": stat.st_mtime_ns,
        "source_size_bytes": stat.st_size,
        "key_algorithm": KEYED_CACHE_VERSION,
        "key": "#ff00ff",
    }


def keyed_cache_valid(source_key: str) -> bool:
    cache_path = keyed_cache_path(source_key)
    meta_path = keyed_cache_meta_path(source_key)
    if not cache_path.exists() or not meta_path.exists():
        return False
    try:
        meta = json.loads(meta_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False
    expected = source_fingerprint(source_key)
    return all(meta.get(key) == value for key, value in expected.items())


def runtime_output_fingerprint(spec: dict[str, Any]) -> dict[str, Any]:
    return {
        "runtime_algorithm": RUNTIME_CACHE_VERSION,
        "source": source_fingerprint(spec["source"]),
        "spec": spec,
    }


def runtime_output_valid(spec: dict[str, Any], output_path: Path) -> bool:
    meta_path = runtime_cache_meta_path(spec)
    if not output_path.exists() or not meta_path.exists():
        return False
    try:
        meta = json.loads(meta_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False
    return meta == runtime_output_fingerprint(spec)


def write_runtime_output_meta(spec: dict[str, Any]) -> None:
    write_json_atomic(runtime_cache_meta_path(spec), runtime_output_fingerprint(spec))


def write_rgba_cache_atomic(path: Path, image: Image.Image) -> None:
    rgba = image.convert("RGBA")
    tmp_path = path.with_name(f"{path.name}.tmp")
    with tmp_path.open("wb") as handle:
        handle.write(f"CCRGBA1 {rgba.width} {rgba.height}\n".encode("ascii"))
        handle.write(rgba.tobytes())
    tmp_path.replace(path)


def write_keyed_cache_meta(source_key: str, producer: str) -> None:
    meta = source_fingerprint(source_key)
    meta["producer"] = producer
    write_json_atomic(keyed_cache_meta_path(source_key), meta)


def write_native_key_plan(plan_path: Path, source_keys: list[str]) -> dict[str, Path]:
    lines = ["# source_path\toutput_path"]
    keyed_paths: dict[str, Path] = {}
    for source_key in source_keys:
        output_path = keyed_cache_path(source_key)
        keyed_paths[source_key] = output_path
        lines.append("\t".join([(SOURCE_DIR / source_key).as_posix(), output_path.as_posix()]))
    plan_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return keyed_paths


def run_native_key_worker(native_tool: Path, threads: int, stale_sources: list[str]) -> None:
    if not stale_sources:
        return
    native_tool = native_tool.resolve()
    if not native_tool.exists():
        raise SystemExit(
            f"missing native worker: {native_tool}\n"
            "Build it first: cmake --build build/_cmake/native-debug --target import_generated_core_assets_native"
        )
    tmp_dir = ROOT / "tmp"
    tmp_dir.mkdir(exist_ok=True)
    KEYED_CACHE_DIR.mkdir(exist_ok=True)
    with tempfile.NamedTemporaryFile("w", suffix=".tsv", delete=False, dir=tmp_dir, encoding="utf-8") as handle:
        plan_path = Path(handle.name)
    try:
        write_native_key_plan(plan_path, stale_sources)
        subprocess.run(
            [
                str(native_tool),
                "--key-plan",
                str(plan_path),
                "--threads",
                str(max(1, threads)),
            ],
            cwd=ROOT,
            check=True,
        )
        for source_key in stale_sources:
            write_keyed_cache_meta(source_key, "native")
    finally:
        plan_path.unlink(missing_ok=True)


def load_native_rgba_cache(path: Path) -> Image.Image:
    with path.open("rb") as handle:
        header = handle.readline().decode("ascii").strip().split()
        if len(header) != 3 or header[0] != "CCRGBA1":
            raise RuntimeError(f"invalid native RGBA cache header: {path}")
        width = int(header[1])
        height = int(header[2])
        data = handle.read()
    expected = width * height * 4
    if len(data) != expected:
        raise RuntimeError(f"invalid native RGBA cache size for {path}: got {len(data)}, expected {expected}")
    return Image.frombytes("RGBA", (width, height), data)


def main() -> None:
    args = parse_args()
    SPRITE_DIR.mkdir(parents=True, exist_ok=True)
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    REVIEW_DIR.mkdir(parents=True, exist_ok=True)
    KEYED_CACHE_DIR.mkdir(parents=True, exist_ok=True)
    RUNTIME_CACHE_DIR.mkdir(parents=True, exist_ok=True)

    crop_sources: dict[str, dict[str, Any]] = {}
    runtime_assets: list[dict[str, Any]] = []
    contact_items: list[tuple[str, Image.Image]] = []
    keyed_source_cache: dict[str, Image.Image] = {}
    source_keys = unique_source_keys()
    stale_runtime_sources = [
        spec["source"] for spec in ASSETS if not runtime_output_valid(spec, SPRITE_DIR / spec["output"])
    ]
    stale_runtime_source_set = set(stale_runtime_sources)
    sources_needed = [source_key for source_key in source_keys if source_key in stale_runtime_source_set]
    stale_sources = [source_key for source_key in sources_needed if not keyed_cache_valid(source_key)]

    if args.mode == "native":
        run_native_key_worker(args.native_tool, args.native_threads, stale_sources)

    for spec in ASSETS:
        source_path = SOURCE_DIR / spec["source"]
        if not source_path.exists():
            raise SystemExit(f"missing generated source: {source_path}")
        output_path = SPRITE_DIR / spec["output"]
        source_key = spec["source"]
        if runtime_output_valid(spec, output_path):
            with Image.open(source_path) as source:
                x0, y0, x1, y1 = source_region_rect(source.size, spec)
            region_size = (x1 - x0, y1 - y0)
            final = Image.open(output_path).convert("RGBA")
        else:
            keyed_source = keyed_source_cache.get(source_key)
            if keyed_source is None:
                if keyed_cache_valid(source_key):
                    keyed_source = load_native_rgba_cache(keyed_cache_path(source_key))
                else:
                    if args.mode == "native":
                        raise RuntimeError(f"native keyed cache was not created for {source_key}")
                    source = Image.open(source_path).convert("RGBA")
                    keyed_source = fast_key_to_alpha(source)
                    write_rgba_cache_atomic(keyed_cache_path(source_key), keyed_source)
                    write_keyed_cache_meta(source_key, "python")
                keyed_source_cache[source_key] = keyed_source
            region = source_region(keyed_source, spec)
            region_size = region.size
            trimmed = fit_to_canvas(region, tuple(spec["size"]), int(spec.get("padding", 0)))
            final = apply_tint(trimmed, spec.get("tint"))
            save_image_atomic(final, output_path)
            write_runtime_output_meta(spec)
        contact_items.append((spec["id"], final))

        bbox = alpha_bbox(final)
        crop_entry: dict[str, Any] = {
            "id": spec["id"],
            "kind": spec["kind"],
            "rect": [0, 0, region_size[0], region_size[1]],
            "output": rel(output_path),
            "source_rect": [0, 0, region_size[0], region_size[1]],
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
