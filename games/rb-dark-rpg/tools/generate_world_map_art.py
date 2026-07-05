#!/usr/bin/env python3
"""Generate the first rb-dark-rpg world-map atlas.

The output is a deterministic raster map, not a UI mock: the same coordinate
helpers are used by the runtime marker overlay so roads, regions, and POIs stay
aligned with gameplay locations.
"""

from __future__ import annotations

import argparse
import math
import random
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter


MAP_WIDTH = 1280
MAP_HEIGHT = 720
SCALE = 2
SEED = 67011
ROAD_SHADOW_WIDTH = 34
ROAD_CORE_WIDTH = 18
ROAD_HIGHLIGHT_WIDTH = 4
GATE_OPEN_WIDTH = 86
GATE_OPEN_HEIGHT = 32
GATE_CLOSED_WIDTH = 68
GATE_CLOSED_HEIGHT = 24

LOCATION_POINTS = {
    "hub_last_post": (0.28, 0.38),
    "hub_gate_outskirts": (0.55, 0.38),
    "old_mill": (0.85, 0.60),
}

REGION_POLYGONS = {
    "hub_last_post": [
        (82, 548),
        (96, 388),
        (142, 285),
        (256, 192),
        (410, 142),
        (520, 190),
        (514, 356),
        (456, 494),
        (302, 620),
        (160, 592),
    ],
    "hub_gate_outskirts": [
        (510, 190),
        (630, 146),
        (790, 194),
        (916, 150),
        (1012, 202),
        (902, 390),
        (760, 510),
        (594, 502),
        (448, 488),
        (506, 356),
    ],
    "old_mill": [
        (902, 390),
        (1012, 202),
        (1118, 186),
        (1194, 332),
        (1144, 500),
        (976, 606),
        (760, 640),
        (760, 510),
    ],
    "south_marsh": [
        (92, 628),
        (292, 672),
        (420, 604),
        (522, 592),
        (740, 640),
        (700, 720),
        (0, 720),
        (0, 560),
    ],
}

REGION_GATES = [
    {"id": "last_post_to_outskirts", "center": (482, 448), "angle": -14, "open": True},
    {"id": "outskirts_to_mill", "center": (902, 390), "angle": -31, "open": True},
]

CURRENT_REGION_ID = "hub_last_post"

LAST_POST_LANDMARK_POLY = [(258, 382), (346, 348), (448, 386), (462, 472), (386, 530), (272, 502)]
OLD_MILL_YARD_POLY = [(1012, 238), (1104, 246), (1132, 326), (1086, 394), (994, 380), (966, 304)]


def location_to_pixel(x: float, y: float, width: int = MAP_WIDTH, height: int = MAP_HEIGHT) -> tuple[float, float]:
    px = 24.0 + max(0.0, min(1.0, x)) * (float(width) - 48.0)
    ui_y = 1.0 - max(0.0, min(1.0, y))
    py = 24.0 + ui_y * (float(height) - 74.0)
    return px, py


def sp(point: tuple[float, float]) -> tuple[int, int]:
    return (int(round(point[0] * SCALE)), int(round(point[1] * SCALE)))


def sv(value: float) -> int:
    return int(round(value * SCALE))


def color(r: int, g: int, b: int, a: int = 255) -> tuple[int, int, int, int]:
    return (r, g, b, a)


def bezier(points: list[tuple[float, float]], steps: int = 90) -> list[tuple[float, float]]:
    out: list[tuple[float, float]] = []
    for i in range(steps + 1):
        t = i / steps
        work = [p for p in points]
        while len(work) > 1:
            work = [
                (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
                for a, b in zip(work, work[1:])
            ]
        out.append(work[0])
    return out


def point_in_poly(x: float, y: float, poly: list[tuple[float, float]]) -> bool:
    inside = False
    j = len(poly) - 1
    for i, pi in enumerate(poly):
        pj = poly[j]
        if ((pi[1] > y) != (pj[1] > y)) and (
            x < (pj[0] - pi[0]) * (y - pi[1]) / ((pj[1] - pi[1]) or 1e-6) + pi[0]
        ):
            inside = not inside
        j = i
    return inside


def draw_poly(draw: ImageDraw.ImageDraw, pts: list[tuple[float, float]], fill, outline=None, width: int = 1) -> None:
    scaled = [sp(p) for p in pts]
    draw.polygon(scaled, fill=fill)
    if outline:
        draw.line(scaled + [scaled[0]], fill=outline, width=sv(width), joint="curve")


def draw_curve(draw: ImageDraw.ImageDraw, pts: list[tuple[float, float]], fill, width: float) -> None:
    draw.line([sp(p) for p in pts], fill=fill, width=sv(width), joint="curve")


def draw_rotated_rect(
    draw: ImageDraw.ImageDraw,
    center: tuple[float, float],
    width: float,
    height: float,
    angle_deg: float,
    fill,
    outline=None,
    outline_width: float = 1.0,
) -> None:
    cx, cy = center
    a = math.radians(angle_deg)
    ca = math.cos(a)
    sa = math.sin(a)
    corners = [(-width / 2, -height / 2), (width / 2, -height / 2), (width / 2, height / 2), (-width / 2, height / 2)]
    pts = [(cx + x * ca - y * sa, cy + x * sa + y * ca) for x, y in corners]
    draw.polygon([sp(p) for p in pts], fill=fill)
    if outline:
        draw.line([sp(p) for p in pts + [pts[0]]], fill=outline, width=sv(outline_width), joint="curve")


def draw_hut(draw: ImageDraw.ImageDraw, x: float, y: float, w: float, h: float, angle: float) -> None:
    draw_rotated_rect(draw, (x + 3, y + 4), w, h, angle, color(23, 18, 14, 110))
    draw_rotated_rect(draw, (x, y), w, h, angle, color(93, 72, 52, 235), color(42, 30, 20, 160), 1.5)
    draw_rotated_rect(draw, (x, y - h * 0.14), w * 1.05, h * 0.36, angle, color(128, 89, 54, 228))


def draw_tree(draw: ImageDraw.ImageDraw, x: float, y: float, size: float, rng: random.Random) -> None:
    trunk = color(62, 43, 27, 130)
    crown = rng.choice([
        color(31, 58, 38, 150),
        color(44, 70, 39, 140),
        color(49, 62, 33, 135),
    ])
    draw.ellipse([sv(x - size), sv(y - size * 0.75), sv(x + size), sv(y + size * 0.85)], fill=crown)
    draw.rectangle([sv(x - size * 0.12), sv(y), sv(x + size * 0.12), sv(y + size * 0.9)], fill=trunk)


def draw_gate_bar(draw: ImageDraw.ImageDraw, x: float, y: float, angle: float) -> None:
    draw_rotated_rect(draw, (x + 3, y + 4), GATE_OPEN_WIDTH + 6, GATE_OPEN_HEIGHT + 6, angle, color(22, 17, 10, 138))
    draw_rotated_rect(
        draw,
        (x, y),
        GATE_OPEN_WIDTH,
        GATE_OPEN_HEIGHT,
        angle,
        color(214, 146, 23, 248),
        color(72, 43, 10, 238),
        3.5,
    )
    draw_rotated_rect(draw, (x, y - 3), GATE_OPEN_WIDTH - 18, 9, angle, color(255, 224, 84, 184))
    draw_rotated_rect(draw, (x, y + 9), GATE_OPEN_WIDTH - 24, 5, angle, color(111, 63, 14, 116))
    draw_rotated_rect(draw, (x, y), GATE_OPEN_WIDTH - 34, 4, angle, color(255, 244, 154, 210))


def road_paths() -> dict[str, list[tuple[float, float]]]:
    post = location_to_pixel(*LOCATION_POINTS["hub_last_post"])
    gate = location_to_pixel(*LOCATION_POINTS["hub_gate_outskirts"])
    mill = location_to_pixel(*LOCATION_POINTS["old_mill"])
    mill_crossing = REGION_GATES[1]["center"]
    return {
        "main_road": bezier(
            [(46, 468), (188, 476), (post[0] - 12, post[1] + 4), (gate[0], gate[1]), mill_crossing],
            150,
        ),
        "road_to_mill": bezier(
            [mill_crossing, (958, 372), (1026, 392), (mill[0] - 8, mill[1] + 12)],
            100,
        ),
        "south_road": bezier([(post[0] - 22, post[1] + 36), (300, 534), (264, 612), (184, 690)], 80),
    }


def add_texture(img: Image.Image, rng: random.Random) -> Image.Image:
    w, h = img.size
    noise = Image.effect_noise((w, h), 46).convert("L")
    warm = Image.new("RGBA", (w, h), color(94, 77, 52, 0))
    warm.putalpha(noise.point(lambda v: max(0, min(46, int((v + 128) * 0.10)))))
    img = Image.alpha_composite(img, warm)

    flecks = Image.new("RGBA", (w, h), color(0, 0, 0, 0))
    d = ImageDraw.Draw(flecks)
    for _ in range(900):
        x = rng.randrange(0, w)
        y = rng.randrange(0, h)
        r = rng.choice([1, 1, 2, 2, 3])
        c = rng.choice([color(219, 184, 107, 24), color(42, 34, 24, 38), color(112, 96, 61, 28)])
        d.ellipse([x - r, y - r, x + r, y + r], fill=c)
    return Image.alpha_composite(img, flecks)


def add_vignette(img: Image.Image) -> Image.Image:
    w, h = img.size
    mask = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(mask)
    d.ellipse([sv(-80), sv(-160), sv(MAP_WIDTH + 80), sv(MAP_HEIGHT + 120)], fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(sv(80)))
    dark = Image.new("RGBA", (w, h), color(11, 10, 8, 105))
    inv = ImageChops.invert(mask)
    dark.putalpha(inv.point(lambda v: int(v * 0.56)))
    return Image.alpha_composite(img, dark)


def build_map_art() -> Image.Image:
    rng = random.Random(SEED)
    size = (MAP_WIDTH * SCALE, MAP_HEIGHT * SCALE)
    img = Image.new("RGBA", size, color(37, 33, 24, 255))
    draw = ImageDraw.Draw(img, "RGBA")

    # Neighboring lands and fog outside the playable first-region cluster.
    neighbor_shapes = [
        [(0, 72), (238, 36), (410, 126), (294, 284), (42, 318), (0, 226)],
        [(884, 70), (1280, 58), (1280, 312), (1136, 354), (1010, 242)],
        [(0, 560), (286, 598), (452, 720), (0, 720)],
        [(846, 560), (1280, 498), (1280, 720), (700, 720)],
    ]
    for pts in neighbor_shapes:
        draw_poly(draw, pts, color(24, 29, 23, 190), color(71, 66, 44, 65), 2)

    region_draw_order = [rid for rid in REGION_POLYGONS if rid != CURRENT_REGION_ID] + [CURRENT_REGION_ID]
    for region_id in region_draw_order:
        pts = REGION_POLYGONS[region_id]
        if region_id == CURRENT_REGION_ID:
            fill = color(119, 111, 60, 242)
            outline = color(244, 235, 76, 222)
        elif region_id == "south_marsh":
            fill = color(28, 43, 33, 132)
            outline = color(111, 115, 48, 88)
        else:
            fill = color(52, 60, 39, 172)
            outline = color(126, 136, 49, 108)
        draw_poly(draw, pts, fill, outline, 4)

    # Water cuts through the region before roads are drawn on top.
    river = bezier([(64, 448), (246, 412), (382, 440), (520, 386), (682, 388)], 120)
    draw_curve(draw, river, color(20, 39, 48, 190), 30)
    draw_curve(draw, river, color(34, 74, 84, 150), 18)
    draw_curve(draw, bezier([(692, 278), (780, 250), (870, 254), (960, 232)], 70), color(35, 75, 83, 120), 18)
    draw.ellipse([sv(128), sv(294), sv(300), sv(382)], fill=color(18, 43, 53, 150))

    post = location_to_pixel(*LOCATION_POINTS["hub_last_post"])
    gate = location_to_pixel(*LOCATION_POINTS["hub_gate_outskirts"])
    mill = location_to_pixel(*LOCATION_POINTS["old_mill"])

    # Roads describe local geography. Region gates, not roads, describe traversal.
    for path in road_paths().values():
        draw_curve(draw, path, color(39, 29, 20, 112), ROAD_SHADOW_WIDTH)
        draw_curve(draw, path, color(124, 98, 61, 184), ROAD_CORE_WIDTH)
        draw_curve(draw, path, color(185, 151, 91, 54), ROAD_HIGHLIGHT_WIDTH)

    # Border crossings: short yellow bars on region borders, like the reference.
    for gate_def in REGION_GATES:
        if gate_def["open"]:
            draw_gate_bar(draw, gate_def["center"][0], gate_def["center"][1], gate_def["angle"])
        else:
            draw_rotated_rect(
                draw,
                (gate_def["center"][0] + 2, gate_def["center"][1] + 2),
                GATE_CLOSED_WIDTH,
                GATE_CLOSED_HEIGHT,
                gate_def["angle"],
                color(28, 24, 20, 86),
            )
            draw_rotated_rect(
                draw,
                gate_def["center"],
                GATE_CLOSED_WIDTH,
                GATE_CLOSED_HEIGHT,
                gate_def["angle"],
                color(94, 78, 43, 120),
                color(134, 112, 65, 112),
                1.5,
            )

    # Last Post landmark: compact walled town inside the active region, not a subregion.
    draw_poly(draw, LAST_POST_LANDMARK_POLY, color(82, 58, 38, 46), None)
    wall_scaled = [sp(p) for p in LAST_POST_LANDMARK_POLY]
    draw.line(wall_scaled + [wall_scaled[0]], fill=color(126, 60, 42, 220), width=sv(5), joint="curve")
    draw.line(wall_scaled + [wall_scaled[0]], fill=color(64, 38, 28, 150), width=sv(2), joint="curve")
    for x, y in LAST_POST_LANDMARK_POLY:
        draw.ellipse([sv(x - 10), sv(y - 10), sv(x + 10), sv(y + 10)], fill=color(129, 58, 47, 220))
    for args in [(318, 426, 54, 32, -10), (383, 392, 44, 28, 8), (414, 468, 58, 30, 16), (292, 490, 44, 28, -18)]:
        draw_hut(draw, *args)
    draw_rotated_rect(draw, (post[0] + 74, post[1] - 48), 32, 56, 9, color(126, 90, 45, 230), color(74, 42, 21, 165), 2)

    # Gate outskirt defenses and hazard mark.
    draw_rotated_rect(draw, (gate[0] - 48, gate[1] - 24), 36, 72, 0, color(96, 66, 38, 220), color(57, 36, 22, 180), 2)
    draw_rotated_rect(draw, (gate[0] + 44, gate[1] - 20), 34, 64, 0, color(96, 66, 38, 218), color(57, 36, 22, 180), 2)
    draw.ellipse([sv(gate[0] - 18), sv(gate[1] - 18), sv(gate[0] + 18), sv(gate[1] + 18)], fill=color(133, 31, 27, 210))
    draw.line([sp((gate[0] - 9, gate[1] - 9)), sp((gate[0] + 9, gate[1] + 9))], fill=color(244, 190, 75, 210), width=sv(5))
    draw.line([sp((gate[0] + 9, gate[1] - 9)), sp((gate[0] - 9, gate[1] + 9))], fill=color(244, 190, 75, 210), width=sv(5))

    # Old mill landmark: compact yard fully contained inside the mill region.
    draw.ellipse([sv(mill[0] - 76), sv(mill[1] - 58), sv(mill[0] + 84), sv(mill[1] + 68)], fill=color(74, 60, 40, 72))
    draw_hut(draw, mill[0] - 8, mill[1] + 22, 82, 52, -10)
    draw.rectangle([sv(mill[0] - 10), sv(mill[1] - 66), sv(mill[0] + 10), sv(mill[1] + 16)], fill=color(141, 101, 58, 228))
    draw.line([sp((mill[0] - 46, mill[1] - 34)), sp((mill[0] + 44, mill[1] - 58))], fill=color(177, 139, 76, 215), width=sv(9))
    draw.line([sp((mill[0] - 34, mill[1] - 70)), sp((mill[0] + 36, mill[1] - 22))], fill=color(177, 139, 76, 215), width=sv(9))
    draw.ellipse([sv(mill[0] - 15), sv(mill[1] - 54), sv(mill[0] + 15), sv(mill[1] - 24)], fill=color(96, 60, 35, 230))

    # Scatter readable terrain detail. Avoid the major road by accepting light overlap only.
    for _ in range(115):
        x = rng.uniform(110, 1160)
        y = rng.uniform(160, 620)
        if not any(point_in_poly(x, y, poly) for poly in REGION_POLYGONS.values()):
            continue
        if rng.random() < 0.62:
            draw_tree(draw, x, y, rng.uniform(6, 13), rng)
        else:
            r = rng.uniform(3, 7)
            draw.ellipse([sv(x - r), sv(y - r * 0.65), sv(x + r), sv(y + r * 0.65)], fill=color(93, 84, 62, rng.randrange(80, 145)))

    # Region line pass over details keeps borders dominant after roads and props.
    for region_id in region_draw_order:
        pts = REGION_POLYGONS[region_id]
        scaled_region = [sp(p) for p in pts]
        if region_id == CURRENT_REGION_ID:
            outer = color(36, 43, 16, 245)
            inner = color(250, 244, 70, 255)
            inner_w = 6
        elif region_id == "south_marsh":
            outer = color(26, 34, 23, 138)
            inner = color(104, 116, 46, 118)
            inner_w = 2
        else:
            outer = color(34, 43, 24, 194)
            inner = color(156, 162, 49, 180)
            inner_w = 3
        draw.line(scaled_region + [scaled_region[0]], fill=outer, width=sv(10), joint="curve")
        draw.line(scaled_region + [scaled_region[0]], fill=inner, width=sv(inner_w), joint="curve")

    for gate_def in REGION_GATES:
        if gate_def["open"]:
            draw_gate_bar(draw, gate_def["center"][0], gate_def["center"][1], gate_def["angle"])
        else:
            draw_rotated_rect(
                draw,
                gate_def["center"],
                GATE_CLOSED_WIDTH,
                GATE_CLOSED_HEIGHT,
                gate_def["angle"],
                color(94, 78, 43, 142),
                color(134, 112, 65, 126),
                1.5,
            )

    img = add_texture(img, rng)
    img = add_vignette(img)
    return img.resize((MAP_WIDTH, MAP_HEIGHT), Image.Resampling.LANCZOS).convert("RGBA")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="games/rb-dark-rpg/assets/ui/generated/world_map_ash_border_01/ash_border_map.png")
    args = parser.parse_args()

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    build_map_art().save(out)
    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
