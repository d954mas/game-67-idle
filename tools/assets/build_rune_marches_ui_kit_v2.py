#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
from collections import deque

from PIL import Image, ImageDraw, ImageFilter, ImageFont


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "gamedesign/projects/rune-marches/art/source_sheets/rune-marches-ui-kit-source-v2.png"
OUT_DIR = ROOT / "assets/runtime/rune-marches-ui-map-rescue-v2"
PREVIEW_DIR = ROOT / "gamedesign/projects/rune-marches/art/previews"


CROPS = [
    {
        "id": "modal_panel",
        "kind": "slice9",
        "base_style": "large_panel",
        "rect": (28, 47, 818, 545),
        "out": "modal_panel_slice9.png",
        "slice9": {"left": 92, "top": 96, "right": 92, "bottom": 82},
        "content": {"x": 120, "y": 120, "w": 578, "h": 320},
        "preview": [(360, 240), (520, 320), (760, 420)],
        "overscan": {"left": 18, "top": 18, "right": 18, "bottom": 18},
        "cleanup_patches": [
            {"target": (345, 0, 515, 130), "sample": (150, 52, 320, 130)},
            {"target": (345, 462, 515, 581), "sample": (520, 500, 690, 581)},
            {"target": (0, 210, 98, 350), "sample": (18, 96, 98, 236)},
            {"target": (756, 210, 854, 350), "sample": (756, 96, 836, 236)},
        ],
        "decorations": [
            {"id": "decor_modal_top_gem", "rect": (345, 0, 515, 130), "out": "decor_modal_top_gem.png"},
            {"id": "decor_modal_bottom_gem", "rect": (345, 462, 515, 581), "out": "decor_modal_bottom_gem.png"},
        ],
    },
    {
        "id": "journal_panel",
        "kind": "slice9",
        "base_style": "large_panel",
        "rect": (889, 148, 335, 438),
        "out": "journal_panel_slice9.png",
        "slice9": {"left": 54, "top": 74, "right": 54, "bottom": 64},
        "content": {"x": 72, "y": 96, "w": 191, "h": 246},
        "preview": [(220, 280), (300, 380), (360, 460)],
        "overscan": {"left": 16, "top": 40, "right": 14, "bottom": 28},
        "cleanup_patches": [
            {"target": (104, 0, 246, 86), "sample": (36, 40, 178, 86)},
            {"target": (104, 418, 246, 506), "sample": (190, 430, 332, 506)},
            {"target": (0, 190, 72, 310), "sample": (16, 80, 72, 200)},
            {"target": (293, 190, 365, 310), "sample": (293, 80, 349, 200)},
        ],
        "decorations": [
            {"id": "decor_journal_top_gem", "rect": (104, 0, 246, 86), "out": "decor_journal_top_gem.png"},
            {"id": "decor_journal_bottom_gem", "rect": (104, 418, 246, 506), "out": "decor_journal_bottom_gem.png"},
        ],
    },
    {
        "id": "button_idle",
        "kind": "slice9",
        "base_style": "button_idle",
        "rect": (28, 660, 395, 145),
        "out": "button_idle_slice9.png",
        "slice9": {"left": 56, "top": 20, "right": 56, "bottom": 20},
        "content": {"x": 76, "y": 42, "w": 243, "h": 61},
        "preview": [(128, 48), (180, 56), (240, 64)],
        "overscan": {"left": 14, "top": 0, "right": 22, "bottom": 0},
    },
    {
        "id": "button_pressed",
        "kind": "slice9",
        "base_style": "button_pressed",
        "rect": (463, 664, 378, 139),
        "out": "button_pressed_slice9.png",
        "slice9": {"left": 56, "top": 20, "right": 56, "bottom": 20},
        "content": {"x": 76, "y": 42, "w": 226, "h": 55},
        "preview": [(128, 48), (180, 56), (240, 64)],
        "overscan": {"left": 14, "top": 0, "right": 14, "bottom": 0},
    },
    {
        "id": "button_disabled",
        "kind": "slice9",
        "base_style": "button_disabled",
        "rect": (873, 667, 351, 138),
        "out": "button_disabled_slice9.png",
        "slice9": {"left": 52, "top": 20, "right": 52, "bottom": 20},
        "content": {"x": 72, "y": 42, "w": 207, "h": 54},
        "preview": [(128, 48), (180, 56), (240, 64)],
        "overscan": {"left": 14, "top": 24, "right": 16, "bottom": 0},
    },
    {
        "id": "status_bar",
        "kind": "slice9",
        "base_style": "status_bar",
        "rect": (30, 850, 1194, 82),
        "out": "status_bar_slice9.png",
        "slice9": {"left": 72, "top": 20, "right": 72, "bottom": 20},
        "content": {"x": 96, "y": 20, "w": 1002, "h": 42},
        "preview": [(320, 48), (540, 56), (900, 64)],
        "overscan": {"left": 16, "top": 0, "right": 16, "bottom": 22},
    },
    {
        "id": "reward_chip",
        "kind": "slice9",
        "base_style": "reward_chip",
        "rect": (28, 1018, 214, 130),
        "out": "reward_chip_slice9.png",
        "slice9": {"left": 44, "top": 20, "right": 44, "bottom": 20},
        "content": {"x": 52, "y": 46, "w": 110, "h": 38},
        "preview": [(120, 56), (160, 64), (220, 72)],
        "overscan": {"left": 8, "top": 48, "right": 8, "bottom": 44},
        "cleanup_patches": [
            {"target": (64, 0, 166, 70), "sample": (28, 48, 130, 118)},
            {"target": (64, 150, 166, 222), "sample": (92, 150, 194, 222)},
        ],
        "decorations": [
            {"id": "decor_reward_top_gem", "rect": (64, 0, 166, 70), "out": "decor_reward_top_gem.png"},
            {"id": "decor_reward_bottom_gem", "rect": (64, 150, 166, 222), "out": "decor_reward_bottom_gem.png"},
        ],
    },
    {
        "id": "icon_frame",
        "kind": "slice9",
        "base_style": "icon_frame",
        "rect": (261, 984, 193, 189),
        "out": "icon_frame_slice9.png",
        "slice9": {"left": 24, "top": 24, "right": 24, "bottom": 24},
        "content": {"x": 54, "y": 54, "w": 85, "h": 81},
        "preview": [(64, 64), (96, 96), (128, 128)],
        "overscan": {"left": 12, "top": 38, "right": 12, "bottom": 24},
        "cleanup_patches": [
            {"target": (66, 0, 150, 68), "sample": (24, 38, 108, 106)},
            {"target": (66, 186, 150, 251), "sample": (108, 186, 192, 251)},
        ],
        "decorations": [
            {"id": "decor_icon_frame_top_gem", "rect": (66, 0, 150, 68), "out": "decor_icon_frame_top_gem.png"},
            {"id": "decor_icon_frame_bottom_gem", "rect": (66, 186, 150, 251), "out": "decor_icon_frame_bottom_gem.png"},
        ],
    },
    {
        "id": "icon_health",
        "kind": "icon",
        "rect": (494, 1016, 120, 128),
        "out": "icon_health.png",
        "role": "health",
        "size": "96px source",
        "trim_padding": 10,
        "isolate_component": "center",
    },
    {
        "id": "icon_mana",
        "kind": "icon",
        "rect": (620, 1000, 118, 152),
        "out": "icon_mana.png",
        "role": "mana",
        "size": "96px source",
        "trim_padding": 10,
        "isolate_component": "center",
    },
    {
        "id": "icon_silver",
        "kind": "icon",
        "rect": (725, 996, 136, 152),
        "out": "icon_silver.png",
        "role": "silver",
        "size": "96px source",
        "trim_padding": 10,
        "isolate_component": "center",
    },
    {
        "id": "icon_xp",
        "kind": "icon",
        "rect": (846, 996, 142, 152),
        "out": "icon_xp.png",
        "role": "xp",
        "size": "96px source",
        "trim_padding": 10,
        "isolate_component": "center",
    },
    {
        "id": "icon_road_safety",
        "kind": "icon",
        "rect": (974, 996, 144, 166),
        "out": "icon_road_safety.png",
        "role": "road_safety",
        "size": "96px source",
        "trim_padding": 10,
        "isolate_component": "center",
    },
    {
        "id": "icon_rune_spark",
        "kind": "icon",
        "rect": (1096, 986, 144, 176),
        "out": "icon_rune_spark.png",
        "role": "rune_spark",
        "size": "96px source",
        "trim_padding": 10,
        "preserve_purple_edges": True,
        "isolate_component": "center",
    },
]


def is_key_like(red: int, green: int, blue: int) -> bool:
    return red > 115 and blue > 120 and green < 145 and red + blue > 300 and red + blue > green * 3


def key_to_alpha(image: Image.Image, *, preserve_purple_edges: bool = False) -> Image.Image:
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    width, height = rgba.size
    connected = Image.new("L", rgba.size, 0)
    connected_pixels = connected.load()
    queue: deque[tuple[int, int]] = deque()

    def push_if_key(x: int, y: int) -> None:
        if connected_pixels[x, y] != 0:
            return
        red, green, blue, _alpha = pixels[x, y]
        if is_key_like(red, green, blue):
            connected_pixels[x, y] = 255
            queue.append((x, y))

    for x in range(width):
        push_if_key(x, 0)
        push_if_key(x, height - 1)
    for y in range(height):
        push_if_key(0, y)
        push_if_key(width - 1, y)

    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < width and 0 <= ny < height:
                push_if_key(nx, ny)

    expanded = connected.filter(ImageFilter.MaxFilter(5 if preserve_purple_edges else 9))
    expanded_pixels = expanded.load()
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if connected_pixels[x, y] != 0:
                pixels[x, y] = (red, green, blue, 0)
            elif expanded_pixels[x, y] != 0 and is_key_like(red, green, blue):
                pixels[x, y] = (red, green, blue, 0)
            elif expanded_pixels[x, y] != 0 and not preserve_purple_edges:
                # Despill antialiased edge pixels that picked up magenta from the key background.
                spill = max(0, min(red, blue) - green)
                if spill > 24:
                    red = max(green, red - spill // 2)
                    blue = max(green, blue - spill // 2)
                    pixels[x, y] = (red, green, blue, alpha)
    if not preserve_purple_edges:
        remove_edge_fringe(rgba, passes=3)
    return rgba


def remove_edge_fringe(image: Image.Image, passes: int) -> None:
    pixels = image.load()
    width, height = image.size
    for _pass in range(passes):
        alpha = image.getchannel("A")
        alpha_pixels = alpha.load()
        to_clear: list[tuple[int, int]] = []
        for y in range(height):
            for x in range(width):
                red, green, blue, current_alpha = pixels[x, y]
                if current_alpha <= 12 or not is_key_like(red, green, blue):
                    continue
                touches_transparent = False
                for ny in range(max(0, y - 1), min(height, y + 2)):
                    for nx in range(max(0, x - 1), min(width, x + 2)):
                        if alpha_pixels[nx, ny] <= 12:
                            touches_transparent = True
                            break
                    if touches_transparent:
                        break
                if touches_transparent:
                    to_clear.append((x, y))
        if not to_clear:
            return
        for x, y in to_clear:
            red, green, blue, _alpha = pixels[x, y]
            pixels[x, y] = (red, green, blue, 0)


def trim_to_alpha(image: Image.Image, padding: int) -> Image.Image:
    alpha = image.getchannel("A")
    bbox = alpha.point(lambda value: 255 if value > 12 else 0).getbbox()
    if bbox is None:
        return image
    left = max(0, bbox[0] - padding)
    top = max(0, bbox[1] - padding)
    right = min(image.width, bbox[2] + padding)
    bottom = min(image.height, bbox[3] + padding)
    return image.crop((left, top, right, bottom))


def apply_cleanup_patches(image: Image.Image, patches: object) -> Image.Image:
    if not isinstance(patches, list):
        return image
    cleaned = image.copy()
    for patch in patches:
        if not isinstance(patch, dict):
            continue
        target = patch.get("target")
        sample = patch.get("sample")
        if not (
            isinstance(target, tuple)
            and isinstance(sample, tuple)
            and len(target) == 4
            and len(sample) == 4
        ):
            continue
        target_box = tuple(int(value) for value in target)
        sample_box = tuple(int(value) for value in sample)
        target_width = max(1, target_box[2] - target_box[0])
        target_height = max(1, target_box[3] - target_box[1])
        patch_image = image.crop(sample_box).resize((target_width, target_height), Image.Resampling.LANCZOS)
        cleaned.alpha_composite(patch_image, (target_box[0], target_box[1]))
    return cleaned


def draw_decor_gem(size: tuple[int, int], asset_id: str) -> Image.Image:
    width, height = size
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    cx = width // 2
    cy = height // 2
    if "bottom" in asset_id:
        cy = max(height // 3, height // 2 - 4)
    gem_h = max(18, min(height - 8, int(height * 0.62)))
    gem_w = max(14, min(width - 16, int(gem_h * 0.62)))
    wing_w = max(18, min(width // 3, gem_h))
    shadow = Image.new("RGBA", size, (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow)
    shadow_draw.ellipse((cx - wing_w, cy - gem_h // 3 + 6, cx + wing_w, cy + gem_h // 3 + 12), fill=(0, 0, 0, 120))
    image.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(4)))
    gold = (239, 190, 88, 255)
    dark_gold = (92, 54, 20, 255)
    teal = (53, 210, 192, 255)
    teal_dark = (12, 78, 84, 255)
    rim = (255, 229, 142, 245)
    draw.polygon(
        [(cx - gem_w // 2, cy), (cx, cy - gem_h // 2), (cx + gem_w // 2, cy), (cx, cy + gem_h // 2)],
        fill=(18, 43, 42, 255),
        outline=dark_gold,
    )
    draw.polygon(
        [(cx - gem_w // 2 - 3, cy), (cx, cy - gem_h // 2 - 3), (cx + gem_w // 2 + 3, cy), (cx, cy + gem_h // 2 + 3)],
        fill=teal_dark,
        outline=gold,
    )
    draw.polygon(
        [(cx - gem_w // 3, cy), (cx, cy - gem_h // 3), (cx + gem_w // 3, cy), (cx, cy + gem_h // 3)],
        fill=teal,
        outline=(180, 250, 226, 235),
    )
    draw.line((cx - gem_w // 5, cy - gem_h // 5, cx + gem_w // 7, cy - gem_h // 3), fill=(226, 255, 242, 230), width=2)
    for side in (-1, 1):
        x0 = cx + side * (gem_w // 2)
        x1 = cx + side * (gem_w // 2 + wing_w)
        draw.polygon([(x0, cy - 10), (x1, cy - 6), (x1 - side * 8, cy + 8), (x0, cy + 12)], fill=dark_gold, outline=gold)
        draw.polygon([(x0 + side * 5, cy - 6), (x1 - side * 10, cy - 3), (x1 - side * 16, cy + 3), (x0 + side * 5, cy + 6)], fill=(144, 91, 34, 245))
        draw.line((x0, cy - 3, x1 - side * 8, cy + 3), fill=rim, width=2)
    return image


def extract_decorations(image: Image.Image, crop: dict[str, object]) -> list[tuple[str, Image.Image]]:
    decorations = crop.get("decorations")
    if not isinstance(decorations, list):
        return []
    outputs: list[tuple[str, Image.Image]] = []
    for decoration in decorations:
        if not isinstance(decoration, dict):
            continue
        asset_id = decoration.get("id")
        rect = decoration.get("rect")
        if not isinstance(asset_id, str) or not isinstance(rect, tuple) or len(rect) != 4:
            continue
        box = tuple(int(value) for value in rect)
        decor = draw_decor_gem((max(1, box[2] - box[0]), max(1, box[3] - box[1])), asset_id)
        decor = trim_to_alpha(decor, 4)
        outputs.append((asset_id, decor))
    return outputs


def mix(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return (
        int(a[0] + (b[0] - a[0]) * t),
        int(a[1] + (b[1] - a[1]) * t),
        int(a[2] + (b[2] - a[2]) * t),
    )


def draw_rounded_gradient(
    image: Image.Image,
    box: tuple[int, int, int, int],
    radius: int,
    top_color: tuple[int, int, int],
    bottom_color: tuple[int, int, int],
    alpha: int = 255,
) -> None:
    left, top, right, bottom = box
    width = max(1, right - left + 1)
    height = max(1, bottom - top + 1)
    gradient = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    gradient_draw = ImageDraw.Draw(gradient)
    for y in range(height):
        t = y / max(1, height - 1)
        red, green, blue = mix(top_color, bottom_color, t)
        gradient_draw.line((0, y, width, y), fill=(red, green, blue, alpha))
    mask = Image.new("L", (width, height), 0)
    mask_draw = ImageDraw.Draw(mask)
    mask_draw.rounded_rectangle((0, 0, width - 1, height - 1), radius=radius, fill=255)
    image.alpha_composite(Image.composite(gradient, Image.new("RGBA", (width, height), (0, 0, 0, 0)), mask), (left, top))


def draw_corner_caps(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], cap: int, color: tuple[int, int, int, int]) -> None:
    left, top, right, bottom = box
    inset = max(4, cap // 4)
    thickness = max(3, cap // 8)
    for sx, sy in ((left, top), (right, top), (left, bottom), (right, bottom)):
        sign_x = 1 if sx == left else -1
        sign_y = 1 if sy == top else -1
        x0 = sx
        y0 = sy
        x1 = sx + sign_x * cap
        y1 = sy + sign_y * cap
        draw.line((x0, y0 + sign_y * inset, x0 + sign_x * inset, y0, x1, y0), fill=color, width=thickness, joint="curve")
        draw.line((x0, y0 + sign_y * inset, x0, y1, x0 + sign_x * inset, y1), fill=(58, 42, 24, 255), width=max(2, thickness - 1))


def draw_wood_grain(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], strength: int = 1) -> None:
    left, top, right, bottom = box
    for index, y in enumerate(range(top + 10, bottom - 6, 8)):
        color = (120, 74, 35, 30 + (index % 3) * 10)
        draw.line((left + 10, y, right - 10, y + (index % 3) - 1), fill=color, width=strength)
    for index, x in enumerate(range(left + 24, right - 42, 92)):
        color = (18, 12, 9, 14)
        draw.arc((x, top + 18 + (index % 3) * 16, x + 56, top + 54 + (index % 4) * 14), 185, 340, fill=color, width=1)


def draw_edge_inlays(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], color: tuple[int, int, int, int]) -> None:
    left, top, right, bottom = box
    for x in range(left + 64, right - 64, 132):
        draw.line((x, top + 2, x + 34, top + 2), fill=color, width=3)
        draw.line((x, bottom - 2, x + 34, bottom - 2), fill=color, width=3)
    for y in range(top + 72, bottom - 72, 128):
        draw.line((left + 2, y, left + 2, y + 28), fill=(72, 47, 24, 210), width=3)
        draw.line((right - 2, y, right - 2, y + 28), fill=(72, 47, 24, 210), width=3)


def draw_slice9_base(size: tuple[int, int], style: str) -> Image.Image:
    width, height = size
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    if style.startswith("button"):
        pad_x = 12
        pad_y = 6 if style != "button_disabled" else 10
        radius = max(10, min(24, height // 5))
        outer = (pad_x, pad_y, width - pad_x - 1, height - pad_y - 1)
        shadow = (outer[0] + 3, outer[1] + 4, outer[2] + 3, outer[3] + 4)
        draw.rounded_rectangle(shadow, radius=radius, fill=(0, 0, 0, 80))
        if style == "button_disabled":
            frame = (158, 153, 146, 255)
            hi = (78, 76, 78)
            lo = (38, 38, 42)
            gold = (190, 184, 174, 255)
        elif style == "button_pressed":
            frame = (196, 128, 42, 255)
            hi = (73, 42, 19)
            lo = (28, 18, 12)
            gold = (238, 183, 78, 255)
        else:
            frame = (196, 154, 72, 255)
            hi = (68, 43, 22)
            lo = (34, 22, 15)
            gold = (236, 190, 91, 255)
        draw.rounded_rectangle(outer, radius=radius, fill=(25, 20, 17, 255), outline=frame, width=4)
        inner = (outer[0] + 20, outer[1] + 18, outer[2] - 20, outer[3] - 18)
        draw_rounded_gradient(image, inner, max(6, radius - 8), hi, lo)
        draw_wood_grain(draw, inner, strength=1)
        draw.rounded_rectangle((inner[0] - 6, inner[1] - 5, inner[2] + 6, inner[3] + 5), radius=max(8, radius - 5), outline=(92, 68, 38, 210), width=2)
        draw.rounded_rectangle(inner, radius=max(6, radius - 8), outline=(12, 10, 9, 230), width=3)
        draw.line((inner[0] + 8, inner[1] + 4, inner[2] - 8, inner[1] + 4), fill=(238, 190, 96, 95), width=2)
        draw.line((inner[0] + 8, inner[3] - 4, inner[2] - 8, inner[3] - 4), fill=(0, 0, 0, 105), width=2)
        draw_corner_caps(draw, outer, min(54, width // 6), gold)
        draw_edge_inlays(draw, outer, (238, 190, 91, 180))
        return image

    if style == "status_bar":
        pad_x = 12
        pad_y = 14
        outer = (pad_x, pad_y, width - pad_x - 1, height - pad_y - 1)
        draw.rounded_rectangle((outer[0] + 2, outer[1] + 3, outer[2] + 2, outer[3] + 3), radius=height // 4, fill=(0, 0, 0, 90))
        draw.rounded_rectangle(outer, radius=height // 4, fill=(25, 20, 16, 255), outline=(176, 128, 54, 255), width=3)
        inner = (outer[0] + 26, outer[1] + 14, outer[2] - 26, outer[3] - 14)
        draw_rounded_gradient(image, inner, max(4, height // 8), (48, 31, 18), (17, 13, 11))
        draw_wood_grain(draw, inner, strength=1)
        draw.line((outer[0] + 18, outer[1] + 4, outer[2] - 18, outer[1] + 4), fill=(225, 165, 72, 120), width=2)
        draw.line((outer[0] + 18, outer[3] - 4, outer[2] - 18, outer[3] - 4), fill=(0, 0, 0, 130), width=2)
        for cx in (outer[0] + 40, outer[2] - 40):
            cy = (outer[1] + outer[3]) // 2
            draw.line((cx - 10, cy, cx + 10, cy), fill=(211, 167, 78, 180), width=2)
        draw.rectangle((0, height - 6, width, height), fill=(0, 0, 0, 0))
        return image

    pad_x = 14 if style in ("reward_chip", "icon_frame") else 18
    pad_y = 12 if style in ("reward_chip", "icon_frame") else 18
    radius = max(10, min(34, min(width, height) // 10))
    outer = (pad_x, pad_y, width - pad_x - 1, height - pad_y - 1)
    draw.rounded_rectangle((outer[0] + 4, outer[1] + 5, outer[2] + 4, outer[3] + 5), radius=radius, fill=(0, 0, 0, 90))
    draw.rounded_rectangle(outer, radius=radius, fill=(32, 29, 29, 255), outline=(177, 130, 62, 255), width=5)
    draw.rounded_rectangle((outer[0] + 3, outer[1] + 3, outer[2] - 3, outer[3] - 3), radius=max(7, radius - 3), outline=(226, 178, 86, 165), width=2)
    metal = (outer[0] + 9, outer[1] + 9, outer[2] - 9, outer[3] - 9)
    draw.rounded_rectangle(metal, radius=max(8, radius - 5), outline=(99, 96, 102, 255), width=5)
    draw.rounded_rectangle((metal[0] + 4, metal[1] + 4, metal[2] - 4, metal[3] - 4), radius=max(6, radius - 8), outline=(36, 34, 37, 230), width=2)
    inner = (outer[0] + 28, outer[1] + 30, outer[2] - 28, outer[3] - 30)
    draw_rounded_gradient(image, inner, max(6, radius - 12), (73, 45, 25), (30, 19, 13))
    draw_wood_grain(draw, inner, strength=1)
    for y in range(inner[1] + 8, inner[3], 12):
        alpha = 42 if (y // 12) % 2 == 0 else 24
        draw.line((inner[0] + 8, y, inner[2] - 8, y), fill=(138, 86, 41, alpha), width=1)
    draw.rounded_rectangle(inner, radius=max(6, radius - 12), outline=(16, 13, 11, 230), width=3)
    draw.line((inner[0] + 12, inner[1] + 5, inner[2] - 12, inner[1] + 5), fill=(238, 190, 96, 70), width=2)
    draw.line((inner[0] + 12, inner[3] - 5, inner[2] - 12, inner[3] - 5), fill=(0, 0, 0, 105), width=2)
    draw_corner_caps(draw, outer, max(34, min(78, min(width, height) // 4)), (230, 178, 82, 255))
    draw_edge_inlays(draw, outer, (238, 190, 91, 160))
    return image


def isolate_component(image: Image.Image, mode: str) -> Image.Image:
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    width, height = rgba.size
    visited = bytearray(width * height)
    best_component: list[tuple[int, int]] = []
    best_score = -1_000_000_000.0
    center_x = width / 2.0
    center_y = height / 2.0

    def index(x: int, y: int) -> int:
        return y * width + x

    for y in range(height):
        for x in range(width):
            start = index(x, y)
            if visited[start] or pixels[x, y][3] <= 48:
                visited[start] = 1
                continue
            component: list[tuple[int, int]] = []
            queue: deque[tuple[int, int]] = deque([(x, y)])
            visited[start] = 1
            while queue:
                cx, cy = queue.popleft()
                component.append((cx, cy))
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if 0 <= nx < width and 0 <= ny < height:
                        cursor = index(nx, ny)
                        if not visited[cursor]:
                            visited[cursor] = 1
                            if pixels[nx, ny][3] > 48:
                                queue.append((nx, ny))
            if not component:
                continue
            min_x = min(point[0] for point in component)
            max_x = max(point[0] for point in component)
            min_y = min(point[1] for point in component)
            max_y = max(point[1] for point in component)
            comp_center_x = (min_x + max_x) / 2.0
            comp_center_y = (min_y + max_y) / 2.0
            distance = abs(comp_center_x - center_x) + abs(comp_center_y - center_y)
            score = float(len(component))
            if mode == "center":
                score -= distance * 200.0
            if score > best_score:
                best_score = score
                best_component = component

    if not best_component:
        return rgba

    mask = Image.new("L", rgba.size, 0)
    mask_pixels = mask.load()
    for x, y in best_component:
        mask_pixels[x, y] = 255
    mask = mask.filter(ImageFilter.MaxFilter(9))
    mask_pixels = mask.load()
    for y in range(height):
        for x in range(width):
            if mask_pixels[x, y] == 0:
                red, green, blue, _alpha = pixels[x, y]
                pixels[x, y] = (red, green, blue, 0)
    return rgba


def extract_raw_asset(source: Image.Image, crop: dict[str, object]) -> Image.Image:
    x, y, width, height = crop["rect"]
    overscan = crop.get("overscan", {})
    if isinstance(overscan, dict):
        left = int(overscan.get("left", 0))
        top = int(overscan.get("top", 0))
        right = int(overscan.get("right", 0))
        bottom = int(overscan.get("bottom", 0))
    else:
        left = top = right = bottom = 0
    x0 = max(0, x - left)
    y0 = max(0, y - top)
    x1 = min(source.width, x + width + right)
    y1 = min(source.height, y + height + bottom)
    image = key_to_alpha(
        source.crop((x0, y0, x1, y1)),
        preserve_purple_edges=bool(crop.get("preserve_purple_edges")),
    )
    return image


def extract_asset(source: Image.Image, crop: dict[str, object]) -> Image.Image:
    image = extract_raw_asset(source, crop)
    base_style = crop.get("base_style")
    if isinstance(base_style, str):
        return draw_slice9_base(image.size, base_style)
    image = apply_cleanup_patches(image, crop.get("cleanup_patches"))
    isolate_mode = crop.get("isolate_component")
    if isinstance(isolate_mode, str):
        image = isolate_component(image, isolate_mode)
    padding = crop.get("trim_padding")
    if isinstance(padding, int):
        image = trim_to_alpha(image, padding)
    return image


def nine_slice_resize(image: Image.Image, margins: dict[str, int], size: tuple[int, int]) -> Image.Image:
    left = margins["left"]
    top = margins["top"]
    right = margins["right"]
    bottom = margins["bottom"]
    width, height = image.size
    out_width, out_height = size
    result = Image.new("RGBA", size, (0, 0, 0, 0))

    src_x = [0, left, width - right, width]
    src_y = [0, top, height - bottom, height]
    dst_x = [0, left, out_width - right, out_width]
    dst_y = [0, top, out_height - bottom, out_height]

    for row in range(3):
        for col in range(3):
            src_box = (src_x[col], src_y[row], src_x[col + 1], src_y[row + 1])
            dst_box = (dst_x[col], dst_y[row], dst_x[col + 1], dst_y[row + 1])
            tile = image.crop(src_box)
            dst_w = max(1, dst_box[2] - dst_box[0])
            dst_h = max(1, dst_box[3] - dst_box[1])
            if tile.size != (dst_w, dst_h):
                tile = tile.resize((dst_w, dst_h), Image.Resampling.LANCZOS)
            result.alpha_composite(tile, (dst_box[0], dst_box[1]))
    return result


def effective_slice9(crop: dict[str, object]) -> dict[str, int]:
    margins = crop["slice9"]
    return {
        "left": int(margins["left"]),
        "top": int(margins["top"]),
        "right": int(margins["right"]),
        "bottom": int(margins["bottom"]),
    }


def label(draw: ImageDraw.ImageDraw, xy: tuple[int, int], text: str) -> None:
    try:
        font = ImageFont.truetype("arial.ttf", 14)
    except OSError:
        font = ImageFont.load_default()
    draw.text(xy, text, fill=(235, 230, 210, 255), font=font)


def make_contact_sheet(crops: list[tuple[str, Image.Image]]) -> Image.Image:
    cell_w, cell_h = 220, 170
    cols = 4
    rows = (len(crops) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * cell_w, rows * cell_h), (24, 22, 25, 255))
    draw = ImageDraw.Draw(sheet)
    for index, (asset_id, image) in enumerate(crops):
        col = index % cols
        row = index // cols
        x = col * cell_w
        y = row * cell_h
        draw.rectangle((x + 4, y + 4, x + cell_w - 4, y + cell_h - 4), outline=(86, 80, 66, 255))
        preview = image.copy()
        preview.thumbnail((cell_w - 24, cell_h - 40), Image.Resampling.LANCZOS)
        sheet.alpha_composite(preview, (x + (cell_w - preview.width) // 2, y + 20))
        label(draw, (x + 10, y + cell_h - 18), asset_id)
    return sheet


def make_slice_preview(images_by_id: dict[str, Image.Image]) -> Image.Image:
    rows = []
    for crop in CROPS:
        if crop["kind"] != "slice9":
            continue
        image = images_by_id[crop["id"]]
        variants = [nine_slice_resize(image, effective_slice9(crop), size) for size in crop["preview"]]
        row_w = sum(item.width for item in variants) + 20 * (len(variants) + 1)
        row_h = max(item.height for item in variants) + 44
        row = Image.new("RGBA", (row_w, row_h), (24, 22, 25, 255))
        draw = ImageDraw.Draw(row)
        label(draw, (12, 6), crop["id"])
        x = 20
        for item in variants:
            row.alpha_composite(item, (x, 34))
            x += item.width + 20
        rows.append(row)

    width = max(row.width for row in rows)
    height = sum(row.height for row in rows)
    sheet = Image.new("RGBA", (width, height), (24, 22, 25, 255))
    y = 0
    for row in rows:
        sheet.alpha_composite(row, (0, y))
        y += row.height
    return sheet


def main() -> None:
    if not SOURCE.exists():
        raise SystemExit(f"missing source sheet: {SOURCE}")
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)

    source = Image.open(SOURCE)
    outputs = []
    images_by_id = {}
    for crop in CROPS:
        raw_image = extract_raw_asset(source, crop)
        for decoration_id, decoration_image in extract_decorations(raw_image, crop):
            decoration_out = f"{decoration_id}.png"
            decoration_image.save(OUT_DIR / decoration_out)
            outputs.append((decoration_id, decoration_image))
        image = extract_asset(source, crop)
        image.save(OUT_DIR / crop["out"])
        outputs.append((crop["id"], image))
        images_by_id[crop["id"]] = image

    make_contact_sheet(outputs).save(PREVIEW_DIR / "rune-marches-ui-kit-v2-contact-sheet.png")
    make_slice_preview(images_by_id).save(PREVIEW_DIR / "rune-marches-ui-kit-v2-slice9-preview.png")
    print(f"wrote {len(outputs)} assets to {OUT_DIR.relative_to(ROOT)}")
    print(f"wrote previews to {PREVIEW_DIR.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
