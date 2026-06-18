#!/usr/bin/env python3
"""Build source material atlases for Backrooms Liminal."""

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.assets.atomic_io import atomic_temp_path, write_json_atomic

try:
    from PIL import Image
except ImportError as exc:  # pragma: no cover - environment failure, surfaced clearly.
    raise SystemExit("error: Pillow is required to build Backrooms material atlas from generated source art") from exc

OUT_DIR = ROOT / "assets" / "backrooms-liminal" / "materials"
ATLAS = OUT_DIR / "portal_material_atlas.ppm"
MANIFEST = OUT_DIR / "portal_material_atlas.json"
SOURCE_IMAGE = ROOT / "gamedesign" / "projects" / "backrooms-liminal" / "art" / "source" / "portal_material_source_sheet_v1.png"
ART_JOB = ROOT / "gamedesign" / "projects" / "backrooms-liminal" / "art_requests" / "t0010_portal_material_atlas_v1.json"
GENERATION_RECORD = ROOT / "gamedesign" / "projects" / "backrooms-liminal" / "art" / "generation_records" / "portal_material_source_sheet_v1.json"
WIDTH = 256
HEIGHT = 256
REGION_SIZE = 128

REGIONS = {
    "wallpaper": [0, 0, REGION_SIZE, REGION_SIZE],
    "carpet": [REGION_SIZE, 0, REGION_SIZE, REGION_SIZE],
    "ceiling_tile": [0, REGION_SIZE, REGION_SIZE, REGION_SIZE],
    "aged_trim": [REGION_SIZE, REGION_SIZE, REGION_SIZE, REGION_SIZE],
}

def paste_rgb_tile(data: bytearray, tile: Image.Image, dst_x: int, dst_y: int) -> None:
    rgb = tile.convert("RGB")
    pixels = rgb.tobytes()
    for y in range(REGION_SIZE):
        src_row = y * REGION_SIZE * 3
        dst_row = ((dst_y + y) * WIDTH + dst_x) * 3
        data[dst_row : dst_row + REGION_SIZE * 3] = pixels[src_row : src_row + REGION_SIZE * 3]


def crop_source_tiles() -> tuple[bytearray, dict[str, list[int]]]:
    if not SOURCE_IMAGE.exists():
        raise SystemExit(f"error: missing material source image: {SOURCE_IMAGE}")
    image = Image.open(SOURCE_IMAGE).convert("RGB")
    source_width, source_height = image.size
    if source_width < 512 or source_height < 512:
        raise SystemExit(f"error: source image is too small for material atlas: {source_width}x{source_height}")

    half_w = source_width // 2
    half_h = source_height // 2
    inset = max(4, min(source_width, source_height) // 160)
    crops = {
        "wallpaper": (0 + inset, 0 + inset, half_w - inset, half_h - inset),
        "carpet": (half_w + inset, 0 + inset, source_width - inset, half_h - inset),
        "ceiling_tile": (0 + inset, half_h + inset, half_w - inset, source_height - inset),
        "aged_trim": (half_w + inset, half_h + inset, source_width - inset, source_height - inset),
    }
    resample = getattr(getattr(Image, "Resampling", Image), "LANCZOS", Image.BICUBIC)
    data = bytearray(WIDTH * HEIGHT * 3)
    for name, (left, top, right, bottom) in crops.items():
        tile = image.crop((left, top, right, bottom)).resize((REGION_SIZE, REGION_SIZE), resample)
        dst_x, dst_y, _, _ = REGIONS[name]
        paste_rgb_tile(data, tile, dst_x, dst_y)
    return data, {name: [int(v) for v in rect] for name, rect in crops.items()}


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    data, source_crops = crop_source_tiles()

    tmp_atlas = atomic_temp_path(ATLAS)
    try:
        tmp_atlas.write_bytes(f"P6\n{WIDTH} {HEIGHT}\n255\n".encode("ascii") + data)
        tmp_atlas.replace(ATLAS)
    finally:
        if tmp_atlas.exists():
            tmp_atlas.unlink()
    write_json_atomic(
        MANIFEST,
        {
            "schema": "backrooms_liminal.material_atlas",
            "version": 1,
            "atlas": str(ATLAS.relative_to(ROOT)).replace("\\", "/"),
            "format": "ppm_p6_rgb8",
            "width": WIDTH,
            "height": HEIGHT,
            "regions": REGIONS,
            "runtime_usage": "src/clean_seed_main.c portal material texture",
            "provenance": {
                "kind": "generated_source_asset",
                "builder": "tools/assets/build_backrooms_liminal_materials.py",
                "source_image": str(SOURCE_IMAGE.relative_to(ROOT)).replace("\\", "/"),
                "source_crops": source_crops,
                "art_job": str(ART_JOB.relative_to(ROOT)).replace("\\", "/"),
                "generation_record": str(GENERATION_RECORD.relative_to(ROOT)).replace("\\", "/"),
                "note": "Iteration generated source sheet for the portal material contract; replace with a fuller material/PBR pack later if needed.",
            },
        },
    )
    print(f"wrote {ATLAS}")
    print(f"wrote {MANIFEST}")


if __name__ == "__main__":
    main()
