#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from collections import deque
from pathlib import Path
from statistics import mean
from typing import Literal

from PIL import Image


RGB = tuple[int, int, int]
AlphaCombine = Literal["min", "max", "avg"]
RecoverySource = Literal["dark", "light", "average"]


def parse_color(value: str) -> RGB:
    text = value.strip().lstrip("#")
    if len(text) != 6:
        raise argparse.ArgumentTypeError("color must be #rrggbb")
    try:
        return tuple(int(text[index : index + 2], 16) for index in (0, 2, 4))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("color must be #rrggbb") from exc


def clamp_byte(value: float) -> int:
    return max(0, min(255, int(round(value))))


def combine(values: list[float], mode: AlphaCombine) -> float:
    if not values:
        return 0.0
    if mode == "min":
        return min(values)
    if mode == "max":
        return max(values)
    return sum(values) / len(values)


def recover_channel(observed: int, alpha: float, background: int) -> float:
    if alpha <= 0.0001:
        return 0.0
    return (observed - (1.0 - alpha) * background) / alpha


def extract_dual_plate_alpha(
    light: Image.Image,
    dark: Image.Image,
    *,
    bg_light: RGB = (255, 255, 255),
    bg_dark: RGB = (0, 0, 0),
    alpha_combine: AlphaCombine = "min",
    recovery_source: RecoverySource = "dark",
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

    output = Image.new("RGBA", light_rgba.size, (0, 0, 0, 0))
    light_pixels = light_rgba.load()
    dark_pixels = dark_rgba.load()
    out_pixels = output.load()
    width, height = output.size

    for y in range(height):
        for x in range(width):
            light_px = light_pixels[x, y]
            dark_px = dark_pixels[x, y]
            alphas: list[float] = []
            for channel in usable_channels:
                observed_diff = light_px[channel] - dark_px[channel]
                alpha = 1.0 - (observed_diff / bg_diff[channel])
                alphas.append(max(0.0, min(1.0, alpha)))
            alpha = combine(alphas, alpha_combine)
            alpha_byte = clamp_byte(alpha * 255)
            if alpha_byte <= alpha_cutoff or (alpha_hardening > 0 and alpha_byte < alpha_hardening):
                out_pixels[x, y] = (0, 0, 0, 0)
                continue

            recovered: list[int] = []
            for channel in range(3):
                dark_fg = recover_channel(dark_px[channel], alpha, bg_dark[channel])
                light_fg = recover_channel(light_px[channel], alpha, bg_light[channel])
                if recovery_source == "dark":
                    value = dark_fg
                elif recovery_source == "light":
                    value = light_fg
                else:
                    value = (dark_fg + light_fg) * 0.5
                recovered.append(clamp_byte(value))
            out_pixels[x, y] = (recovered[0], recovered[1], recovered[2], alpha_byte)

    return output


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
            red, green, blue, _alpha = pixels[x, y]
            pixels[x, y] = (red, green, blue, 0)
            removed += 1
    image.paste(rgba)
    return removed


def build_report(image: Image.Image, *, removed_blob_pixels: int) -> dict:
    rgba = image.convert("RGBA")
    alphas = list(rgba.getchannel("A").getdata())
    visible = [alpha for alpha in alphas if alpha > 0]
    return {
        "schema": "game.dual_plate_alpha_report",
        "version": 1,
        "size": list(rgba.size),
        "visible_pixels": len(visible),
        "transparent_pixels": len(alphas) - len(visible),
        "min_visible_alpha": min(visible) if visible else 0,
        "max_visible_alpha": max(visible) if visible else 0,
        "mean_visible_alpha": round(mean(visible), 3) if visible else 0,
        "removed_blob_pixels": removed_blob_pixels,
    }


def write_markdown_report(path: Path, report: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "\n".join(
            [
                "---",
                "type: DualPlateAlphaReport",
                f"visible_pixels: {report['visible_pixels']}",
                "---",
                "",
                "# Dual Plate Alpha Report",
                "",
                f"Size: `{report['size'][0]}x{report['size'][1]}`",
                f"Visible pixels: `{report['visible_pixels']}`",
                f"Transparent pixels: `{report['transparent_pixels']}`",
                f"Visible alpha range: `{report['min_visible_alpha']}..{report['max_visible_alpha']}`",
                f"Mean visible alpha: `{report['mean_visible_alpha']}`",
                f"Removed blob pixels: `{report['removed_blob_pixels']}`",
                "",
            ]
        ),
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract RGBA PNG from aligned light/dark background plates.")
    parser.add_argument("--light", type=Path, required=True)
    parser.add_argument("--dark", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--bg-light", type=parse_color, default=parse_color("#ffffff"))
    parser.add_argument("--bg-dark", type=parse_color, default=parse_color("#000000"))
    parser.add_argument("--alpha-combine", choices=["min", "max", "avg"], default="min")
    parser.add_argument("--recovery-source", choices=["dark", "light", "average"], default="dark")
    parser.add_argument("--alpha-cutoff", type=int, default=0)
    parser.add_argument("--alpha-hardening", type=int, default=0)
    parser.add_argument("--blob-min-area", type=int, default=0)
    parser.add_argument("--keep-largest-blob", action="store_true")
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    result = extract_dual_plate_alpha(
        Image.open(args.light),
        Image.open(args.dark),
        bg_light=args.bg_light,
        bg_dark=args.bg_dark,
        alpha_combine=args.alpha_combine,
        recovery_source=args.recovery_source,
        alpha_cutoff=args.alpha_cutoff,
        alpha_hardening=args.alpha_hardening,
    )
    removed_blob_pixels = cleanup_alpha_blobs(
        result,
        min_area=args.blob_min_area,
        keep_largest=args.keep_largest_blob,
        alpha_threshold=max(1, args.alpha_cutoff),
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    result.save(args.output)

    report = build_report(result, removed_blob_pixels=removed_blob_pixels)
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
    if args.json_output:
        args.json_output.parent.mkdir(parents=True, exist_ok=True)
        args.json_output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    if args.report:
        write_markdown_report(args.report, report)
    print(f"wrote {args.output} ({report['visible_pixels']} visible pixels)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
