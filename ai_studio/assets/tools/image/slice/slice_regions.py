#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
import zipfile
from io import BytesIO
from pathlib import Path
from typing import Any

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[5]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import save_image_atomic, write_json_atomic


RGB = tuple[int, int, int]
ALPHA_KEY_MATTE = "key_matte"
ALPHA_GENERATION = "generation"


def rel(path: Path | None) -> str | None:
    if path is None:
        return None
    resolved = path.resolve()
    try:
        return resolved.relative_to(ROOT.resolve()).as_posix()
    except ValueError:
        return resolved.as_posix()


def safe_name(value: str, fallback: str = "asset") -> str:
    cleaned = "".join(ch if ch.isalnum() or ch in "_-" else "_" for ch in value.strip())
    cleaned = cleaned.strip("_")[:80]
    return cleaned or fallback


def parse_color(value: str) -> RGB:
    text = value.strip().lstrip("#")
    if len(text) != 6:
        raise argparse.ArgumentTypeError("color must be #rrggbb")
    try:
        return tuple(int(text[index : index + 2], 16) for index in (0, 2, 4))  # type: ignore[return-value]
    except ValueError as exc:
        raise argparse.ArgumentTypeError("color must be #rrggbb") from exc


def format_color(value: RGB) -> str:
    return "#{:02x}{:02x}{:02x}".format(*value)


def estimate_border_key_color(image: Image.Image) -> RGB:
    rgba = image.convert("RGBA")
    pixels = list(rgba.crop((0, 0, rgba.width, 1)).getdata())
    pixels += list(rgba.crop((0, rgba.height - 1, rgba.width, rgba.height)).getdata())
    pixels += list(rgba.crop((0, 0, 1, rgba.height)).getdata())
    pixels += list(rgba.crop((rgba.width - 1, 0, rgba.width, rgba.height)).getdata())
    counts: dict[RGB, int] = {}
    for red, green, blue, alpha in pixels:
        if alpha == 0:
            continue
        key = (int(red), int(green), int(blue))
        counts[key] = counts.get(key, 0) + 1
    if not counts:
        return (255, 0, 255)
    return max(counts.items(), key=lambda item: item[1])[0]


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def rect_tuple(value: Any, *, label: str) -> tuple[int, int, int, int]:
    if not isinstance(value, list | tuple) or len(value) != 4:
        raise ValueError(f"{label} must be [x, y, width, height]")
    x, y, width, height = [int(part) for part in value]
    if width <= 0 or height <= 0:
        raise ValueError(f"{label} width/height must be > 0")
    return x, y, width, height


def clamp_rect(rect: tuple[int, int, int, int], image_size: tuple[int, int]) -> tuple[int, int, int, int]:
    x, y, width, height = rect
    image_width, image_height = image_size
    left = max(0, min(image_width - 1, x))
    top = max(0, min(image_height - 1, y))
    right = max(left + 1, min(image_width, x + width))
    bottom = max(top + 1, min(image_height, y + height))
    return left, top, right - left, bottom - top


def polygon_points(value: Any, *, label: str, image_size: tuple[int, int]) -> list[list[int]]:
    if value is None:
        return []
    if not isinstance(value, list | tuple):
        raise ValueError(f"{label} must be a list of [x, y] points")
    image_width, image_height = image_size
    points: list[list[int]] = []
    for index, point in enumerate(value, start=1):
        if not isinstance(point, list | tuple) or len(point) != 2:
            raise ValueError(f"{label} point {index} must be [x, y]")
        x, y = [int(part) for part in point]
        points.append([max(0, min(image_width, x)), max(0, min(image_height, y))])
    if points and len(points) < 3:
        raise ValueError(f"{label} must contain at least 3 points")
    return points


def alpha_policy(value: Any) -> dict[str, Any]:
    raw = value if isinstance(value, dict) else {}
    mode = str(raw.get("mode") or ALPHA_KEY_MATTE).strip().lower()
    if mode in {"dual_plate", "generated", "regenerate"}:
        mode = ALPHA_GENERATION
    if mode not in {ALPHA_KEY_MATTE, ALPHA_GENERATION}:
        mode = ALPHA_KEY_MATTE
    return {
        **raw,
        "mode": mode,
    }


def normalize_regions(data: Any, image_size: tuple[int, int]) -> list[dict[str, Any]]:
    raw_regions = data.get("regions") if isinstance(data, dict) else data
    if not isinstance(raw_regions, list):
        raise ValueError("regions JSON must contain a regions list")
    regions: list[dict[str, Any]] = []
    for index, region in enumerate(raw_regions, start=1):
        if not isinstance(region, dict):
            raise ValueError(f"region {index} must be an object")
        rect = clamp_rect(rect_tuple(region.get("rect"), label=f"region {index} rect"), image_size)
        polygon = polygon_points(region.get("polygon"), label=f"region {index} polygon", image_size=image_size)
        region_id = safe_name(str(region.get("id") or f"region_{index:03d}"), f"region_{index:03d}")
        region_name = str(region.get("name") or "").strip()
        alpha = alpha_policy(region.get("alpha"))
        normalized = {
            **region,
            "id": region_id,
            **({"name": region_name} if region_name else {}),
            "alpha": alpha,
            "rect": list(rect),
            "content_bbox": region.get("content_bbox", list(rect)),
        }
        if polygon:
            normalized["polygon"] = polygon
        regions.append(normalized)
    return regions


def slice_filename(prefix: str, region: dict[str, Any], used: set[str]) -> str:
    base_name = safe_name(str(region.get("name") or region["id"]), "region")
    base = f"{safe_name(prefix)}_{base_name}"
    filename = f"{base}.png"
    suffix = 2
    while filename in used:
        filename = f"{base}_{suffix:03d}.png"
        suffix += 1
    used.add(filename)
    return filename


def checkerboard(size: tuple[int, int], cell: int = 12) -> Image.Image:
    width, height = size
    image = Image.new("RGBA", size, (12, 16, 23, 255))
    draw = ImageDraw.Draw(image)
    for y in range(0, height, cell):
        for x in range(0, width, cell):
            if ((x // cell) + (y // cell)) % 2 == 0:
                draw.rectangle([x, y, min(width, x + cell) - 1, min(height, y + cell) - 1], fill=(27, 36, 48, 255))
    return image


def draw_fit(target: Image.Image, source: Image.Image, box: tuple[int, int, int, int]) -> None:
    x, y, width, height = box
    if source.width <= 0 or source.height <= 0:
        return
    scale = min(width / source.width, height / source.height, 1.0)
    draw_width = max(1, round(source.width * scale))
    draw_height = max(1, round(source.height * scale))
    resized = source.resize((draw_width, draw_height), Image.Resampling.LANCZOS)
    target.alpha_composite(resized, (x + (width - draw_width) // 2, y + (height - draw_height) // 2))


def apply_polygon_mask(crop: Image.Image, region: dict[str, Any]) -> Image.Image:
    polygon = region.get("polygon")
    if not polygon:
        return crop
    x, y, _width, _height = rect_tuple(region["rect"], label=f"{region['id']} rect")
    local_points = [(int(point[0]) - x, int(point[1]) - y) for point in polygon]
    mask = Image.new("L", crop.size, 0)
    ImageDraw.Draw(mask).polygon(local_points, fill=255)
    result = crop.copy()
    alpha = result.getchannel("A")
    masked_alpha = Image.new("L", crop.size, 0)
    masked_alpha.paste(alpha, mask=mask)
    result.putalpha(masked_alpha)
    return result


def apply_alpha(crop: Image.Image, region: dict[str, Any], key_color: RGB) -> tuple[Image.Image, dict[str, Any]]:
    alpha = alpha_policy(region.get("alpha"))
    if alpha["mode"] == ALPHA_GENERATION:
        return crop, {
            **alpha,
            "mode": ALPHA_GENERATION,
            "status": "needs_generation",
            "reason": alpha.get("reason") or "selected for generated dual-plate alpha",
        }
    # LAW (lead, 2026-07-02): no silent fallbacks. If the key-matte alpha tool is
    # unavailable we raise loudly instead of degrading to a low-quality cutout.
    try:
        from ai_studio.assets.tools.image.alpha_matte.key_matte import key_matte_cutout
    except ImportError as exc:
        raise RuntimeError(
            "key_matte alpha tool is required for the key_matte alpha path but could not be "
            "imported (ai_studio.assets.tools.image.alpha_matte.key_matte); install the studio "
            f"Python deps and verify the module path: {exc}"
        ) from exc
    output = key_matte_cutout(crop, key_color)
    return output, {
        **alpha,
        "mode": ALPHA_KEY_MATTE,
        "status": "applied",
        "key_color": format_color(key_color),
    }


def build_review_sheet(slices: list[dict[str, Any]], output: Path) -> None:
    if not slices:
        image = Image.new("RGBA", (360, 180), (13, 17, 23, 255))
        draw = ImageDraw.Draw(image)
        draw.text((18, 18), "No regions", fill=(148, 163, 184, 255))
        save_image_atomic(image, output)
        return

    columns = min(4, max(1, round(len(slices) ** 0.5 + 0.499)))
    rows = (len(slices) + columns - 1) // columns
    cell_width = 180
    cell_height = 158
    margin = 16
    sheet = Image.new("RGBA", (margin * 2 + columns * cell_width, margin * 2 + rows * cell_height), (13, 17, 23, 255))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for index, item in enumerate(slices):
        col = index % columns
        row = index // columns
        x = margin + col * cell_width
        y = margin + row * cell_height
        board = checkerboard((cell_width - 12, cell_height - 36))
        sheet.alpha_composite(board, (x, y))
        crop = Image.open(item["path"]).convert("RGBA")
        draw_fit(sheet, crop, (x + 8, y + 8, cell_width - 28, cell_height - 56))
        draw.rectangle([x, y, x + cell_width - 13, y + cell_height - 37], outline=(44, 55, 71, 255), width=1)
        draw.text((x, y + cell_height - 24), item.get("name") or item["id"], fill=(231, 237, 245, 255), font=font)
        draw.text((x, y + cell_height - 10), f"{crop.width}x{crop.height}", fill=(148, 163, 184, 255), font=font)
    save_image_atomic(sheet, output)


def png_bytes(path: Path) -> bytes:
    return path.read_bytes()


def write_zip(zip_output: Path, *, regions_path: Path, manifest_path: Path | None, review_sheet: Path | None, slices: list[dict[str, Any]]) -> None:
    zip_output.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = zip_output.with_name(f".{zip_output.name}.tmp")
    try:
        with zipfile.ZipFile(tmp_path, "w", compression=zipfile.ZIP_STORED) as archive:
            archive.write(regions_path, "regions.json")
            if manifest_path is not None and manifest_path.exists():
                archive.write(manifest_path, "manifest.json")
            if review_sheet is not None and review_sheet.exists():
                archive.write(review_sheet, "review_sheet.png")
            for item in slices:
                archive.writestr(f"slices/{item['file']}", png_bytes(item["path"]))
        tmp_path.replace(zip_output)
    finally:
        if tmp_path.exists():
            tmp_path.unlink()


def slice_regions(
    *,
    source: Path,
    regions_path: Path,
    output_dir: Path,
    prefix: str,
    review_sheet: Path | None,
    zip_output: Path | None,
    manifest_output: Path | None,
    key_color: RGB | None,
) -> dict[str, Any]:
    image = Image.open(source).convert("RGBA")
    alpha_key_color = key_color or estimate_border_key_color(image)
    regions_data = read_json(regions_path)
    regions = normalize_regions(regions_data, image.size)
    slices_dir = output_dir / "slices"
    slices_dir.mkdir(parents=True, exist_ok=True)
    slices: list[dict[str, Any]] = []
    used_filenames: set[str] = set()
    for region in regions:
        x, y, width, height = rect_tuple(region["rect"], label=f"{region['id']} rect")
        crop = image.crop((x, y, x + width, y + height))
        crop, alpha = apply_alpha(crop, region, alpha_key_color)
        crop = apply_polygon_mask(crop, region)
        filename = slice_filename(prefix, region, used_filenames)
        output_path = slices_dir / filename
        save_image_atomic(crop, output_path)
        slices.append(
            {
                "id": region["id"],
                "name": region.get("name"),
                "file": filename,
                "path": output_path,
                "rect": [x, y, width, height],
                "polygon": region.get("polygon"),
                "alpha": alpha,
                "width": width,
                "height": height,
            }
        )

    if review_sheet is not None:
        build_review_sheet(slices, review_sheet)

    manifest = {
        "schema": "ai_studio.raster2d.slices.v1",
        "source": rel(source),
        "regions": rel(regions_path),
        "output_dir": rel(output_dir),
        "prefix": safe_name(prefix),
        "slice_count": len(slices),
        "slices": [
            {
                "id": item["id"],
                **({"name": item["name"]} if item.get("name") else {}),
                "file": item["file"],
                "path": rel(item["path"]),
                "rect": item["rect"],
                **({"polygon": item["polygon"]} if item.get("polygon") else {}),
                "alpha": item["alpha"],
                "width": item["width"],
                "height": item["height"],
            }
            for item in slices
        ],
        "review_sheet": rel(review_sheet),
        "zip_output": rel(zip_output),
    }
    if manifest_output is not None:
        write_json_atomic(manifest_output, manifest)
    if zip_output is not None:
        write_zip(zip_output, regions_path=regions_path, manifest_path=manifest_output, review_sheet=review_sheet, slices=slices)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description="Slice a raster source by reviewed region rects and optionally build a review sheet and ZIP.")
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--regions", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--prefix", default="asset")
    parser.add_argument("--review-sheet", type=Path)
    parser.add_argument("--zip-output", type=Path)
    parser.add_argument("--manifest-output", type=Path)
    parser.add_argument("--key-color", type=parse_color)
    args = parser.parse_args()

    manifest = slice_regions(
        source=args.source,
        regions_path=args.regions,
        output_dir=args.output_dir,
        prefix=args.prefix,
        review_sheet=args.review_sheet,
        zip_output=args.zip_output,
        manifest_output=args.manifest_output,
        key_color=args.key_color,
    )
    print(f"pass: sliced {manifest['slice_count']} region(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
