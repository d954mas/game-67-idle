#!/usr/bin/env python3
"""Generate a tileable stylized-studs grass texture source and seam preview."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / "assets" / "textures" / "mech_builder_battler_stylized_studs_grass_tile_v1.png"
SOURCE = ROOT / "assets" / "source" / "textures" / "mech-builder-battler" / "stylized_studs_grass_tile_v1.png"
PREVIEW = (
    ROOT
    / "gamedesign"
    / "projects"
    / "mech-builder-battler"
    / "art"
    / "texture_previews"
    / "stylized_studs_grass_tile_v1_2x2.png"
)
SIZE = 1024


def wrap_ellipse(draw: ImageDraw.ImageDraw, xy: tuple[float, float, float, float], fill) -> None:
    x0, y0, x1, y1 = xy
    for ox in (-SIZE, 0, SIZE):
        for oy in (-SIZE, 0, SIZE):
            draw.ellipse((x0 + ox, y0 + oy, x1 + ox, y1 + oy), fill=fill)


def wrap_line(draw: ImageDraw.ImageDraw, points: list[tuple[float, float]], fill, width: int) -> None:
    for ox in (-SIZE, 0, SIZE):
        for oy in (-SIZE, 0, SIZE):
            draw.line([(x + ox, y + oy) for x, y in points], fill=fill, width=width, joint="curve")


def near_leaf(cx: float, cy: float) -> bool:
    motifs = [(130, 160), (420, 110), (760, 220), (210, 520), (600, 560), (910, 760), (85, 900)]
    for mx, my in motifs:
        dx = min(abs(cx - mx), SIZE - abs(cx - mx))
        dy = min(abs(cy - my), SIZE - abs(cy - my))
        if (dx * dx) / (145 * 145) + (dy * dy) / (78 * 78) < 1.0:
            return True
    return False


def draw_tile() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (126, 210, 54, 255))
    draw = ImageDraw.Draw(img, "RGBA")

    for y in range(0, SIZE, 128):
        for x in range(0, SIZE, 128):
            tint = (112, 198, 48, 55) if ((x + y) // 128) % 2 else (158, 226, 70, 45)
            draw.rectangle((x, y, x + 128, y + 128), fill=tint)

    for mx, my in [(130, 160), (420, 110), (760, 220), (210, 520), (600, 560), (910, 760), (85, 900)]:
        wrap_ellipse(draw, (mx - 126, my - 54, mx + 126, my + 54), fill=(87, 178, 39, 62))
        wrap_line(draw, [(mx - 92, my + 12), (mx - 18, my - 42), (mx + 98, my - 8)], (183, 247, 64, 160), 13)
        wrap_line(draw, [(mx - 74, my + 26), (mx - 20, my + 62), (mx + 72, my + 28)], (78, 161, 36, 100), 11)
        wrap_line(draw, [(mx - 18, my - 36), (mx - 72, my - 72)], (190, 255, 72, 120), 9)
        wrap_line(draw, [(mx + 18, my - 28), (mx + 72, my - 70)], (190, 255, 72, 120), 9)

    stud_shadow = (61, 116, 38, 82)
    stud_hi = (186, 243, 89, 84)
    stud_face = (105, 185, 45, 110)
    for y in range(34, SIZE, 48):
        for x in range(34, SIZE, 48):
            if near_leaf(x, y):
                continue
            wrap_ellipse(draw, (x - 8, y - 5, x + 10, y + 13), fill=stud_shadow)
            wrap_ellipse(draw, (x - 10, y - 10, x + 10, y + 10), fill=stud_face)
            wrap_ellipse(draw, (x - 6, y - 8, x + 5, y - 1), fill=stud_hi)

    img = img.filter(ImageFilter.UnsharpMask(radius=1.2, percent=120, threshold=4))
    return img


def main() -> None:
    tile = draw_tile()
    SOURCE.parent.mkdir(parents=True, exist_ok=True)
    RUNTIME.parent.mkdir(parents=True, exist_ok=True)
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    tile.save(SOURCE)
    tile.save(RUNTIME)

    preview = Image.new("RGBA", (SIZE * 2, SIZE * 2), (0, 0, 0, 0))
    for y in range(2):
        for x in range(2):
            preview.alpha_composite(tile, (x * SIZE, y * SIZE))
    preview.resize((1024, 1024), Image.Resampling.LANCZOS).save(PREVIEW)
    print(f"wrote {SOURCE.relative_to(ROOT)}")
    print(f"wrote {RUNTIME.relative_to(ROOT)}")
    print(f"wrote {PREVIEW.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
