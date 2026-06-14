#!/usr/bin/env python3
from __future__ import annotations

from collections import deque
from typing import Any

from PIL import Image, ImageFilter


RGB = tuple[int, int, int]


def is_exact_key_like(red: int, green: int, blue: int, key: RGB = (255, 0, 255), tolerance: int = 10) -> bool:
    return max(abs(red - key[0]), abs(green - key[1]), abs(blue - key[2])) <= tolerance


def is_key_fringe_like(red: int, green: int, blue: int) -> bool:
    return red > 115 and blue > 120 and green < 145 and red + blue > 300 and red + blue > green * 3


def is_purple_halo_like(red: int, green: int, blue: int) -> bool:
    return red > 75 and blue > 75 and green < 120 and min(red, blue) - green > 20 and red + blue > green * 2 + 80


def is_dark_purple_halo_like(red: int, green: int, blue: int) -> bool:
    return (
        red >= 32
        and blue >= 32
        and (green < min(red, blue) * 0.55 or green <= 12)
        and abs(red - blue) < 64
        and red + blue > green * 3 + 38
    )


def is_magenta_edge_spill_like(red: int, green: int, blue: int) -> bool:
    return red > 80 and blue > 45 and green < 120 and red > green + 32 and blue > green + 6


def is_dark_magenta_edge_spill_like(red: int, green: int, blue: int) -> bool:
    return (
        red > 44
        and blue > 34
        and green < 42
        and red > green + 24
        and blue > green + 14
        and red + blue > green * 2 + 48
    )


def is_any_purple_halo_like(red: int, green: int, blue: int) -> bool:
    return (
        is_purple_halo_like(red, green, blue)
        or is_dark_purple_halo_like(red, green, blue)
        or is_magenta_edge_spill_like(red, green, blue)
        or is_dark_magenta_edge_spill_like(red, green, blue)
    )


def is_source_key_spill_like(red: int, green: int, blue: int, key: RGB) -> bool:
    key_red, key_green, key_blue = key
    if key_green > 220 and key_red < 40 and key_blue < 40:
        return green > 90 and green > red * 1.25 and green > blue * 1.25 and green - max(red, blue) > 22
    if key_red > 220 and key_blue > 220 and key_green < 40:
        return is_exact_key_like(red, green, blue, key=key, tolerance=36)
    if key_red > 220 and key_green < 40 and key_blue < 40:
        return red > 90 and red > green * 1.25 and red > blue * 1.25 and red - max(green, blue) > 22
    if key_blue > 220 and key_red < 40 and key_green < 40:
        return blue > 90 and blue > red * 1.25 and blue > green * 1.25 and blue - max(red, green) > 22
    return is_exact_key_like(red, green, blue, key=key, tolerance=36)


def touches_alpha(alpha_pixels: Any, x: int, y: int, width: int, height: int, radius: int, *, visible: bool) -> bool:
    for ny in range(max(0, y - radius), min(height, y + radius + 1)):
        for nx in range(max(0, x - radius), min(width, x + radius + 1)):
            if (alpha_pixels[nx, ny] > 12) == visible:
                return True
    return False


def remove_edge_fringe(image: Image.Image, passes: int = 3) -> None:
    pixels = image.load()
    width, height = image.size
    for _pass in range(passes):
        alpha = image.getchannel("A")
        alpha_pixels = alpha.load()
        to_clear: list[tuple[int, int]] = []
        for y in range(height):
            for x in range(width):
                red, green, blue, current_alpha = pixels[x, y]
                if current_alpha <= 12:
                    continue
                key_fringe = is_key_fringe_like(red, green, blue) and touches_alpha(
                    alpha_pixels, x, y, width, height, 1, visible=False
                )
                purple_halo = is_purple_halo_like(red, green, blue) and touches_alpha(
                    alpha_pixels, x, y, width, height, 2, visible=False
                )
                if key_fringe or purple_halo:
                    to_clear.append((x, y))
        if not to_clear:
            return
        for x, y in to_clear:
            red, green, blue, _alpha = pixels[x, y]
            pixels[x, y] = (red, green, blue, 0)


def repair_visible_halo(
    image: Image.Image,
    *,
    radius: int = 6,
    require_transparent_touch: bool = True,
) -> None:
    pixels = image.load()
    alpha = image.getchannel("A")
    alpha_pixels = alpha.load()
    width, height = image.size
    updates: list[tuple[int, int, int, int, int, int]] = []
    for y in range(height):
        for x in range(width):
            red, green, blue, current_alpha = pixels[x, y]
            if current_alpha <= 12 or not is_any_purple_halo_like(red, green, blue):
                continue
            if require_transparent_touch and not touches_alpha(alpha_pixels, x, y, width, height, radius, visible=False):
                continue
            neighbors: list[RGB] = []
            for ny in range(max(0, y - radius), min(height, y + radius + 1)):
                for nx in range(max(0, x - radius), min(width, x + radius + 1)):
                    nr, ng, nb, na = pixels[nx, ny]
                    if na > 12 and not is_any_purple_halo_like(nr, ng, nb) and not is_key_fringe_like(nr, ng, nb):
                        neighbors.append((nr, ng, nb))
            if neighbors:
                updates.append(
                    (
                        x,
                        y,
                        sum(item[0] for item in neighbors) // len(neighbors),
                        sum(item[1] for item in neighbors) // len(neighbors),
                        sum(item[2] for item in neighbors) // len(neighbors),
                        current_alpha,
                    )
                )
    for x, y, red, green, blue, alpha in updates:
        pixels[x, y] = (red, green, blue, alpha)


def remove_source_key_spill(image: Image.Image, key: RGB, passes: int = 3, radius: int = 2) -> None:
    pixels = image.load()
    width, height = image.size
    for _pass in range(passes):
        alpha = image.getchannel("A")
        alpha_pixels = alpha.load()
        to_clear: list[tuple[int, int]] = []
        for y in range(height):
            for x in range(width):
                red, green, blue, current_alpha = pixels[x, y]
                if current_alpha <= 12 or not is_source_key_spill_like(red, green, blue, key):
                    continue
                if touches_alpha(alpha_pixels, x, y, width, height, radius, visible=False):
                    to_clear.append((x, y))
        if not to_clear:
            return
        for x, y in to_clear:
            red, green, blue, _alpha = pixels[x, y]
            pixels[x, y] = (red, green, blue, 0)


def bleed_transparent_rgb(image: Image.Image, passes: int = 16, key: RGB | None = None) -> None:
    pixels = image.load()
    width, height = image.size
    for _pass in range(passes):
        alpha = image.getchannel("A")
        alpha_pixels = alpha.load()
        updates: list[tuple[int, int, int, int, int]] = []
        for y in range(height):
            for x in range(width):
                if alpha_pixels[x, y] > 12:
                    continue
                neighbors: list[RGB] = []
                for ny in range(max(0, y - 1), min(height, y + 2)):
                    for nx in range(max(0, x - 1), min(width, x + 2)):
                        if nx == x and ny == y:
                            continue
                        red, green, blue, neighbor_alpha = pixels[nx, ny]
                        source_key_bad = key is not None and is_source_key_spill_like(red, green, blue, key)
                        if neighbor_alpha > 12 or not (
                            source_key_bad or is_key_fringe_like(red, green, blue) or is_any_purple_halo_like(red, green, blue)
                        ):
                            neighbors.append((red, green, blue))
                if neighbors:
                    updates.append(
                        (
                            x,
                            y,
                            sum(item[0] for item in neighbors) // len(neighbors),
                            sum(item[1] for item in neighbors) // len(neighbors),
                            sum(item[2] for item in neighbors) // len(neighbors),
                        )
                    )
        if not updates:
            return
        for x, y, red, green, blue in updates:
            pixels[x, y] = (red, green, blue, 0)


def repair_transparent_edge_rgb(image: Image.Image, radius: int = 4, key: RGB | None = None) -> None:
    pixels = image.load()
    alpha = image.getchannel("A")
    alpha_pixels = alpha.load()
    width, height = image.size
    updates: list[tuple[int, int, int, int, int]] = []
    for y in range(height):
        for x in range(width):
            red, green, blue, current_alpha = pixels[x, y]
            source_key_bad = key is not None and is_source_key_spill_like(red, green, blue, key)
            if current_alpha > 12 or not (source_key_bad or is_key_fringe_like(red, green, blue) or is_any_purple_halo_like(red, green, blue)):
                continue
            if not touches_alpha(alpha_pixels, x, y, width, height, 0, visible=False):
                continue
            neighbors: list[RGB] = []
            for ny in range(max(0, y - radius), min(height, y + radius + 1)):
                for nx in range(max(0, x - radius), min(width, x + radius + 1)):
                    if alpha_pixels[nx, ny] > 12:
                        nr, ng, nb, _na = pixels[nx, ny]
                        neighbors.append((nr, ng, nb))
            if neighbors:
                updates.append(
                    (
                        x,
                        y,
                        sum(item[0] for item in neighbors) // len(neighbors),
                        sum(item[1] for item in neighbors) // len(neighbors),
                        sum(item[2] for item in neighbors) // len(neighbors),
                    )
                )
    for x, y, red, green, blue in updates:
        pixels[x, y] = (red, green, blue, 0)


def key_to_alpha(
    image: Image.Image,
    *,
    key: RGB = (255, 0, 255),
    exact_tolerance: int = 10,
    edge_tolerance: int = 24,
    aggressive_visible_decontaminate: bool = False,
) -> Image.Image:
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
        if is_exact_key_like(red, green, blue, key=key, tolerance=exact_tolerance):
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

    edge_mask = connected.filter(ImageFilter.MaxFilter(5))
    edge_pixels = edge_mask.load()
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if connected_pixels[x, y]:
                pixels[x, y] = (red, green, blue, 0)
            elif edge_pixels[x, y] and is_exact_key_like(red, green, blue, key=key, tolerance=edge_tolerance):
                pixels[x, y] = (red, green, blue, 0)
            elif edge_pixels[x, y]:
                spill = min(red, blue) - green
                if spill > 28:
                    red = max(green, red - spill // 2)
                    blue = max(green, blue - spill // 2)
                    pixels[x, y] = (red, green, blue, alpha)
    remove_edge_fringe(rgba)
    remove_source_key_spill(rgba, key)
    repair_visible_halo(rgba, require_transparent_touch=not aggressive_visible_decontaminate)
    bleed_transparent_rgb(rgba, key=key)
    repair_transparent_edge_rgb(rgba, key=key)
    return rgba


def resize_rgba_premultiplied(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    width, height = rgba.size
    premultiplied = Image.new("RGBA", rgba.size, (0, 0, 0, 0))
    premultiplied_pixels = premultiplied.load()
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            premultiplied_pixels[x, y] = (
                red * alpha // 255,
                green * alpha // 255,
                blue * alpha // 255,
                alpha,
            )
    resized = premultiplied.resize(size, Image.Resampling.LANCZOS)
    resized_pixels = resized.load()
    for y in range(resized.height):
        for x in range(resized.width):
            red, green, blue, alpha = resized_pixels[x, y]
            if alpha > 0:
                resized_pixels[x, y] = (
                    min(255, red * 255 // alpha),
                    min(255, green * 255 // alpha),
                    min(255, blue * 255 // alpha),
                    alpha,
                )
    bleed_transparent_rgb(resized, passes=4)
    repair_transparent_edge_rgb(resized)
    return resized
