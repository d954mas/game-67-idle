#!/usr/bin/env python3
"""Create a brighter toy/plastic runtime atlas from the downloaded CC0 mech atlas."""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image


def clamp_byte(value: float) -> int:
    return max(0, min(255, int(round(value))))


def recolor_pixel(r: int, g: int, b: int, a: int) -> tuple[int, int, int, int]:
    if a == 0:
        return (r, g, b, a)

    chroma = max(r, g, b) - min(r, g, b)

    if r > 150 and g > 80 and b < 80:
        # Keep orange panels punchy and readable as module/armor accents.
        return (255, clamp_byte(g * 1.08 + 26), clamp_byte(b * 0.82 + 8), a)
    if g > r * 1.18 and g > b * 1.05:
        # Push the old muddy green armor toward toy emerald plastic.
        return (46, clamp_byte(g * 1.38 + 38), clamp_byte(b * 1.18 + 44), a)
    if b > 115 and g > 90 and r < 80:
        # Preserve cyan/blue tech accents.
        return (30, clamp_byte(g * 1.12 + 30), 255, a)
    if r > 85 and b > 90 and g < 55:
        # Preserve small magenta/red details as hot accent paint.
        return (255, 34, 116, a)
    if chroma < 18:
        # Neutral mech metal becomes blue-tinted toy plastic/painted metal.
        v = (r + g + b) / 3.0
        if v < 45:
            return (24, 34, 48, a)
        if v < 90:
            return (64, 92, 118, a)
        if v < 145:
            return (100, 140, 166, a)
        if v < 215:
            return (164, 206, 226, a)
        return (242, 252, 255, a)

    # Mild saturation/value lift for any rare source colors not classified
    # above, keeping the downloaded atlas recognizable.
    avg = (r + g + b) / 3.0
    sat = 1.22
    return (
        clamp_byte(avg + (r - avg) * sat + 8),
        clamp_byte(avg + (g - avg) * sat + 8),
        clamp_byte(avg + (b - avg) * sat + 8),
        a,
    )


def recolor(src: Path, dst: Path) -> None:
    image = Image.open(src).convert("RGBA")
    out = Image.new("RGBA", image.size)
    pixels = image.get_flattened_data() if hasattr(image, "get_flattened_data") else image.getdata()
    out.putdata([recolor_pixel(*px) for px in pixels])
    dst.parent.mkdir(parents=True, exist_ok=True)
    out.save(dst)


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print("usage: recolor_mech_atlas.py <source.png> <runtime-output.png>", file=sys.stderr)
        return 2
    recolor(Path(argv[1]), Path(argv[2]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
