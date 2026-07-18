#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

from ai_studio.assets.tools.image.alpha_matte.chroma_key_alpha import (
    source_key_spill_mask,
)
from ai_studio.assets.tools.lib.atomic_io import save_image_atomic, write_json_atomic


RGB = tuple[int, int, int]
VISIBLE_ALPHA = 12
EDGE_RADIUS = 2
THRESHOLD_KEYS = {
    "max_spill_edge_ratio",
    "max_halo_edge_ratio",
    "max_alpha_noise_ratio",
    "max_empty_margin_ratio",
    "aspect_ratio",
}


def parse_color(value: str) -> RGB:
    text = value.strip().lstrip("#")
    if len(text) != 6:
        raise argparse.ArgumentTypeError("color must be #RRGGBB")
    try:
        return tuple(int(text[index : index + 2], 16) for index in (0, 2, 4))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("color must be #RRGGBB") from exc


def ratio(value: Any, label: str, *, nullable: bool = False) -> float | None:
    if value is None and nullable:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not np.isfinite(value) or value < 0 or value > 1:
        raise ValueError(f"{label} must be a ratio in 0..1{(' or null' if nullable else '')}")
    return float(value)


def validate_thresholds(value: Any, *, keyed: bool) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != THRESHOLD_KEYS:
        raise ValueError("technical gate thresholds have unexpected or missing fields")
    if keyed and (value["max_spill_edge_ratio"] is None or value["max_halo_edge_ratio"] is None):
        raise ValueError("keyed gates require spill and halo thresholds")
    if not keyed and (value["max_spill_edge_ratio"] is not None or value["max_halo_edge_ratio"] is not None):
        raise ValueError("transparent gates require null spill and halo thresholds")
    spill = ratio(value["max_spill_edge_ratio"], "max_spill_edge_ratio", nullable=not keyed)
    halo = ratio(value["max_halo_edge_ratio"], "max_halo_edge_ratio", nullable=not keyed)
    alpha_noise = ratio(value["max_alpha_noise_ratio"], "max_alpha_noise_ratio")
    empty_margin = ratio(value["max_empty_margin_ratio"], "max_empty_margin_ratio")
    aspect = value["aspect_ratio"]
    if not isinstance(aspect, dict) or set(aspect) != {"width", "height", "max_relative_error"}:
        raise ValueError("aspect_ratio has unexpected or missing fields")
    width = aspect["width"]
    height = aspect["height"]
    if isinstance(width, bool) or not isinstance(width, int) or width < 1:
        raise ValueError("aspect_ratio.width must be a positive integer")
    if isinstance(height, bool) or not isinstance(height, int) or height < 1:
        raise ValueError("aspect_ratio.height must be a positive integer")
    aspect_error = ratio(aspect["max_relative_error"], "aspect_ratio.max_relative_error")
    return {
        "max_spill_edge_ratio": spill,
        "max_halo_edge_ratio": halo,
        "max_alpha_noise_ratio": alpha_noise,
        "max_empty_margin_ratio": empty_margin,
        "aspect_ratio": {"width": width, "height": height, "max_relative_error": aspect_error},
    }


def rounded(value: float) -> float:
    return round(float(value), 6)


def mask_bbox(mask: Any) -> list[int] | None:
    ys, xs = np.nonzero(mask)
    if not len(xs):
        return None
    left = int(xs.min())
    top = int(ys.min())
    return [left, top, int(xs.max()) - left + 1, int(ys.max()) - top + 1]


def key_halo_mask(red: Any, green: Any, blue: Any, key: RGB) -> Any:
    if key == (255, 0, 255):
        key_chroma = np.minimum(red, blue)
        return (key_chroma > 32) & (key_chroma - green > 12)
    if key == (0, 255, 0):
        other = np.maximum(red, blue)
        return (green > 32) & (green - other > 12)
    raise ValueError("quality gate key_color must be canonical magenta #FF00FF or green #00FF00")


def evaluate(image: Image.Image, *, key_color: RGB | None, thresholds: dict[str, Any]) -> dict[str, Any]:
    if key_color is not None and key_color not in {(255, 0, 255), (0, 255, 0)}:
        raise ValueError("quality gate key_color must be canonical magenta #FF00FF or green #00FF00")
    limits = validate_thresholds(thresholds, keyed=key_color is not None)
    rgba = image.convert("RGBA")
    array = np.asarray(rgba)
    height, width = array.shape[:2]
    alpha = array[..., 3]
    foreground = alpha > VISIBLE_ALPHA
    transparent = ~foreground
    visible_px = int(np.count_nonzero(foreground))
    transparent_px = int(np.count_nonzero(transparent))

    content_bbox = mask_bbox(foreground)
    if content_bbox is None:
        content_bbox = None
        empty_margin_ratio = 1.0
    else:
        _left, _top, bbox_width, bbox_height = content_bbox
        bbox_area = bbox_width * bbox_height
        empty_margin_ratio = 1.0 - bbox_area / max(1, width * height)

    edge_structure = np.ones((EDGE_RADIUS * 2 + 1, EDGE_RADIUS * 2 + 1), dtype=bool)
    foreground_edge = foreground & ndimage.binary_dilation(transparent, structure=edge_structure)
    edge_sample_px = int(np.count_nonzero(foreground_edge))
    spill_mask = np.zeros(foreground.shape, dtype=bool)
    halo_mask = np.zeros(foreground.shape, dtype=bool)
    if key_color is not None and edge_sample_px:
        red = array[..., 0].astype(np.int16)
        green = array[..., 1].astype(np.int16)
        blue = array[..., 2].astype(np.int16)
        spill_mask = foreground_edge & source_key_spill_mask(red, green, blue, key_color)
        halo_mask = foreground_edge & key_halo_mask(red, green, blue, key_color) & ~spill_mask

    spill_px = int(np.count_nonzero(spill_mask))
    halo_px = int(np.count_nonzero(halo_mask))
    spill_edge_ratio = spill_px / max(1, edge_sample_px)
    halo_edge_ratio = halo_px / max(1, edge_sample_px)

    neighborhood = np.ones((3, 3), dtype=np.uint8)
    foreground_neighbors = ndimage.convolve(foreground.astype(np.uint8), neighborhood, mode="constant", cval=0) - foreground
    alpha_transition = (
        (foreground & ndimage.binary_dilation(transparent, structure=neighborhood))
        | (transparent & ndimage.binary_dilation(foreground, structure=neighborhood))
    )
    isolated_foreground = foreground & (foreground_neighbors <= 1)
    pinhole_transparency = transparent & (foreground_neighbors >= 7)
    alpha_noise_mask = alpha_transition & (isolated_foreground | pinhole_transparency)
    alpha_transition_px = int(np.count_nonzero(alpha_transition))
    alpha_noise_px = int(np.count_nonzero(alpha_noise_mask))
    alpha_noise_ratio = alpha_noise_px / max(1, alpha_transition_px)

    expected_aspect = limits["aspect_ratio"]["width"] / limits["aspect_ratio"]["height"]
    actual_aspect = width / max(1, height)
    aspect_relative_error = abs(actual_aspect - expected_aspect) / expected_aspect

    metrics = {
        "size": [width, height],
        "content_bbox": content_bbox,
        "visible_px": visible_px,
        "transparent_px": transparent_px,
        "edge_sample_px": edge_sample_px,
        "spill_edge_px": spill_px,
        "spill_edge_ratio": rounded(spill_edge_ratio),
        "halo_edge_px": halo_px,
        "halo_edge_ratio": rounded(halo_edge_ratio),
        "alpha_transition_sample_px": alpha_transition_px,
        "alpha_noise_px": alpha_noise_px,
        "alpha_noise_ratio": rounded(alpha_noise_ratio),
        "empty_margin_ratio": rounded(empty_margin_ratio),
        "aspect_relative_error": rounded(aspect_relative_error),
    }
    problems: list[dict[str, Any]] = []

    def check(code: str, metric: str, maximum: float | None) -> None:
        if maximum is not None and float(metrics[metric]) > maximum:
            problems.append({"code": code, "metric": metric, "value": metrics[metric], "maximum": maximum})

    if visible_px == 0:
        problems.append({"code": "empty_asset", "metric": "visible_px", "value": 0, "minimum": 1})
    if transparent_px == 0:
        problems.append({"code": "no_transparency", "metric": "transparent_px", "value": 0, "minimum": 1})
    check("key_spill", "spill_edge_ratio", limits["max_spill_edge_ratio"])
    check("edge_halo", "halo_edge_ratio", limits["max_halo_edge_ratio"])
    check("alpha_noise", "alpha_noise_ratio", limits["max_alpha_noise_ratio"])
    check("empty_margin", "empty_margin_ratio", limits["max_empty_margin_ratio"])
    check("aspect_ratio", "aspect_relative_error", limits["aspect_ratio"]["max_relative_error"])

    local_problem_mask = spill_mask | halo_mask | alpha_noise_mask
    problem_bbox = mask_bbox(local_problem_mask)
    if problem_bbox is None and problems:
        problem_bbox = content_bbox or [0, 0, width, height]
    return {
        "schema": "game.asset_technical_gate",
        "version": 1,
        "verdict": "fail" if problems else "pass",
        "key_color": None if key_color is None else "#{:02X}{:02X}{:02X}".format(*key_color),
        "thresholds": limits,
        "metrics": metrics,
        "problems": problems,
        "problem_bbox": problem_bbox,
    }


def write_problem_thumbnail(image: Image.Image, report: dict[str, Any], output: Path, *, max_size: int = 192) -> None:
    rgba = image.convert("RGBA")
    box = report.get("problem_bbox") or [0, 0, rgba.width, rgba.height]
    left, top, width, height = (int(value) for value in box)
    padding = 4
    crop_box = (
        max(0, left - padding),
        max(0, top - padding),
        min(rgba.width, left + width + padding),
        min(rgba.height, top + height + padding),
    )
    crop = rgba.crop(crop_box)
    draw = ImageDraw.Draw(crop)
    local = (left - crop_box[0], top - crop_box[1], left - crop_box[0] + width - 1, top - crop_box[1] + height - 1)
    draw.rectangle(local, outline=(255, 48, 48, 255), width=max(1, min(crop.size) // 32))
    crop.thumbnail((max_size, max_size), Image.Resampling.NEAREST)
    save_image_atomic(crop, output, format="PNG")


def problem_summary(problem: dict[str, Any]) -> str:
    maximum = problem.get("maximum")
    if maximum is not None:
        return f"{problem['code']}={problem['value']}>{maximum}"
    return f"{problem['code']}={problem['value']}"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Fail-closed deterministic quality gate for prepared RGBA game assets.")
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--key-color", type=parse_color, help="Source chroma key (#RRGGBB); omit for native-transparent art.")
    parser.add_argument("--thresholds", type=Path, required=True)
    parser.add_argument("--json-output", type=Path, required=True)
    parser.add_argument("--problem-thumbnail", type=Path, required=True)
    args = parser.parse_args(argv)

    try:
        args.problem_thumbnail.unlink(missing_ok=True)
        thresholds = json.loads(args.thresholds.read_text(encoding="utf-8"))
        image = Image.open(args.source).convert("RGBA")
        report = evaluate(image, key_color=args.key_color, thresholds=thresholds)
    except (OSError, ValueError) as error:
        report = {
            "schema": "game.asset_technical_gate",
            "version": 1,
            "verdict": "fail",
            "source": str(args.source).replace("\\", "/"),
            "problems": [{"code": "invalid_input", "message": str(error)}],
        }
        try:
            write_json_atomic(args.json_output, report)
        except OSError:
            pass
        print(f"FAIL {args.source.name}: invalid_input={error}")
        return 2
    report["source"] = str(args.source).replace("\\", "/")
    try:
        write_json_atomic(args.json_output, report)
        if report["problems"]:
            write_problem_thumbnail(image, report, args.problem_thumbnail)
    except OSError as error:
        print(f"FAIL {args.source.name}: artifact_write={error}")
        return 2
    if report["problems"]:
        summary = ", ".join(problem_summary(problem) for problem in report["problems"])
        print(f"FAIL {args.source.name}: {summary}")
        return 1
    metrics = report["metrics"]
    print(
        f"PASS {args.source.name}: spill={metrics['spill_edge_ratio']} halo={metrics['halo_edge_ratio']} "
        f"alpha_noise={metrics['alpha_noise_ratio']} empty_margin={metrics['empty_margin_ratio']} "
        f"aspect_error={metrics['aspect_relative_error']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
