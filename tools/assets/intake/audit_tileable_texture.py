#!/usr/bin/env python3
"""Audit repeat texture seams and write a 2x2 tiling preview."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from PIL import Image


def pixel_delta(a: tuple[int, int, int, int], b: tuple[int, int, int, int]) -> float:
    return sum(abs(int(left) - int(right)) for left, right in zip(a, b)) / 4.0


def edge_stats(image: Image.Image) -> dict[str, float]:
    rgba = image.convert("RGBA")
    width, height = rgba.size
    if width < 2 or height < 2:
        raise ValueError("image must be at least 2x2")

    pixels = rgba.load()
    horizontal = [pixel_delta(pixels[0, y], pixels[width - 1, y]) for y in range(height)]
    vertical = [pixel_delta(pixels[x, 0], pixels[x, height - 1]) for x in range(width)]
    combined = horizontal + vertical
    return {
        "width": width,
        "height": height,
        "left_right_mean_delta": round(sum(horizontal) / len(horizontal), 4),
        "left_right_max_delta": round(max(horizontal), 4),
        "top_bottom_mean_delta": round(sum(vertical) / len(vertical), 4),
        "top_bottom_max_delta": round(max(vertical), 4),
        "mean_edge_delta": round(sum(combined) / len(combined), 4),
        "max_edge_delta": round(max(combined), 4),
    }


def write_preview(image: Image.Image, path: Path) -> None:
    rgba = image.convert("RGBA")
    width, height = rgba.size
    preview = Image.new("RGBA", (width * 2, height * 2), (0, 0, 0, 0))
    for y in range(2):
        for x in range(2):
            preview.alpha_composite(rgba, (x * width, y * height))
    path.parent.mkdir(parents=True, exist_ok=True)
    preview.save(path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--preview", type=Path)
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--max-mean-edge-delta", type=float)
    parser.add_argument("--max-edge-delta", type=float)
    return parser.parse_args()


def report_markdown(source: Path, preview: Path | None, result: dict[str, object]) -> str:
    return "\n".join(
        [
            f"# Tileable Texture Audit: {source.name}",
            "",
            f"- Source: {source}",
            f"- Preview: {preview if preview else '-'}",
            f"- Verdict: {result['verdict']}",
            f"- Size: {result['width']}x{result['height']}",
            f"- Mean edge delta: {result['mean_edge_delta']}",
            f"- Max edge delta: {result['max_edge_delta']}",
            "",
            "Review the 2x2 preview at gameplay scale before accepting a repeated texture.",
            "",
        ]
    )


def main() -> int:
    args = parse_args()
    image = Image.open(args.source)
    result: dict[str, object] = edge_stats(image)
    problems: list[str] = []
    if args.max_mean_edge_delta is not None and result["mean_edge_delta"] > args.max_mean_edge_delta:
        problems.append(f"mean edge delta {result['mean_edge_delta']} > {args.max_mean_edge_delta}")
    if args.max_edge_delta is not None and result["max_edge_delta"] > args.max_edge_delta:
        problems.append(f"max edge delta {result['max_edge_delta']} > {args.max_edge_delta}")
    result.update(
        {
            "source": str(args.source),
            "preview": str(args.preview) if args.preview else None,
            "verdict": "fail" if problems else "pass",
            "problems": problems,
        }
    )

    if args.preview:
        write_preview(image, args.preview)
    if args.json_output:
        args.json_output.parent.mkdir(parents=True, exist_ok=True)
        args.json_output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(report_markdown(args.source, args.preview, result), encoding="utf-8")

    print(json.dumps(result, indent=2))
    return 1 if problems else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
