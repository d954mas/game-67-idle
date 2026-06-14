#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from time import perf_counter
from pathlib import Path
from typing import Any

from PIL import Image

try:
    import numpy as np
except ImportError:  # pragma: no cover - fallback path is kept for portable minimal Python installs.
    np = None


ROOT = Path.cwd()
SCRIPT_ROOT = Path(__file__).resolve().parents[2]
if str(SCRIPT_ROOT) not in sys.path:
    sys.path.insert(0, str(SCRIPT_ROOT))

from tools.assets.chroma_key_alpha import (
    is_any_purple_halo_like,
    is_exact_key_like,
    is_green_screen_spill_like,
    is_key_fringe_like,
    is_source_key_spill_like,
)


def project_path(path: str) -> Path:
    candidate = Path(path)
    if candidate.is_absolute():
        return candidate
    return ROOT / candidate


def alpha_bbox(image: Image.Image, threshold: int = 12) -> tuple[int, int, int, int] | None:
    alpha = image.getchannel("A")
    return alpha.point(lambda value: 255 if value > threshold else 0).getbbox()


def touches_transparent(alpha_pixels: Any, x: int, y: int, width: int, height: int, radius: int) -> bool:
    for ny in range(max(0, y - radius), min(height, y + radius + 1)):
        for nx in range(max(0, x - radius), min(width, x + radius + 1)):
            if alpha_pixels[nx, ny] <= 12:
                return True
    return False


def dilate_mask_numpy(mask: Any, radius: int) -> Any:
    padded = np.pad(mask, radius, mode="constant", constant_values=False)
    result = np.zeros(mask.shape, dtype=bool)
    size = radius * 2 + 1
    for y in range(size):
        for x in range(size):
            result |= padded[y : y + mask.shape[0], x : x + mask.shape[1]]
    return result


def key_fringe_mask_array(array: Any) -> Any:
    red = array[..., 0].astype(np.int16)
    green = array[..., 1].astype(np.int16)
    blue = array[..., 2].astype(np.int16)
    return (red > 115) & (blue > 120) & (green < 145) & (red + blue > 300) & (red + blue > green * 3)


def purple_halo_mask_array(array: Any) -> Any:
    red = array[..., 0].astype(np.int16)
    green = array[..., 1].astype(np.int16)
    blue = array[..., 2].astype(np.int16)
    purple = (red > 75) & (blue > 75) & (green < 120) & (np.minimum(red, blue) - green > 20) & (red + blue > green * 2 + 80)
    dark_purple = (
        (red >= 32)
        & (blue >= 32)
        & ((green < np.minimum(red, blue) * 0.55) | (green <= 12))
        & (np.abs(red - blue) < 64)
        & (red + blue > green * 3 + 38)
    )
    magenta = (red > 80) & (blue > 45) & (green < 120) & (red > green + 32) & (blue > green + 6)
    dark_magenta = (red > 44) & (blue > 34) & (green < 42) & (red > green + 24) & (blue > green + 14) & (red + blue > green * 2 + 48)
    return purple | dark_purple | magenta | dark_magenta


def green_screen_spill_mask_array(array: Any) -> Any:
    red = array[..., 0].astype(np.int16)
    green = array[..., 1].astype(np.int16)
    blue = array[..., 2].astype(np.int16)
    return (green > 100) & (green > red * 1.35) & (green > blue * 1.35) & (green - np.maximum(red, blue) > 28)


def source_key_spill_mask_array(array: Any, key: tuple[int, int, int]) -> Any:
    red = array[..., 0].astype(np.int16)
    green = array[..., 1].astype(np.int16)
    blue = array[..., 2].astype(np.int16)
    key_red, key_green, key_blue = key
    if key_green > 220 and key_red < 40 and key_blue < 40:
        return (green > 90) & (green > red * 1.25) & (green > blue * 1.25) & (green - np.maximum(red, blue) > 22)
    if key_red > 220 and key_blue > 220 and key_green < 40:
        return np.maximum.reduce((np.abs(red - key_red), np.abs(green - key_green), np.abs(blue - key_blue))) <= 36
    if key_red > 220 and key_green < 40 and key_blue < 40:
        return (red > 90) & (red > green * 1.25) & (red > blue * 1.25) & (red - np.maximum(green, blue) > 22)
    if key_blue > 220 and key_red < 40 and key_green < 40:
        return (blue > 90) & (blue > red * 1.25) & (blue > green * 1.25) & (blue - np.maximum(red, green) > 22)
    return np.maximum.reduce((np.abs(red - key_red), np.abs(green - key_green), np.abs(blue - key_blue))) <= 36


def count_edge_color_numpy(image: Image.Image, mask_fn: Any, radius: int) -> int | None:
    if np is None:
        return None
    array = np.asarray(image.convert("RGBA"), dtype=np.uint8)
    alpha = array[..., 3]
    visible = alpha > 12
    transparent_near = dilate_mask_numpy(alpha <= 12, radius)
    return int(np.count_nonzero(visible & mask_fn(array) & transparent_near))


def count_edge_color(image: Image.Image, predicate: Any, radius: int) -> int:
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    alpha = rgba.getchannel("A")
    alpha_pixels = alpha.load()
    width, height = rgba.size
    count = 0
    for y in range(height):
        for x in range(width):
            red, green, blue, current_alpha = pixels[x, y]
            if current_alpha <= 12 or not predicate(red, green, blue):
                continue
            if touches_transparent(alpha_pixels, x, y, width, height, radius):
                count += 1
    return count


def count_edge_key_fringe(image: Image.Image) -> int:
    vector_count = count_edge_color_numpy(image, key_fringe_mask_array, 1)
    if vector_count is not None:
        return vector_count
    return count_edge_color(image, is_key_fringe_like, 1)


def count_edge_source_key_fringe(image: Image.Image, key: tuple[int, int, int] | None) -> int:
    if key is None:
        return 0
    vector_count = count_edge_color_numpy(image, lambda array: source_key_spill_mask_array(array, key), 2)
    if vector_count is not None:
        return vector_count
    return count_edge_color(image, lambda red, green, blue: is_source_key_spill_like(red, green, blue, key), 2)


def count_edge_purple_halo(image: Image.Image) -> int:
    vector_count = count_edge_color_numpy(image, purple_halo_mask_array, 6)
    if vector_count is not None:
        return vector_count
    return count_edge_color(image, is_any_purple_halo_like, 6)


def count_edge_green_spill(image: Image.Image) -> int:
    vector_count = count_edge_color_numpy(image, green_screen_spill_mask_array, 2)
    if vector_count is not None:
        return vector_count
    return count_edge_color(image, is_green_screen_spill_like, 2)


def count_transparent_edge_bad_rgb(
    image: Image.Image,
    key: tuple[int, int, int] | None = None,
    *,
    preserve_purple: bool = False,
    preserve_green: bool = False,
    preserve_source_key: bool = False,
) -> int:
    if np is not None:
        array = np.asarray(image.convert("RGBA"), dtype=np.uint8)
        alpha = array[..., 3]
        transparent = alpha <= 12
        bad = np.zeros(alpha.shape, dtype=bool)
        if not preserve_purple:
            bad |= key_fringe_mask_array(array) | purple_halo_mask_array(array)
        if not preserve_green:
            bad |= green_screen_spill_mask_array(array)
        if key is not None and not preserve_source_key:
            bad |= source_key_spill_mask_array(array, key)
        near_visible = dilate_mask_numpy(alpha > 12, 2)
        return int(np.count_nonzero(transparent & bad & near_visible))
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    alpha = rgba.getchannel("A")
    alpha_pixels = alpha.load()
    width, height = rgba.size
    count = 0
    for y in range(height):
        for x in range(width):
            red, green, blue, current_alpha = pixels[x, y]
            if current_alpha > 12:
                continue
            source_key_bad = key is not None and not preserve_source_key and is_source_key_spill_like(red, green, blue, key)
            green_bad = not preserve_green and is_green_screen_spill_like(red, green, blue)
            purple_bad = not preserve_purple and (is_key_fringe_like(red, green, blue) or is_any_purple_halo_like(red, green, blue))
            if not (
                source_key_bad
                or green_bad
                or purple_bad
            ):
                continue
            near_visible = False
            for ny in range(max(0, y - 2), min(height, y + 3)):
                for nx in range(max(0, x - 2), min(width, x + 3)):
                    if alpha_pixels[nx, ny] > 12:
                        near_visible = True
                        break
                if near_visible:
                    break
            if near_visible:
                count += 1
    return count


def parse_hex_color(value: Any) -> tuple[int, int, int] | None:
    if not isinstance(value, str):
        return None
    text = value.strip()
    if text.startswith("#"):
        text = text[1:]
    if len(text) != 6:
        return None
    try:
        return (int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16))
    except ValueError:
        return None


def crop_entries(manifest: dict[str, Any]) -> list[dict[str, Any]]:
    entries: list[dict[str, Any]] = []
    for source in manifest.get("sources", []):
        source_path = source.get("path", "")
        for crop in source.get("crops", []):
            item = dict(crop)
            item["source"] = source_path
            entries.append(item)
    return entries


def audit_asset(crop: dict[str, Any], root: Path, source_key: tuple[int, int, int] | None, *, profile: bool = False) -> dict[str, Any]:
    started = perf_counter()
    phase_start = started
    timings: dict[str, float] = {}

    def mark_timing(name: str) -> None:
        nonlocal phase_start
        if not profile:
            return
        now = perf_counter()
        timings[name] = round((now - phase_start) * 1000, 3)
        phase_start = now

    output = crop.get("output")
    result: dict[str, Any] = {
        "id": crop.get("id", ""),
        "kind": crop.get("kind", ""),
        "output": output,
        "problems": [],
    }
    if not isinstance(output, str) or not output:
        result["problems"].append("missing output path")
        return result

    path = (root / output).resolve()
    if not path.exists():
        result["problems"].append(f"missing output file: {output}")
        return result

    image = Image.open(path).convert("RGBA")
    mark_timing("load_image")
    result["size"] = [image.width, image.height]
    bbox = alpha_bbox(image)
    mark_timing("alpha_bbox")
    if bbox is None:
        result["problems"].append("output has no visible alpha content")
        if profile:
            timings["total"] = round((perf_counter() - started) * 1000, 3)
            result["timing_ms"] = timings
        return result

    left, top, right, bottom = bbox
    padding = {
        "left": left,
        "top": top,
        "right": image.width - right,
        "bottom": image.height - bottom,
    }
    result["alpha_bbox"] = [left, top, right, bottom]
    result["edge_padding"] = padding

    if crop.get("kind") == "icon":
        expected_padding = crop.get("trim_padding")
        min_padding = crop.get("min_output_padding")
        if not isinstance(min_padding, int):
            min_padding = 4 if isinstance(expected_padding, int) and expected_padding >= 4 else 2
        for side, value in padding.items():
            if value < min_padding:
                result["problems"].append(f"icon alpha content too close to {side} edge: {value}px < {min_padding}px")
    elif crop.get("kind") == "slice9":
        min_padding_spec = crop.get("min_edge_padding")
        if isinstance(min_padding_spec, dict):
            min_padding_by_side = {side: int(min_padding_spec.get(side, 0)) for side in padding}
        else:
            min_padding = min_padding_spec if isinstance(min_padding_spec, int) else 6
            min_padding_by_side = {side: int(min_padding) for side in padding}
        for side, value in padding.items():
            min_padding = min_padding_by_side[side]
            if value < min_padding:
                result["problems"].append(f"slice9 alpha content too close to {side} edge: {value}px < {min_padding}px")
    mark_timing("padding_policy")

    preserve_purple = bool(crop.get("preserve_purple_edges"))
    preserve_green = bool(crop.get("preserve_green_edges"))
    preserve_source_key = bool(crop.get("preserve_source_key_edges"))
    fringe_count = 0 if preserve_purple else count_edge_key_fringe(image)
    mark_timing("edge_key_fringe")
    result["edge_key_fringe_pixels"] = fringe_count
    fringe_limit = crop.get("edge_key_fringe_limit", 0)
    if isinstance(fringe_limit, int) and fringe_count > fringe_limit:
        result["problems"].append(f"key-color edge fringe remains: {fringe_count}px > {fringe_limit}px")

    purple_halo_count = 0 if preserve_purple else count_edge_purple_halo(image)
    mark_timing("edge_purple_halo")
    result["edge_purple_halo_pixels"] = purple_halo_count
    purple_halo_limit = crop.get("edge_purple_halo_limit", 0)
    if isinstance(purple_halo_limit, int) and purple_halo_count > purple_halo_limit:
        result["problems"].append(f"purple edge halo remains: {purple_halo_count}px > {purple_halo_limit}px")

    green_spill_count = 0 if preserve_green else count_edge_green_spill(image)
    mark_timing("edge_green_spill")
    result["edge_green_spill_pixels"] = green_spill_count
    green_spill_limit = crop.get("edge_green_spill_limit", 0)
    if isinstance(green_spill_limit, int) and green_spill_count > green_spill_limit:
        result["problems"].append(f"green-screen edge spill remains: {green_spill_count}px > {green_spill_limit}px")

    source_key_fringe_count = 0 if preserve_source_key else count_edge_source_key_fringe(image, source_key)
    mark_timing("edge_source_key_fringe")
    result["edge_source_key_fringe_pixels"] = source_key_fringe_count
    source_key_fringe_limit = crop.get("edge_source_key_fringe_limit", 0)
    if isinstance(source_key_fringe_limit, int) and source_key_fringe_count > source_key_fringe_limit:
        result["problems"].append(
            f"source key edge fringe remains: {source_key_fringe_count}px > {source_key_fringe_limit}px"
        )

    transparent_bad_rgb_count = count_transparent_edge_bad_rgb(
        image,
        source_key,
        preserve_purple=preserve_purple,
        preserve_green=preserve_green,
        preserve_source_key=preserve_source_key,
    )
    mark_timing("transparent_edge_bad_rgb")
    result["transparent_edge_bad_rgb_pixels"] = transparent_bad_rgb_count
    transparent_bad_rgb_limit = crop.get("transparent_edge_bad_rgb_limit", 0)
    if isinstance(transparent_bad_rgb_limit, int) and transparent_bad_rgb_count > transparent_bad_rgb_limit:
        result["problems"].append(
            f"transparent edge keeps key/purple/green RGB that can bleed during filtering: "
            f"{transparent_bad_rgb_count}px > {transparent_bad_rgb_limit}px"
        )

    if profile:
        timings["total"] = round((perf_counter() - started) * 1000, 3)
        result["timing_ms"] = timings
    return result


def render_markdown(report: dict[str, Any]) -> str:
    lines = [
        "---",
        "type: GeneratedUIAssetAudit",
        f"crop_manifest: {report['crop_manifest']}",
        f"verdict: {report['verdict']}",
        "---",
        "",
        "# Generated UI Asset Audit",
        "",
        f"Verdict: **{report['verdict'].upper()}**",
        "",
        f"Assets checked: {report['assets_checked']}",
        f"Problems: {len(report['problems'])}",
        "",
    ]
    if report.get("timing_ms"):
        lines.extend(
            [
                "## Timing",
                "",
                f"Total: {report['timing_ms'].get('total', '-')} ms",
                "",
            ]
        )
        for asset in sorted(report["assets"], key=lambda item: item.get("timing_ms", {}).get("total", 0), reverse=True):
            timing = asset.get("timing_ms")
            if timing:
                lines.append(f"- `{asset.get('id')}` total={timing.get('total', '-')} ms")
        lines.append("")
    if report["problems"]:
        lines.extend(["## Problems", ""])
        for problem in report["problems"]:
            lines.append(f"- {problem}")
        lines.append("")
    lines.extend(["## Asset Summary", ""])
    for asset in report["assets"]:
        padding = asset.get("edge_padding", {})
        status = "FAIL" if asset.get("problems") else "PASS"
        lines.append(
            f"- {status} `{asset.get('id')}` ({asset.get('kind')}): "
            f"size={asset.get('size', '-')}, padding={padding}, "
            f"fringe={asset.get('edge_key_fringe_pixels', '-')}, "
            f"source_key_fringe={asset.get('edge_source_key_fringe_pixels', '-')}, "
            f"green_spill={asset.get('edge_green_spill_pixels', '-')}, "
            f"purple_halo={asset.get('edge_purple_halo_pixels', '-')}, "
            f"transparent_bad_rgb={asset.get('transparent_edge_bad_rgb_pixels', '-')}"
        )
    lines.append("")
    return "\n".join(lines)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Audit generated UI runtime PNGs for clipping and chroma-key fringe.")
    parser.add_argument("--crop-manifest", required=True)
    parser.add_argument("--json-output")
    parser.add_argument("--report")
    parser.add_argument("--profile", action="store_true", help="Record per-asset timing in JSON/Markdown and print the slowest asset.")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    started = perf_counter()
    manifest_path = project_path(args.crop_manifest)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    source_key = parse_hex_color(manifest.get("green_screen", {}).get("key"))
    assets = [audit_asset(crop, ROOT, source_key, profile=args.profile) for crop in crop_entries(manifest)]
    problems = [f"{asset['id']}: {problem}" for asset in assets for problem in asset.get("problems", [])]
    report = {
        "schema": "game.generated_ui_asset_audit",
        "version": 1,
        "crop_manifest": args.crop_manifest.replace("\\", "/"),
        "verdict": "fail" if problems else "pass",
        "assets_checked": len(assets),
        "problems": problems,
        "assets": assets,
    }
    if args.profile:
        report["timing_ms"] = {
            "total": round((perf_counter() - started) * 1000, 3),
            "assets_total": round(sum(asset.get("timing_ms", {}).get("total", 0) for asset in assets), 3),
        }
    if args.json_output:
        json_path = project_path(args.json_output)
        json_path.parent.mkdir(parents=True, exist_ok=True)
        json_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if args.report:
        report_path = project_path(args.report)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(render_markdown(report), encoding="utf-8")

    print(f"{report['verdict']}: checked {len(assets)} generated UI asset(s)")
    if args.profile and assets:
        slowest = max(assets, key=lambda asset: asset.get("timing_ms", {}).get("total", 0))
        print(f"profile: slowest asset `{slowest.get('id')}` {slowest.get('timing_ms', {}).get('total', 0)} ms")
    for problem in problems:
        print(f"problem: {problem}")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
