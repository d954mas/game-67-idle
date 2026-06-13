#!/usr/bin/env python3
"""Build runtime-ready 67 World PNG assets from crop manifests."""

from __future__ import annotations

import json
from collections import deque
from pathlib import Path

from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parents[2]
CROP_MANIFEST = ROOT / "gamedesign" / "meme-evolution" / "data" / "art_crop_manifest.json"
ASSET_MANIFEST = ROOT / "gamedesign" / "meme-evolution" / "data" / "asset_manifest.json"


def rel(path: Path) -> str:
    return str(path.relative_to(ROOT)).replace("\\", "/")


def remove_green_background(img: Image.Image, mode: str = "edge") -> Image.Image:
    rgba = img.convert("RGBA")
    if mode == "none":
        return rgba
    pixels = rgba.load()
    width, height = rgba.size
    key = "magenta" if mode.startswith("magenta") else "green"
    mode_base = mode.removeprefix("magenta_")

    alpha = Image.new("L", rgba.size, 255)
    alpha_px = alpha.load()
    visited = set()
    queue: deque[tuple[int, int]] = deque()

    def is_key_like(x: int, y: int, soft: bool = False) -> bool:
        r, g, b, _ = pixels[x, y]
        if key == "magenta":
            magenta_score = min(r, b) - g
            if soft:
                return r > 150 and b > 150 and g < 145 and magenta_score > 24
            return r > 190 and b > 190 and g < 120 and magenta_score > 52
        green_score = g - max(r, b)
        if soft:
            return g > 90 and green_score > 18 and g > int(r * 1.08) and g > int(b * 1.08)
        return g > 115 and green_score > 34 and g > int(r * 1.18) and g > int(b * 1.18)

    if mode_base == "global":
        for y in range(height):
            for x in range(width):
                if is_key_like(x, y):
                    alpha_px[x, y] = 0
                elif is_key_like(x, y, soft=True):
                    alpha_px[x, y] = 80
        alpha = alpha.filter(ImageFilter.GaussianBlur(radius=0.45))
        rgba.putalpha(alpha)
        return rgba

    for x in range(width):
        for y in (0, height - 1):
            if is_key_like(x, y):
                queue.append((x, y))
                visited.add((x, y))
    for y in range(height):
        for x in (0, width - 1):
            if is_key_like(x, y):
                queue.append((x, y))
                visited.add((x, y))

    while queue:
        x, y = queue.popleft()
        alpha_px[x, y] = 0
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if nx < 0 or ny < 0 or nx >= width or ny >= height or (nx, ny) in visited:
                continue
            if is_key_like(nx, ny):
                visited.add((nx, ny))
                queue.append((nx, ny))

    for x, y in list(visited):
        for nx in range(max(0, x - 1), min(width, x + 2)):
            for ny in range(max(0, y - 1), min(height, y + 2)):
                if (nx, ny) not in visited and is_key_like(nx, ny, soft=True):
                    alpha_px[nx, ny] = min(alpha_px[nx, ny], 80)

    alpha = alpha.filter(ImageFilter.GaussianBlur(radius=0.45))
    rgba.putalpha(alpha)
    return rgba


def trim_transparent(img: Image.Image, pad: int = 4) -> tuple[Image.Image, list[int]]:
    alpha = img.getchannel("A")
    bbox = alpha.getbbox()
    if bbox is None:
        return img, [0, 0, img.width, img.height]
    left, top, right, bottom = bbox
    left = max(0, left - pad)
    top = max(0, top - pad)
    right = min(img.width, right + pad)
    bottom = min(img.height, bottom + pad)
    return img.crop((left, top, right, bottom)), [left, top, right, bottom]


def adjusted_slice9(slice9: list[int] | None, trim_box: list[int], original_size: tuple[int, int], output_size: tuple[int, int]) -> list[int] | None:
    if not slice9:
        return None
    left, top, right, bottom = slice9
    trim_left, trim_top, trim_right, trim_bottom = trim_box
    original_w, original_h = original_size
    width, height = output_size
    left = max(1, min(width - 2, left - trim_left))
    top = max(1, min(height - 2, top - trim_top))
    right = max(1, min(width - left - 1, right - (original_w - trim_right)))
    bottom = max(1, min(height - top - 1, bottom - (original_h - trim_bottom)))
    return [int(left), int(top), int(right), int(bottom)]


def main() -> int:
    if not CROP_MANIFEST.exists():
        raise SystemExit(f"missing crop manifest: {CROP_MANIFEST}")

    spec = json.loads(CROP_MANIFEST.read_text(encoding="utf-8"))
    out_dir = ROOT / spec["output_dir"]
    out_dir.mkdir(parents=True, exist_ok=True)
    ASSET_MANIFEST.parent.mkdir(parents=True, exist_ok=True)

    runtime_assets = []
    for source in spec["sources"]:
        source_path = ROOT / source["path"]
        if not source_path.exists():
            raise SystemExit(f"missing source sheet: {source_path}")
        sheet = Image.open(source_path).convert("RGBA")

        for asset in source["assets"]:
            crop = sheet.crop(tuple(asset["box"]))
            cleaned, trim_box = trim_transparent(remove_green_background(crop, asset.get("chroma_mode", "edge")))
            out_path = out_dir / f"{asset['id']}-v1.png"
            cleaned.save(out_path)

            entry = {
                "id": asset["id"],
                "kind": asset["kind"],
                "source": rel(source_path),
                "source_box": asset["box"],
                "trim_box": trim_box,
                "runtime": rel(out_path),
                "size": [cleaned.width, cleaned.height],
            }
            if "pivot" in asset:
                entry["pivot"] = asset["pivot"]
            slice9 = adjusted_slice9(asset.get("slice9"), trim_box, crop.size, cleaned.size)
            if slice9:
                entry["slice9"] = slice9
            runtime_assets.append(entry)

    manifest = {
        "schema": "67_world.asset_manifest",
        "version": 2,
        "art_request": "gamedesign/meme-evolution/art_requests/67-world-reusable-ui-v1.json",
        "crop_manifest": rel(CROP_MANIFEST),
        "runtime_dir": rel(out_dir),
        "commands": {
            "slice_assets": "py -3.12 tools/assets/build_67_world_art.py",
            "configure_native": "cmake --preset native-debug",
            "build_pack_tool": "cmake --build --preset native-debug --target build_67_world_packs",
            "build_pack": "build/game_seed/native-debug/build_67_world_packs.exe build/game_seed/67-world-packs"
        },
        "assets": runtime_assets,
    }
    ASSET_MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {len(runtime_assets)} assets -> {out_dir}")
    print(f"wrote manifest -> {ASSET_MANIFEST}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
