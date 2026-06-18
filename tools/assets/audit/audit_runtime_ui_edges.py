#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import os
from typing import Any

from PIL import Image


def parse_crop(value: str | None) -> tuple[int, int, int, int] | None:
    if not value:
        return None
    parts = [int(part.strip()) for part in value.split(",")]
    if len(parts) != 4:
        raise argparse.ArgumentTypeError("--crop must be x1,y1,x2,y2")
    x1, y1, x2, y2 = parts
    if x2 <= x1 or y2 <= y1:
        raise argparse.ArgumentTypeError("--crop must have x2>x1 and y2>y1")
    return x1, y1, x2, y2


def parse_rgb(value: str | None) -> tuple[int, int, int] | None:
    if not value:
        return None
    parts = [int(part.strip()) for part in value.split(",")]
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("--key-color must be r,g,b")
    if any(part < 0 or part > 255 for part in parts):
        raise argparse.ArgumentTypeError("--key-color channels must be 0-255")
    return parts[0], parts[1], parts[2]


def is_bad_pixel(
    px: tuple[int, int, int, int],
    family: str,
    *,
    min_alpha: int,
    key_color: tuple[int, int, int] | None,
    tolerance: float,
) -> bool:
    r, g, b, a = px
    if a < min_alpha:
        return False
    if family == "purple":
        return r > 95 and b > 95 and g < 85 and (r - g) > 40 and (b - g) > 40
    if family == "magenta":
        return r > 150 and b > 130 and g < 115 and (r - g) > 45 and (b - g) > 30
    if family == "key":
        if key_color is None:
            raise ValueError("--key-color is required when --family key")
        kr, kg, kb = key_color
        return math.dist((r, g, b), (kr, kg, kb)) <= tolerance
    raise ValueError(f"unknown family: {family}")


def audit(args: argparse.Namespace) -> dict[str, Any]:
    full = os.path.abspath(args.image)
    image = Image.open(full).convert("RGBA")
    origin = (0, 0)
    crop = parse_crop(args.crop)
    if crop:
        image = image.crop(crop)
        origin = (crop[0], crop[1])

    key_color = parse_rgb(args.key_color)
    samples: list[dict[str, Any]] = []
    bad = 0
    visible = 0
    for y in range(image.height):
        for x in range(image.width):
            px = image.getpixel((x, y))
            if px[3] >= args.min_alpha:
                visible += 1
            if is_bad_pixel(
                px,
                args.family,
                min_alpha=args.min_alpha,
                key_color=key_color,
                tolerance=args.tolerance,
            ):
                bad += 1
                if len(samples) < args.samples:
                    samples.append({"x": origin[0] + x, "y": origin[1] + y, "rgba": list(px)})

    pct = (bad / visible * 100.0) if visible else 0.0
    passed = bad <= args.max_pixels and pct <= args.max_pct
    return {
        "schema": "game.runtime_ui_edge_audit",
        "image": full,
        "crop": list(crop) if crop else None,
        "size": [image.width, image.height],
        "family": args.family,
        "min_alpha": args.min_alpha,
        "key_color": list(key_color) if key_color else None,
        "tolerance": args.tolerance,
        "visible_pixels": visible,
        "bad_pixels": bad,
        "bad_visible_pct": round(pct, 5),
        "max_pixels": args.max_pixels,
        "max_pct": args.max_pct,
        "verdict": "pass" if passed else "fail",
        "samples": samples,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit runtime UI image/crop edges for visible chroma/key-color fringe.")
    parser.add_argument("--image", required=True, help="source PNG or screenshot path")
    parser.add_argument("--crop", help="optional crop as x1,y1,x2,y2")
    parser.add_argument("--family", choices=["purple", "magenta", "key"], default="purple")
    parser.add_argument("--key-color", help="required for --family key, as r,g,b")
    parser.add_argument("--tolerance", type=float, default=24.0)
    parser.add_argument("--min-alpha", type=int, default=24)
    parser.add_argument("--max-pixels", type=int, default=0)
    parser.add_argument("--max-pct", type=float, default=0.0)
    parser.add_argument("--samples", type=int, default=12)
    parser.add_argument("--json-output")
    args = parser.parse_args()

    if args.family == "key" and not args.key_color:
        parser.error("--key-color is required when --family key")

    report = audit(args)
    text = json.dumps(report, indent=2)
    print(text)
    if args.json_output:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_output)), exist_ok=True)
        with open(args.json_output, "w", encoding="utf-8") as handle:
            handle.write(text + "\n")
    return 0 if report["verdict"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
