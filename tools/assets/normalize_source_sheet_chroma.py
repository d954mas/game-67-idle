#!/usr/bin/env python3
from __future__ import annotations

import argparse
from collections import deque
from pathlib import Path

from PIL import Image


def parse_color(value: str) -> tuple[int, int, int]:
    text = value.strip().lstrip("#")
    if len(text) != 6:
        raise argparse.ArgumentTypeError("color must be #rrggbb")
    return tuple(int(text[index : index + 2], 16) for index in (0, 2, 4))


def is_key_like(pixel: tuple[int, int, int, int], key: tuple[int, int, int], tolerance: int) -> bool:
    red, green, blue, alpha = pixel
    return alpha == 0 or max(abs(red - key[0]), abs(green - key[1]), abs(blue - key[2])) <= tolerance


def normalize_background(source: Path, output: Path, key: tuple[int, int, int], tolerance: int) -> int:
    image = Image.open(source).convert("RGBA")
    width, height = image.size
    pixels = image.load()
    visited = bytearray(width * height)
    queue: deque[tuple[int, int]] = deque()

    def offset(x: int, y: int) -> int:
        return y * width + x

    def push(x: int, y: int) -> None:
        index = offset(x, y)
        if visited[index]:
            return
        visited[index] = 1
        if is_key_like(pixels[x, y], key, tolerance):
            queue.append((x, y))

    for x in range(width):
        push(x, 0)
        push(x, height - 1)
    for y in range(height):
        push(0, y)
        push(width - 1, y)

    changed = 0
    while queue:
        x, y = queue.popleft()
        red, green, blue, alpha = pixels[x, y]
        if (red, green, blue, alpha) != (*key, 255):
            pixels[x, y] = (*key, 255)
            changed += 1
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < width and 0 <= ny < height:
                push(nx, ny)

    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)
    return changed


def main() -> int:
    parser = argparse.ArgumentParser(description="Normalize border-connected chroma background in a generated source sheet.")
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--key-color", type=parse_color, default=parse_color("#ff00ff"))
    parser.add_argument("--key-tolerance", type=int, default=48)
    args = parser.parse_args()

    changed = normalize_background(args.source, args.output, args.key_color, args.key_tolerance)
    print(f"wrote {args.output} ({changed} background pixels normalized)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
