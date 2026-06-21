#!/usr/bin/env python3
"""Build a review-only before/after montage for the T0024 forge proof."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT = "gamedesign/projects/ember-road/reviews/T0024_town_forge_lead_acceptance_moment.png"


def load_font(size: int) -> ImageFont.ImageFont:
    for name in ("arial.ttf", "segoeui.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def run(output: str) -> int:
    shots = [
        ("Before click: forge Mine Lantern", ROOT / "build/captures/ember-road/state_town_lantern_upgrade.png"),
        ("After click: lantern ready, Depth 2 route lit", ROOT / "build/captures/ember-road/state_town_lantern_forged.png"),
    ]
    images: list[tuple[str, Image.Image]] = []
    for title, path in shots:
        if not path.exists():
            raise FileNotFoundError(path)
        image = Image.open(path).convert("RGB")
        image.thumbnail((640, 360), Image.Resampling.LANCZOS)
        images.append((title, image))

    canvas = Image.new("RGB", (1320, 500), (20, 18, 20))
    draw = ImageDraw.Draw(canvas)
    title_font = load_font(22)
    caption_font = load_font(16)

    for (title, image), x in zip(images, (20, 660), strict=True):
        draw.rectangle((x - 4, 50, x + 644, 414), fill=(44, 36, 30), outline=(172, 128, 52), width=2)
        canvas.paste(image, (x, 54))
        draw.text((x + 10, 20), title, fill=(255, 220, 140), font=title_font)

    caption = "T0024 native forge proof: scene action -> item/result rail -> visible route unlock. Y-up game/UI logic."
    draw.text((20, 440), caption, fill=(235, 220, 185), font=caption_font)

    output_path = ROOT / output
    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(output_path)
    print(output)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    return run(args.output)


if __name__ == "__main__":
    raise SystemExit(main())
