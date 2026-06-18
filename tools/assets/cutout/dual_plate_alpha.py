#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from collections import deque
from pathlib import Path
from time import perf_counter
from typing import Literal

import numpy as np
from PIL import Image

import sys

ROOT = Path(__file__).resolve().parents[3]  # cutout -> assets -> tools -> repo root
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.assets.atomic_io import save_image_atomic, write_json_atomic, write_text_atomic
from tools.assets.cutout.dual_plate_pair_gate import evaluate as evaluate_pair

RGB = tuple[int, int, int]
# "proj" = Smith & Blinn (1996) Theorem-4 joint-channel projection: the
# least-squares (1-alpha) using all channels at once. "min"/"max"/"avg" are the
# legacy per-channel solve + reconcile and are kept for backward compatibility.
AlphaCombine = Literal["min", "max", "avg", "proj"]
RecoverySource = Literal["dark", "light", "average"]


def analysis_engine() -> str:
    return "numpy"


def parse_color(value: str) -> RGB:
    text = value.strip().lstrip("#")
    if len(text) != 6:
        raise argparse.ArgumentTypeError("color must be #rrggbb")
    try:
        return tuple(int(text[index : index + 2], 16) for index in (0, 2, 4))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("color must be #rrggbb") from exc


def extract_dual_plate_alpha(
    light: Image.Image,
    dark: Image.Image,
    *,
    bg_light: RGB = (255, 255, 255),
    bg_dark: RGB = (0, 0, 0),
    alpha_combine: AlphaCombine = "proj",
    recovery_source: RecoverySource = "average",
    alpha_cutoff: int = 0,
    alpha_hardening: int = 0,
) -> Image.Image:
    light_rgba = light.convert("RGBA")
    dark_rgba = dark.convert("RGBA")
    if light_rgba.size != dark_rgba.size:
        raise ValueError(f"plate dimensions differ: light={light_rgba.size}, dark={dark_rgba.size}")

    bg_diff = [bg_light[index] - bg_dark[index] for index in range(3)]
    usable_channels = [index for index, diff in enumerate(bg_diff) if abs(diff) >= 8]
    if not usable_channels:
        raise ValueError("bg-light and bg-dark must differ by at least 8 in one channel")

    return extract_dual_plate_alpha_numpy(
        light_rgba,
        dark_rgba,
        bg_light=bg_light,
        bg_dark=bg_dark,
        bg_diff=bg_diff,
        usable_channels=usable_channels,
        alpha_combine=alpha_combine,
        recovery_source=recovery_source,
        alpha_cutoff=alpha_cutoff,
        alpha_hardening=alpha_hardening,
    )


def extract_dual_plate_alpha_numpy(
    light_rgba: Image.Image,
    dark_rgba: Image.Image,
    *,
    bg_light: RGB,
    bg_dark: RGB,
    bg_diff: list[int],
    usable_channels: list[int],
    alpha_combine: AlphaCombine,
    recovery_source: RecoverySource,
    alpha_cutoff: int,
    alpha_hardening: int,
) -> Image.Image:
    light_array = np.asarray(light_rgba, dtype=np.float32)
    dark_array = np.asarray(dark_rgba, dtype=np.float32)

    if alpha_combine == "proj":
        bg_diff_full = np.asarray(bg_diff, dtype=np.float32)
        proj_denom = float(np.dot(bg_diff_full, bg_diff_full)) or 1.0
        observed_full = light_array[..., :3] - dark_array[..., :3]
        projected = np.tensordot(observed_full, bg_diff_full, axes=([-1], [0])) / proj_denom
        alpha = np.clip(1.0 - projected, 0.0, 1.0)
    else:
        bg_diff_array = np.asarray([bg_diff[channel] for channel in usable_channels], dtype=np.float32)
        observed_diff = light_array[..., usable_channels] - dark_array[..., usable_channels]
        alpha_values = np.clip(1.0 - (observed_diff / bg_diff_array), 0.0, 1.0)
        if alpha_combine == "min":
            alpha = np.min(alpha_values, axis=-1)
        elif alpha_combine == "max":
            alpha = np.max(alpha_values, axis=-1)
        else:
            alpha = np.mean(alpha_values, axis=-1)
    alpha_byte = np.rint(alpha * 255.0).clip(0, 255).astype(np.uint8)

    visible_mask = alpha_byte > alpha_cutoff
    if alpha_hardening > 0:
        visible_mask &= alpha_byte >= alpha_hardening

    bg_dark_array = np.asarray(bg_dark, dtype=np.float32)
    bg_light_array = np.asarray(bg_light, dtype=np.float32)
    alpha_safe = np.where(alpha > 0.0001, alpha, 1.0)[..., None]
    dark_fg = (dark_array[..., :3] - (1.0 - alpha[..., None]) * bg_dark_array) / alpha_safe
    light_fg = (light_array[..., :3] - (1.0 - alpha[..., None]) * bg_light_array) / alpha_safe
    if recovery_source == "dark":
        recovered = dark_fg
    elif recovery_source == "light":
        recovered = light_fg
    else:
        recovered = (dark_fg + light_fg) * 0.5

    output = np.zeros((*alpha.shape, 4), dtype=np.uint8)
    output[..., :3] = np.rint(recovered).clip(0, 255).astype(np.uint8)
    output[..., 3] = alpha_byte
    output[~visible_mask] = 0
    return Image.fromarray(output, "RGBA")


def cleanup_alpha_blobs(
    image: Image.Image,
    *,
    min_area: int = 0,
    keep_largest: bool = False,
    alpha_threshold: int = 1,
) -> int:
    if min_area <= 0 and not keep_largest:
        return 0
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    width, height = rgba.size
    visited = bytearray(width * height)
    components: list[list[tuple[int, int]]] = []

    def offset(x: int, y: int) -> int:
        return y * width + x

    for y in range(height):
        for x in range(width):
            index = offset(x, y)
            if visited[index] or pixels[x, y][3] < alpha_threshold:
                visited[index] = 1
                continue
            queue: deque[tuple[int, int]] = deque([(x, y)])
            visited[index] = 1
            component: list[tuple[int, int]] = []
            while queue:
                px, py = queue.popleft()
                component.append((px, py))
                for nx, ny in ((px + 1, py), (px - 1, py), (px, py + 1), (px, py - 1)):
                    if not (0 <= nx < width and 0 <= ny < height):
                        continue
                    next_index = offset(nx, ny)
                    if visited[next_index]:
                        continue
                    visited[next_index] = 1
                    if pixels[nx, ny][3] >= alpha_threshold:
                        queue.append((nx, ny))
            components.append(component)

    if not components:
        image.paste(rgba)
        return 0

    largest = max(components, key=len)
    removed = 0
    for component in components:
        should_remove = min_area > 0 and len(component) < min_area
        if keep_largest and component is not largest:
            should_remove = True
        if not should_remove:
            continue
        for x, y in component:
            pixels[x, y] = (0, 0, 0, 0)
            removed += 1
    image.paste(rgba)
    return removed


def count_transparent_nonzero_rgb(image: Image.Image) -> int:
    return int(report_image_stats(image)["transparent_nonzero_rgb_pixels"])


def alpha_bbox(image: Image.Image) -> list[int] | None:
    bbox = image.convert("RGBA").getchannel("A").getbbox()
    if not bbox:
        return None
    left, top, right, bottom = bbox
    return [left, top, right - left, bottom - top]


def report_image_stats(image: Image.Image) -> dict[str, int | float | list[int] | None]:
    rgba = image.convert("RGBA")
    return report_image_stats_numpy(rgba)


def report_image_stats_numpy(rgba: Image.Image) -> dict[str, int | float | list[int] | None]:
    pixels = np.asarray(rgba, dtype=np.uint8)
    alpha = pixels[..., 3]
    visible_mask = alpha > 0
    visible_pixels = int(np.count_nonzero(visible_mask))
    transparent_pixels = int(alpha.size - visible_pixels)
    hidden_rgb_pixels = int(np.count_nonzero((alpha == 0) & np.any(pixels[..., :3] != 0, axis=-1)))
    alpha_bbox_value: list[int] | None = None
    if visible_pixels:
        ys, xs = np.nonzero(visible_mask)
        left = int(xs.min())
        top = int(ys.min())
        right = int(xs.max()) + 1
        bottom = int(ys.max()) + 1
        alpha_bbox_value = [left, top, right - left, bottom - top]
        visible_alpha = alpha[visible_mask]
        min_visible_alpha = int(visible_alpha.min())
        max_visible_alpha = int(visible_alpha.max())
        mean_visible_alpha = round(float(visible_alpha.mean()), 3)
    else:
        min_visible_alpha = 0
        max_visible_alpha = 0
        mean_visible_alpha = 0
    return {
        "alpha_bbox": alpha_bbox_value,
        "visible_pixels": visible_pixels,
        "transparent_pixels": transparent_pixels,
        "transparent_nonzero_rgb_pixels": hidden_rgb_pixels,
        "min_visible_alpha": min_visible_alpha,
        "max_visible_alpha": max_visible_alpha,
        "mean_visible_alpha": mean_visible_alpha,
    }


def build_report(image: Image.Image, *, removed_blob_pixels: int, timings: dict[str, float] | None = None, engine: str | None = None) -> dict:
    rgba = image.convert("RGBA")
    stats = report_image_stats(rgba)
    problems: list[str] = []
    if not stats["visible_pixels"]:
        problems.append("extraction produced no visible alpha pixels")
    if stats["transparent_nonzero_rgb_pixels"] > 0:
        problems.append(f"transparent pixels retain non-zero RGB: {stats['transparent_nonzero_rgb_pixels']}")
    report = {
        "schema": "game.dual_plate_alpha_report",
        "version": 1,
        "analysis_engine": engine or analysis_engine(),
        "verdict": "pass" if not problems else "fail",
        "status": "pass" if not problems else "fail",
        "problems": problems,
        "size": list(rgba.size),
        **stats,
        "removed_blob_pixels": removed_blob_pixels,
    }
    if timings:
        report["timing_ms"] = timings
    return report


def write_markdown_report(path: Path, report: dict) -> None:
    write_text_atomic(
        path,
        "\n".join(
            [
                "---",
                "type: DualPlateAlphaReport",
                f"verdict: {report['verdict']}",
                f"visible_pixels: {report['visible_pixels']}",
                "---",
                "",
                "# Dual Plate Alpha Report",
                "",
                f"Verdict: **{report['verdict']}**",
                f"Analysis engine: `{report['analysis_engine']}`",
                f"Size: `{report['size'][0]}x{report['size'][1]}`",
                f"Alpha bbox: `{report['alpha_bbox']}`",
                f"Visible pixels: `{report['visible_pixels']}`",
                f"Transparent pixels: `{report['transparent_pixels']}`",
                f"Transparent non-zero RGB pixels: `{report['transparent_nonzero_rgb_pixels']}`",
                f"Visible alpha range: `{report['min_visible_alpha']}..{report['max_visible_alpha']}`",
                f"Mean visible alpha: `{report['mean_visible_alpha']}`",
                f"Removed blob pixels: `{report['removed_blob_pixels']}`",
                *(["", "## Problems", *[f"- {problem}" for problem in report["problems"]]] if report.get("problems") else []),
                *(["", "## Timing", *[f"- {name}: {elapsed} ms" for name, elapsed in report["timing_ms"].items()]] if report.get("timing_ms") else []),
                "",
            ]
        )
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract RGBA PNG from aligned light/dark background plates.")
    parser.add_argument("--light", type=Path, required=True)
    parser.add_argument("--dark", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--bg-light", type=parse_color, default=parse_color("#ffffff"))
    parser.add_argument("--bg-dark", type=parse_color, default=parse_color("#000000"))
    parser.add_argument("--alpha-combine", choices=["min", "max", "avg", "proj"], default="proj")
    parser.add_argument("--recovery-source", choices=["dark", "light", "average"], default="average")
    parser.add_argument("--alpha-cutoff", type=int, default=0)
    parser.add_argument("--alpha-hardening", type=int, default=0)
    parser.add_argument("--blob-min-area", type=int, default=0)
    parser.add_argument("--keep-largest-blob", action="store_true")
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--no-fail", action="store_true", help="Write diagnostic output even when the extraction report verdict is fail.")
    parser.add_argument("--skip-pair-gate", action="store_true", help="Do not run the white/black pair consistency gate (allow matteing a misaligned/redrawn pair).")
    parser.add_argument("--profile", action="store_true", help="Record extraction, cleanup, save, and report timings.")
    args = parser.parse_args()

    light_image = Image.open(args.light).convert("RGBA")
    dark_image = Image.open(args.dark).convert("RGBA")
    if dark_image.size != light_image.size:
        # Independent AI generations differ by a few px; align canvas to the light
        # plate. Real subject misalignment is still caught by the pair gate below.
        dark_image = dark_image.resize(light_image.size, Image.Resampling.LANCZOS)
    started = perf_counter()
    extract_started = perf_counter()
    result = extract_dual_plate_alpha(
        light_image,
        dark_image,
        bg_light=args.bg_light,
        bg_dark=args.bg_dark,
        alpha_combine=args.alpha_combine,
        recovery_source=args.recovery_source,
        alpha_cutoff=args.alpha_cutoff,
        alpha_hardening=args.alpha_hardening,
    )
    timings: dict[str, float] = {}
    if args.profile:
        timings["extract"] = round((perf_counter() - extract_started) * 1000, 3)
    cleanup_started = perf_counter()
    removed_blob_pixels = cleanup_alpha_blobs(
        result,
        min_area=args.blob_min_area,
        keep_largest=args.keep_largest_blob,
        alpha_threshold=max(1, args.alpha_cutoff),
    )
    if args.profile:
        timings["cleanup"] = round((perf_counter() - cleanup_started) * 1000, 3)
    save_started = perf_counter()
    save_image_atomic(result, args.output)
    if args.profile:
        timings["save_output"] = round((perf_counter() - save_started) * 1000, 3)

    report_started = perf_counter()
    report = build_report(result, removed_blob_pixels=removed_blob_pixels, engine=analysis_engine())
    report.update(
        {
            "light": str(args.light),
            "dark": str(args.dark),
            "output": str(args.output),
            "bg_light": list(args.bg_light),
            "bg_dark": list(args.bg_dark),
            "alpha_combine": args.alpha_combine,
            "recovery_source": args.recovery_source,
            "alpha_cutoff": args.alpha_cutoff,
            "alpha_hardening": args.alpha_hardening,
            "blob_min_area": args.blob_min_area,
            "keep_largest_blob": args.keep_largest_blob,
        }
    )
    # Pipeline gate: a misaligned/redrawn white/black pair produces ghosted alpha,
    # so refuse it here (verdict fail -> exit 1) instead of letting a bad pair
    # become a runtime asset. Bypass only with --skip-pair-gate.
    if not args.skip_pair_gate:
        pair = evaluate_pair(light_image.convert("RGBA"), dark_image.convert("RGBA"))
        report["pair_gate"] = {key: pair[key] for key in ("verdict", "inconsistent_fraction", "mean_edge_chroma")}
        if pair["verdict"] == "regenerate":
            report["problems"].append(
                f"dual-plate pair failed the consistency gate (inconsistent_fraction={pair['inconsistent_fraction']}); "
                "regenerate the pair (chain: black = edit of the white plate) instead of matteing it"
            )
            report["verdict"] = "fail"
            report["status"] = "fail"
    if args.profile:
        timings["build_report"] = round((perf_counter() - report_started) * 1000, 3)
        timings["total"] = round((perf_counter() - started) * 1000, 3)
        report["timing_ms"] = timings
    if args.json_output:
        write_json_atomic(args.json_output, report, trailing_newline=False)
    if args.report:
        write_markdown_report(args.report, report)
    print(f"{report['verdict']}: wrote {args.output} ({report['visible_pixels']} visible pixels)")
    if args.profile:
        print(f"profile: dual-plate alpha total {report['timing_ms']['total']} ms")
    if report["verdict"] != "pass" and not args.no_fail:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
