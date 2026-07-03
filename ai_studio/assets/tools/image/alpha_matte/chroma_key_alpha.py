#!/usr/bin/env python3
from __future__ import annotations

from typing import Any

import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

from ai_studio.assets.tools.lib.color import key_distance

RGB = tuple[int, int, int]
NUMPY_FAST_PATH_MIN_PIXELS = 256 * 256


def is_exact_key_like(red: int, green: int, blue: int, key: RGB = (255, 0, 255), tolerance: int = 10) -> bool:
    return bool(key_distance((red, green, blue), key) <= tolerance)


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


def is_green_screen_spill_like(red: int, green: int, blue: int) -> bool:
    return green > 100 and green > red * 1.35 and green > blue * 1.35 and green - max(red, blue) > 28


def is_muted_green_key_spill_like(red: int, green: int, blue: int) -> bool:
    return green >= 55 and blue <= 32 and green - blue >= 40 and green - red >= 18


def is_source_key_spill_like(red: int, green: int, blue: int, key: RGB) -> bool:
    key_red, key_green, key_blue = key
    if key_green > 220 and key_red < 40 and key_blue < 40:
        saturated = green > 90 and green > red * 1.25 and green > blue * 1.25 and green - max(red, blue) > 22
        return saturated or is_muted_green_key_spill_like(red, green, blue)
    if key_green > 220 and key_blue > 220 and key_red < 40:
        return green > 70 and blue > 70 and green > red * 1.25 and blue > red * 1.25 and min(green, blue) - red > 26 and abs(green - blue) < 96
    if key_red > 220 and key_blue > 220 and key_green < 40:
        return is_exact_key_like(red, green, blue, key=key, tolerance=36)
    if key_red > 220 and key_green < 40 and key_blue < 40:
        return red > 90 and red > green * 1.25 and red > blue * 1.25 and red - max(green, blue) > 22
    if key_blue > 220 and key_red < 40 and key_green < 40:
        return blue > 90 and blue > red * 1.25 and blue > green * 1.25 and blue - max(red, green) > 22
    return is_exact_key_like(red, green, blue, key=key, tolerance=36)


def key_fringe_mask_rgb(red: Any, green: Any, blue: Any) -> Any:
    """Vectorized counterpart of ``is_key_fringe_like`` over int16 r/g/b arrays."""
    return (red > 115) & (blue > 120) & (green < 145) & (red + blue > 300) & (red + blue > green * 3)


def purple_halo_only_mask_rgb(red: Any, green: Any, blue: Any) -> Any:
    """Vectorized counterpart of the single ``is_purple_halo_like`` clause."""
    return (red > 75) & (blue > 75) & (green < 120) & (np.minimum(red, blue) - green > 20) & (red + blue > green * 2 + 80)


def any_purple_halo_mask_rgb(red: Any, green: Any, blue: Any) -> Any:
    """Vectorized counterpart of ``is_any_purple_halo_like`` (purple+dark+magenta+dark_magenta)."""
    purple = purple_halo_only_mask_rgb(red, green, blue)
    dark_purple = (
        (red >= 32)
        & (blue >= 32)
        & ((green < np.minimum(red, blue) * 0.55) | (green <= 12))
        & (np.abs(red - blue) < 64)
        & (red + blue > green * 3 + 38)
    )
    magenta = (red > 80) & (blue > 45) & (green < 120) & (red > green + 32) & (blue > green + 6)
    dark_magenta = (red > 44) & (blue > 34) & (green < 42) & (red > green + 24) & (blue > green + 14) & (red + blue > green * 2 + 48)
    return purple | dark_purple | magenta | dark_magenta


def green_screen_spill_mask_rgb(red: Any, green: Any, blue: Any) -> Any:
    """Vectorized counterpart of ``is_green_screen_spill_like`` over int16 r/g/b arrays."""
    return (green > 100) & (green > red * 1.35) & (green > blue * 1.35) & (green - np.maximum(red, blue) > 28)


def source_key_spill_mask(red: Any, green: Any, blue: Any, key: RGB, magenta_tolerance: int = 36) -> Any:
    key_red, key_green, key_blue = key
    if key_green > 220 and key_red < 40 and key_blue < 40:
        saturated = (green > 90) & (green > red * 1.25) & (green > blue * 1.25) & (green - np.maximum(red, blue) > 22)
        muted = (green >= 55) & (blue <= 32) & (green - blue >= 40) & (green - red >= 18)
        return saturated | muted
    if key_green > 220 and key_blue > 220 and key_red < 40:
        return (
            (green > 70)
            & (blue > 70)
            & (green > red * 1.25)
            & (blue > red * 1.25)
            & (np.minimum(green, blue) - red > 26)
            & (np.abs(green - blue) < 96)
        )
    if key_red > 220 and key_blue > 220 and key_green < 40:
        return np.maximum.reduce((np.abs(red - key_red), np.abs(green - key_green), np.abs(blue - key_blue))) <= magenta_tolerance
    if key_red > 220 and key_green < 40 and key_blue < 40:
        return (red > 90) & (red > green * 1.25) & (red > blue * 1.25) & (red - np.maximum(green, blue) > 22)
    if key_blue > 220 and key_red < 40 and key_green < 40:
        return (blue > 90) & (blue > red * 1.25) & (blue > green * 1.25) & (blue - np.maximum(red, green) > 22)
    return np.maximum.reduce((np.abs(red - key_red), np.abs(green - key_green), np.abs(blue - key_blue))) <= magenta_tolerance


def bad_edge_rgb_mask_array(array: Any, key: RGB | None) -> Any:
    red = array[..., 0].astype(np.int16)
    green = array[..., 1].astype(np.int16)
    blue = array[..., 2].astype(np.int16)
    key_fringe = key_fringe_mask_rgb(red, green, blue)
    purple_halo = any_purple_halo_mask_rgb(red, green, blue)
    green_spill = green_screen_spill_mask_rgb(red, green, blue)
    source_key = np.zeros(red.shape, dtype=bool)
    if key is not None:
        source_key = source_key_spill_mask(red, green, blue, key)
    return key_fringe | purple_halo | green_spill | source_key


def dilate_bool_mask(mask: Any, radius: int) -> Any:
    if radius <= 0:
        return mask.copy()
    # Square (Chebyshev) dilation: one scipy pass with a (2r+1)^2 structuring
    # element replaces the O((2r+1)^2) python OR double-loop, identical result.
    structure = np.ones((radius * 2 + 1, radius * 2 + 1), dtype=bool)
    return ndimage.binary_dilation(mask, structure=structure)


def box_sum(values: Any, radius: int) -> Any:
    if values.ndim == 2:
        padded = np.pad(values, ((radius, radius), (radius, radius)), mode="constant", constant_values=0)
        integral = np.pad(padded, ((1, 0), (1, 0)), mode="constant", constant_values=0).cumsum(axis=0).cumsum(axis=1)
        diameter = radius * 2 + 1
        return integral[diameter:, diameter:] - integral[:-diameter, diameter:] - integral[diameter:, :-diameter] + integral[:-diameter, :-diameter]
    padded = np.pad(values, ((radius, radius), (radius, radius), (0, 0)), mode="constant", constant_values=0)
    integral = np.pad(padded, ((1, 0), (1, 0), (0, 0)), mode="constant", constant_values=0).cumsum(axis=0).cumsum(axis=1)
    diameter = radius * 2 + 1
    return integral[diameter:, diameter:] - integral[:-diameter, diameter:] - integral[diameter:, :-diameter] + integral[:-diameter, :-diameter]


def decontaminate_source_key_spill_numpy(
    array: Any,
    *,
    key: RGB,
    aggressive_visible_decontaminate: bool,
    radius: int = 6,
) -> None:
    alpha = array[..., 3]
    visible = alpha > 12
    red = array[..., 0].astype(np.int16)
    green = array[..., 1].astype(np.int16)
    blue = array[..., 2].astype(np.int16)
    spill = source_key_spill_mask(red, green, blue, key)
    if aggressive_visible_decontaminate:
        touch = np.ones(alpha.shape, dtype=bool)
    else:
        touch = dilate_bool_mask(alpha <= 12, radius)
    update = visible & spill & touch
    if not np.any(update):
        return

    bad = spill | key_fringe_mask_rgb(red, green, blue) | green_screen_spill_mask_rgb(red, green, blue) | any_purple_halo_mask_rgb(red, green, blue)
    source = visible & (~bad)
    count = box_sum(source.astype(np.uint16), radius)
    total = box_sum(array[..., :3].astype(np.uint32) * source[..., None], radius)
    neighbor_update = update & (count > 0)
    if np.any(neighbor_update):
        array[..., :3][neighbor_update] = (total[neighbor_update] // count[neighbor_update, None]).astype(np.uint8)
    fallback_update = update & (count == 0)
    if np.any(fallback_update):
        cyan_amount = np.maximum(0, np.minimum(green, blue) - red)
        repaired_green = np.maximum(red, green - cyan_amount)
        repaired_blue = np.maximum(red, blue - cyan_amount)
        array[..., 1][fallback_update] = np.clip(repaired_green[fallback_update], 0, 255).astype(np.uint8)
        array[..., 2][fallback_update] = np.clip(repaired_blue[fallback_update], 0, 255).astype(np.uint8)


def decontaminate_source_key_spill_image(
    image: Image.Image,
    *,
    key: RGB,
    require_transparent_touch: bool = True,
    radius: int = 6,
) -> None:
    array = np.array(image.convert("RGBA"), dtype=np.uint8)
    decontaminate_source_key_spill_numpy(
        array,
        key=key,
        aggressive_visible_decontaminate=not require_transparent_touch,
        radius=radius,
    )
    image.paste(Image.fromarray(array))


def bleed_transparent_rgb_numpy(image: Image.Image, passes: int = 16, key: RGB | None = None) -> None:
    array = np.array(image.convert("RGBA"), dtype=np.uint8)
    if array.size == 0:
        return
    for _pass in range(passes):
        alpha = array[..., 3]
        transparent = alpha <= 12
        bad = bad_edge_rgb_mask_array(array, key)
        source = (~transparent) | (~bad)
        padded_rgb = np.pad(array[..., :3].astype(np.uint32), ((1, 1), (1, 1), (0, 0)), mode="edge")
        padded_source = np.pad(source, ((1, 1), (1, 1)), mode="constant", constant_values=False)
        total = np.zeros(array[..., :3].shape, dtype=np.uint32)
        count = np.zeros(alpha.shape, dtype=np.uint16)
        for dy, dx in ((-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)):
            y0 = 1 + dy
            x0 = 1 + dx
            neighbor_source = padded_source[y0 : y0 + alpha.shape[0], x0 : x0 + alpha.shape[1]]
            neighbor_rgb = padded_rgb[y0 : y0 + alpha.shape[0], x0 : x0 + alpha.shape[1]]
            total += neighbor_rgb * neighbor_source[..., None]
            count += neighbor_source.astype(np.uint16)
        update = transparent & (count > 0)
        if not np.any(update):
            break
        array[..., :3][update] = (total[update] // count[update, None]).astype(np.uint8)
        array[..., 3][update] = 0
    image.paste(Image.fromarray(array))


def bleed_transparent_rgb(image: Image.Image, passes: int = 16, key: RGB | None = None) -> None:
    bleed_transparent_rgb_numpy(image, passes=passes, key=key)


def zero_fully_transparent_rgb(image: Image.Image) -> None:
    array = np.array(image.convert("RGBA"), dtype=np.uint8)
    transparent = array[..., 3] == 0
    if np.any(transparent):
        array[..., :3][transparent] = 0
        image.paste(Image.fromarray(array))


def repair_transparent_edge_rgb(image: Image.Image, radius: int = 4, key: RGB | None = None) -> None:
    array = np.array(image.convert("RGBA"), dtype=np.uint8)
    if array.size == 0:
        return
    alpha = array[..., 3]
    transparent = alpha <= 12
    bad = bad_edge_rgb_mask_array(array, key)
    update = transparent & bad
    if not np.any(update):
        return

    visible = alpha > 12
    count = box_sum(visible.astype(np.uint16), radius)
    total = box_sum(array[..., :3].astype(np.uint32) * visible[..., None], radius)
    update &= count > 0
    if np.any(update):
        array[..., :3][update] = (total[update] // count[update, None]).astype(np.uint8)
        array[..., 3][update] = 0
        image.paste(Image.fromarray(array))


def resize_rgba_premultiplied(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    rgba = image.convert("RGBA")
    array = np.array(rgba, dtype=np.uint16)
    alpha = array[..., 3:4]
    array[..., :3] = (array[..., :3] * alpha) // 255
    premultiplied = Image.fromarray(array.astype(np.uint8))
    resized = premultiplied.resize(size, Image.Resampling.LANCZOS)
    resized_array = np.array(resized, dtype=np.uint16)
    resized_alpha = resized_array[..., 3]
    visible = resized_alpha > 0
    for channel in range(3):
        values = resized_array[..., channel]
        values[visible] = np.minimum(255, (values[visible] * 255) // resized_alpha[visible])
    resized = Image.fromarray(resized_array.astype(np.uint8))
    bleed_transparent_rgb(resized, passes=4)
    repair_transparent_edge_rgb(resized)
    zero_fully_transparent_rgb(resized)
    return resized
