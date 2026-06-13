#!/usr/bin/env python3
"""Create the Windows .ico used by the native PC release."""

from __future__ import annotations

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets" / "runtime" / "67-world" / "icon_67_badge-v1.png"
OUTPUT = ROOT / "assets" / "runtime" / "67-world" / "67-world.ico"
SIZES = [16, 24, 32, 48, 64, 128, 256]


def fit_on_square(source: Image.Image, size: int) -> Image.Image:
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    margin = max(1, round(size * 0.08))
    max_side = size - margin * 2
    image = source.copy()
    image.thumbnail((max_side, max_side), Image.Resampling.LANCZOS)
    x = (size - image.width) // 2
    y = (size - image.height) // 2
    canvas.alpha_composite(image, (x, y))
    return canvas


def main() -> int:
    if not SOURCE.exists():
        raise FileNotFoundError(SOURCE)
    source = Image.open(SOURCE).convert("RGBA")
    images = [fit_on_square(source, size) for size in SIZES]
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    images[-1].save(OUTPUT, sizes=[(size, size) for size in SIZES], append_images=images[:-1])
    print(f"generated: {OUTPUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
