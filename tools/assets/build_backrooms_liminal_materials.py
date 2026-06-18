#!/usr/bin/env python3
"""Build source material atlases for Backrooms Liminal."""

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.assets.atomic_io import atomic_temp_path, write_json_atomic

OUT_DIR = ROOT / "assets" / "backrooms-liminal" / "materials"
ATLAS = OUT_DIR / "portal_material_atlas.ppm"
MANIFEST = OUT_DIR / "portal_material_atlas.json"
WIDTH = 256
HEIGHT = 256


def clamp_u8(value: int) -> int:
    return max(0, min(255, value))


def pixel(x: int, y: int) -> tuple[int, int, int]:
    tile_x = x & 127
    tile_y = y & 127
    zone = (1 if x >= 128 else 0) + (2 if y >= 128 else 0)
    hashed = (tile_x * 37 + tile_y * 73 + ((tile_x * tile_y) % 97)) & 255

    if zone == 0:
        seam = tile_x % 32 == 0 or tile_y % 46 == 0
        fleck = ((tile_x * 17 + tile_y * 31 + ((tile_x * tile_y) % 19)) & 23) == 0
        stain = (((tile_x * 3 + tile_y * 11) & 63) < 7) and (((tile_x + tile_y * 5) & 15) < 5)
        vertical_wear = (tile_x % 32 in (30, 1)) and (((tile_y * 7 + tile_x) & 7) < 5)
        r = 168 + ((tile_x * 5 + tile_y * 3) & 23)
        g = 151 + ((tile_x * 7 + tile_y * 11) & 19)
        b = 78 + ((tile_x * 13 + tile_y * 2) & 15)
        if seam:
            r -= 44
            g -= 40
            b -= 24
        if fleck:
            r -= 54
            g -= 45
            b -= 25
        if stain:
            r -= 42
            g -= 38
            b -= 18
        if vertical_wear:
            r += 22
            g += 16
            b += 7
    elif zone == 1:
        seam = tile_x % 26 == 0 or tile_y % 30 == 0
        fiber = ((tile_x * 19 + tile_y * 5) & 15) < 4
        damp = ((tile_x - 54) * (tile_x - 54) + (tile_y - 76) * (tile_y - 76)) < 680 or hashed > 236
        r = 86 + ((tile_x * 3 + tile_y * 9) & 19)
        g = 69 + ((tile_x * 11 + tile_y * 5) & 15)
        b = 38 + ((tile_x * 7 + tile_y * 13) & 11)
        if fiber:
            r += 18
            g += 13
            b += 5
        if seam:
            r -= 34
            g -= 27
            b -= 16
        if damp:
            r -= 31
            g -= 27
            b -= 14
    elif zone == 2:
        grid = tile_x % 38 < 2 or tile_y % 34 < 2
        speckle = hashed > 224
        d = (tile_x - 84) * (tile_x - 84) + (tile_y - 38) * (tile_y - 38)
        water_ring = 360 < d < 610
        r = 156 + ((tile_x * 5 + tile_y * 2) & 17)
        g = 148 + ((tile_x * 2 + tile_y * 7) & 15)
        b = 102 + ((tile_x * 13 + tile_y * 3) & 13)
        if grid:
            r -= 55
            g -= 51
            b -= 36
        if speckle:
            r -= 39
            g -= 36
            b -= 26
        if water_ring:
            r -= 30
            g -= 32
            b -= 18
    else:
        rib = tile_x % 18 < 3 or tile_y % 42 < 3
        scratch = ((tile_x * 29 + tile_y * 17) & 31) < 3
        r = 116 + ((tile_x * 9 + tile_y * 4) & 15)
        g = 92 + ((tile_x * 4 + tile_y * 11) & 13)
        b = 46 + ((tile_x * 7 + tile_y * 2) & 9)
        if rib:
            r -= 44
            g -= 34
            b -= 18
        if scratch:
            r += 42
            g += 31
            b += 12

    return clamp_u8(r), clamp_u8(g), clamp_u8(b)


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    data = bytearray()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            data.extend(pixel(x, y))

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
            "regions": {
                "wallpaper": [0, 0, 128, 128],
                "carpet": [128, 0, 128, 128],
                "ceiling_tile": [0, 128, 128, 128],
                "aged_trim": [128, 128, 128, 128],
            },
            "runtime_usage": "src/clean_seed_main.c portal material texture",
            "provenance": {
                "kind": "procedural_source_asset",
                "builder": "tools/assets/build_backrooms_liminal_materials.py",
                "note": "Iteration source asset for the portal material contract; replace with generated or artist-authored material sources before final art.",
            },
        },
    )
    print(f"wrote {ATLAS}")
    print(f"wrote {MANIFEST}")


if __name__ == "__main__":
    main()
