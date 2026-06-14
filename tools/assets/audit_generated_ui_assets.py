#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from PIL import Image


ROOT = Path.cwd()
SCRIPT_ROOT = Path(__file__).resolve().parents[2]
if str(SCRIPT_ROOT) not in sys.path:
    sys.path.insert(0, str(SCRIPT_ROOT))

from tools.assets.chroma_key_alpha import is_any_purple_halo_like, is_exact_key_like, is_key_fringe_like, is_source_key_spill_like


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
    return count_edge_color(image, is_key_fringe_like, 1)


def count_edge_source_key_fringe(image: Image.Image, key: tuple[int, int, int] | None) -> int:
    if key is None:
        return 0
    return count_edge_color(image, lambda red, green, blue: is_source_key_spill_like(red, green, blue, key), 2)


def count_edge_purple_halo(image: Image.Image) -> int:
    return count_edge_color(image, is_any_purple_halo_like, 6)


def count_transparent_edge_bad_rgb(image: Image.Image, key: tuple[int, int, int] | None = None) -> int:
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
            source_key_bad = key is not None and is_source_key_spill_like(red, green, blue, key)
            if not (source_key_bad or is_key_fringe_like(red, green, blue) or is_any_purple_halo_like(red, green, blue)):
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


def audit_asset(crop: dict[str, Any], root: Path, source_key: tuple[int, int, int] | None) -> dict[str, Any]:
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
    result["size"] = [image.width, image.height]
    bbox = alpha_bbox(image)
    if bbox is None:
        result["problems"].append("output has no visible alpha content")
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

    preserve_purple = bool(crop.get("preserve_purple_edges"))
    fringe_count = 0 if preserve_purple else count_edge_key_fringe(image)
    result["edge_key_fringe_pixels"] = fringe_count
    fringe_limit = crop.get("edge_key_fringe_limit", 0)
    if isinstance(fringe_limit, int) and fringe_count > fringe_limit:
        result["problems"].append(f"key-color edge fringe remains: {fringe_count}px > {fringe_limit}px")

    purple_halo_count = 0 if preserve_purple else count_edge_purple_halo(image)
    result["edge_purple_halo_pixels"] = purple_halo_count
    purple_halo_limit = crop.get("edge_purple_halo_limit", 0)
    if isinstance(purple_halo_limit, int) and purple_halo_count > purple_halo_limit:
        result["problems"].append(f"purple edge halo remains: {purple_halo_count}px > {purple_halo_limit}px")

    source_key_fringe_count = 0 if preserve_purple else count_edge_source_key_fringe(image, source_key)
    result["edge_source_key_fringe_pixels"] = source_key_fringe_count
    source_key_fringe_limit = crop.get("edge_source_key_fringe_limit", 0)
    if isinstance(source_key_fringe_limit, int) and source_key_fringe_count > source_key_fringe_limit:
        result["problems"].append(
            f"source key edge fringe remains: {source_key_fringe_count}px > {source_key_fringe_limit}px"
        )

    transparent_bad_rgb_count = 0 if preserve_purple else count_transparent_edge_bad_rgb(image, source_key)
    result["transparent_edge_bad_rgb_pixels"] = transparent_bad_rgb_count
    transparent_bad_rgb_limit = crop.get("transparent_edge_bad_rgb_limit", 0)
    if isinstance(transparent_bad_rgb_limit, int) and transparent_bad_rgb_count > transparent_bad_rgb_limit:
        result["problems"].append(
            f"transparent edge keeps key/purple RGB that can bleed during filtering: "
            f"{transparent_bad_rgb_count}px > {transparent_bad_rgb_limit}px"
        )

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
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    manifest_path = project_path(args.crop_manifest)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    source_key = parse_hex_color(manifest.get("green_screen", {}).get("key"))
    assets = [audit_asset(crop, ROOT, source_key) for crop in crop_entries(manifest)]
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
    if args.json_output:
        json_path = project_path(args.json_output)
        json_path.parent.mkdir(parents=True, exist_ok=True)
        json_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if args.report:
        report_path = project_path(args.report)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(render_markdown(report), encoding="utf-8")

    print(f"{report['verdict']}: checked {len(assets)} generated UI asset(s)")
    for problem in problems:
        print(f"problem: {problem}")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
