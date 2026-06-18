#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from time import perf_counter
from typing import Any, Callable

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

try:
    import numpy as np
except ImportError:  # pragma: no cover - fallback mode still runs non-soft modes.
    np = None

from tools.assets.atomic_io import save_image_atomic, write_json_atomic, write_text_atomic
from tools.assets.chroma_key_alpha import (
    bleed_transparent_rgb,
    decontaminate_source_key_spill_image,
    green_screen_spill_mask_rgb,
    key_to_alpha,
    repair_transparent_edge_rgb,
    source_key_spill_mask,
    zero_fully_transparent_rgb,
)
from tools.assets.dual_plate_alpha import extract_dual_plate_alpha

RGB = tuple[int, int, int]

BACKGROUNDS: list[tuple[str, RGB]] = [
    ("dark", (28, 30, 36)),
    ("light", (226, 218, 196)),
    ("warm", (114, 78, 44)),
]


def project_path(value: str | Path) -> Path:
    path = Path(value)
    return path if path.is_absolute() else ROOT / path


def rel(path: Path) -> str:
    try:
        return path.resolve().relative_to(ROOT.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def parse_color(value: str) -> RGB:
    text = value.strip().lstrip("#")
    if len(text) != 6:
        raise ValueError(f"expected #rrggbb, got {value!r}")
    return (int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16))


def format_color(value: RGB) -> str:
    return "#{:02x}{:02x}{:02x}".format(*value)


def font(size: int) -> ImageFont.ImageFont:
    for name in ("DejaVuSans.ttf", "Arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def checkerboard(size: tuple[int, int], cell: int = 14) -> Image.Image:
    image = Image.new("RGBA", size, (42, 42, 46, 255))
    draw = ImageDraw.Draw(image)
    for y in range(0, size[1], cell):
        for x in range(0, size[0], cell):
            if (x // cell + y // cell) % 2 == 0:
                draw.rectangle((x, y, x + cell - 1, y + cell - 1), fill=(64, 64, 70, 255))
    return image


def contain(image: Image.Image, max_size: tuple[int, int]) -> Image.Image:
    rgba = image.convert("RGBA")
    scale = min(max_size[0] / rgba.width, max_size[1] / rgba.height, 1.0)
    size = (max(1, round(rgba.width * scale)), max(1, round(rgba.height * scale)))
    if size == rgba.size:
        return rgba
    return rgba.resize(size, Image.Resampling.LANCZOS)


def composite_on(image: Image.Image, bg: RGB, size: tuple[int, int]) -> Image.Image:
    canvas = Image.new("RGBA", size, (*bg, 255))
    preview = contain(image, (size[0] - 12, size[1] - 12))
    canvas.alpha_composite(preview, ((size[0] - preview.width) // 2, (size[1] - preview.height) // 2))
    return canvas


def flatten_on_background(image: Image.Image, bg: RGB) -> Image.Image:
    rgba = image.convert("RGBA")
    canvas = Image.new("RGBA", rgba.size, (*bg, 255))
    canvas.alpha_composite(rgba)
    return canvas


def add_drop_shadow(
    image: Image.Image,
    *,
    offset: tuple[int, int] = (18, 24),
    blur: int = 18,
    opacity: int = 96,
    padding: int = 34,
) -> Image.Image:
    rgba = image.convert("RGBA")
    canvas_size = (rgba.width + padding * 2 + abs(offset[0]), rgba.height + padding * 2 + abs(offset[1]))
    base_x = padding + max(0, -offset[0])
    base_y = padding + max(0, -offset[1])
    shadow_x = base_x + offset[0]
    shadow_y = base_y + offset[1]

    shadow_alpha = rgba.getchannel("A").filter(ImageFilter.GaussianBlur(blur))
    shadow = Image.new("RGBA", canvas_size, (0, 0, 0, 0))
    shadow_pixels = Image.new("RGBA", rgba.size, (0, 0, 0, opacity))
    shadow_pixels.putalpha(shadow_alpha.point(lambda value: min(opacity, value * opacity // 255)))
    shadow.alpha_composite(shadow_pixels, (shadow_x, shadow_y))
    shadow.alpha_composite(rgba, (base_x, base_y))
    return shadow


def make_procedural_sign_truth() -> Image.Image:
    image = Image.new("RGBA", (360, 260), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    for index, y in enumerate((76, 118, 160)):
        base = (120 + index * 18, 72 + index * 11, 34 + index * 7, 255)
        highlight = (184 + index * 10, 118 + index * 8, 58 + index * 5, 255)
        draw.rounded_rectangle((64, y, 296, y + 46), radius=10, fill=base, outline=(58, 35, 18, 255), width=4)
        draw.line((78, y + 11, 282, y + 7), fill=highlight, width=3)
        draw.line((80, y + 34, 286, y + 31), fill=(70, 42, 20, 210), width=2)
        for knot_x in (112 + index * 28, 224 - index * 12):
            draw.ellipse((knot_x - 10, y + 16, knot_x + 12, y + 31), fill=(78, 42, 18, 190))
            draw.arc((knot_x - 11, y + 14, knot_x + 14, y + 34), 10, 310, fill=(205, 132, 66, 155), width=2)

    draw.polygon(((86, 118), (62, 140), (86, 162)), fill=(72, 42, 22, 255), outline=(44, 25, 13, 255))
    draw.polygon(((274, 118), (300, 140), (274, 162)), fill=(72, 42, 22, 255), outline=(44, 25, 13, 255))
    for x in (88, 272):
        draw.ellipse((x - 7, 92, x + 7, 106), fill=(38, 34, 28, 255), outline=(210, 180, 112, 255), width=2)
        draw.ellipse((x - 7, 178, x + 7, 192), fill=(38, 34, 28, 255), outline=(210, 180, 112, 255), width=2)

    draw.text((116, 128), "MINE", fill=(244, 218, 147, 255), font=font(40))
    draw.text((121, 131), "MINE", fill=(88, 49, 24, 210), font=font(40))
    return add_drop_shadow(image, offset=(24, 30), blur=16, opacity=118, padding=38)


def render_mode_cell(name: str, image: Image.Image, metrics: dict[str, Any], size: tuple[int, int] = (250, 330)) -> Image.Image:
    cell = Image.new("RGBA", size, (18, 19, 23, 255))
    draw = ImageDraw.Draw(cell)
    label_font = font(14)
    small_font = font(10)
    draw.rectangle((0, 0, size[0] - 1, size[1] - 1), outline=(90, 86, 78, 255), width=1)
    draw.text((10, 8), name, fill=(244, 238, 220, 255), font=label_font)

    top = checkerboard((size[0] - 20, 160), 12)
    preview = contain(image, (top.width - 14, top.height - 14))
    top.alpha_composite(preview, ((top.width - preview.width) // 2, (top.height - preview.height) // 2))
    cell.alpha_composite(top, (10, 32))

    bg_w = (size[0] - 28) // 3
    for index, (_bg_name, bg) in enumerate(BACKGROUNDS):
        swatch = composite_on(image, bg, (bg_w, 62))
        x = 10 + index * (bg_w + 4)
        cell.alpha_composite(swatch, (x, 204))

    lines = [
        f"alpha: {metrics.get('visible_pixels', '-')}",
        f"key: {metrics.get('visible_key_pixels', '-')}",
        f"spill: {metrics.get('green_spill_pixels', '-')}",
        f"hidden: {metrics.get('transparent_nonzero_rgb', '-')}",
    ]
    for index, line in enumerate(lines):
        draw.text((10, 274 + index * 13), line, fill=(218, 211, 196, 255), font=small_font)
    return cell


def visible_key_mask(array: Any, key: RGB, tolerance: int) -> Any:
    rgb = array[..., :3].astype(np.int16)
    key_array = np.asarray(key, dtype=np.int16)
    return np.max(np.abs(rgb - key_array), axis=2) <= tolerance


def metrics(image: Image.Image, key: RGB) -> dict[str, Any]:
    rgba = image.convert("RGBA")
    if np is None:
        pixels = list(rgba.getdata())
        visible = [px for px in pixels if px[3] > 12]
        transparent_nonzero = sum(1 for r, g, b, a in pixels if a == 0 and (r or g or b))
        return {
            "visible_pixels": len(visible),
            "visible_key_pixels": None,
            "green_spill_pixels": None,
            "source_key_spill_pixels": None,
            "transparent_nonzero_rgb": transparent_nonzero,
            "alpha_bbox": rgba.getchannel("A").getbbox(),
        }
    array = np.asarray(rgba, dtype=np.uint8)
    alpha = array[..., 3]
    visible = alpha > 12
    red = array[..., 0].astype(np.int16)
    green = array[..., 1].astype(np.int16)
    blue = array[..., 2].astype(np.int16)
    return {
        "visible_pixels": int(np.count_nonzero(visible)),
        "visible_key_pixels": int(np.count_nonzero(visible & visible_key_mask(array, key, 24))),
        "green_spill_pixels": int(np.count_nonzero(visible & green_screen_spill_mask_rgb(red, green, blue))),
        "source_key_spill_pixels": int(np.count_nonzero(visible & source_key_spill_mask(red, green, blue, key))),
        "transparent_nonzero_rgb": int(np.count_nonzero((alpha == 0) & np.any(array[..., :3] != 0, axis=2))),
        "alpha_bbox": list(rgba.getchannel("A").getbbox() or ()),
    }


def soft_chroma_matte(crop: Image.Image, key: RGB, exact_tolerance: int = 10, matte_tolerance: int = 96) -> Image.Image:
    if np is None:
        return key_to_alpha(
            crop,
            key=key,
            exact_tolerance=exact_tolerance,
            edge_tolerance=matte_tolerance,
            aggressive_visible_decontaminate=True,
            remove_key_holes=True,
        )
    rgba = crop.convert("RGBA")
    array = np.array(rgba, dtype=np.uint8)
    rgb = array[..., :3].astype(np.int16)
    key_array = np.asarray(key, dtype=np.int16)
    distance = np.max(np.abs(rgb - key_array), axis=2).astype(np.float32)
    alpha = ((distance - exact_tolerance) / max(1, matte_tolerance - exact_tolerance) * 255.0).clip(0, 255)
    alpha = np.minimum(alpha, array[..., 3].astype(np.float32))
    array[..., 3] = np.rint(alpha).astype(np.uint8)
    result = Image.fromarray(array, "RGBA")
    decontaminate_source_key_spill_image(result, key=key, require_transparent_touch=False)
    bleed_transparent_rgb(result, key=key)
    repair_transparent_edge_rgb(result, key=key)
    zero_fully_transparent_rgb(result)
    return result


def pymatting_trimap(crop: Image.Image, key: RGB, exact_tolerance: int = 10, foreground_tolerance: int = 82) -> Image.Image:
    if np is None:
        return soft_chroma_matte(crop, key, exact_tolerance=exact_tolerance, matte_tolerance=foreground_tolerance)
    try:
        from pymatting import estimate_alpha_cf, estimate_foreground_ml
    except Exception:
        return soft_chroma_matte(crop, key, exact_tolerance=exact_tolerance, matte_tolerance=foreground_tolerance)

    rgba = crop.convert("RGBA")
    original_size = rgba.size
    work = rgba
    max_dim = max(work.size)
    scale = 1.0
    if max_dim > 240:
        scale = 240 / max_dim
        work = work.resize((max(1, round(work.width * scale)), max(1, round(work.height * scale))), Image.Resampling.LANCZOS)

    array = np.asarray(work, dtype=np.float64) / 255.0
    rgb = array[..., :3]
    key_array = np.asarray(key, dtype=np.float64) / 255.0
    distance = np.max(np.abs(rgb - key_array), axis=2) * 255.0
    trimap = np.full(distance.shape, 0.5, dtype=np.float64)
    trimap[distance <= exact_tolerance] = 0.0
    trimap[distance >= foreground_tolerance] = 1.0
    try:
        alpha = estimate_alpha_cf(rgb, trimap)
        alpha = np.clip(alpha, 0.0, 1.0)
        foreground = estimate_foreground_ml(rgb, alpha)
        output_array = np.zeros((*alpha.shape, 4), dtype=np.uint8)
        output_array[..., :3] = np.rint(np.clip(foreground, 0.0, 1.0) * 255.0).astype(np.uint8)
        output_array[..., 3] = np.rint(alpha * 255.0).astype(np.uint8)
        result = Image.fromarray(output_array, "RGBA")
        if result.size != original_size:
            result = result.resize(original_size, Image.Resampling.LANCZOS)
        decontaminate_source_key_spill_image(result, key=key, require_transparent_touch=False)
        bleed_transparent_rgb(result, key=key)
        repair_transparent_edge_rgb(result, key=key)
        zero_fully_transparent_rgb(result)
        return result
    except Exception:
        return soft_chroma_matte(crop, key, exact_tolerance=exact_tolerance, matte_tolerance=foreground_tolerance)


def mode_source(crop: Image.Image, _key: RGB) -> Image.Image:
    return crop.convert("RGBA")


def mode_current(crop: Image.Image, key: RGB) -> Image.Image:
    return key_to_alpha(crop, key=key, aggressive_visible_decontaminate=False)


def mode_current_aggressive(crop: Image.Image, key: RGB) -> Image.Image:
    return key_to_alpha(crop, key=key, aggressive_visible_decontaminate=True)


def mode_key_holes(crop: Image.Image, key: RGB) -> Image.Image:
    return key_to_alpha(crop, key=key, aggressive_visible_decontaminate=True, remove_key_holes=True)


SINGLE_BACKGROUND_MODES: list[tuple[str, Callable[[Image.Image, RGB], Image.Image]]] = [
    ("source", mode_source),
    ("current", mode_current),
    ("aggressive", mode_current_aggressive),
    ("holes", mode_key_holes),
    ("soft matte", soft_chroma_matte),
    ("pymatting", pymatting_trimap),
]


def process_single_background_case(case: dict[str, Any], output_dir: Path) -> dict[str, Any]:
    crop, source_label, rect = source_image_for_case(case)
    key = parse_color(case["key"])

    results: list[dict[str, Any]] = []
    cells: list[Image.Image] = []
    for mode_name, mode_fn in SINGLE_BACKGROUND_MODES:
        started = perf_counter()
        output = mode_fn(crop, key)
        elapsed_ms = round((perf_counter() - started) * 1000, 3)
        mode_metrics = metrics(output, key)
        mode_metrics["time_ms"] = elapsed_ms
        mode_metrics["mode"] = mode_name
        results.append(mode_metrics)
        cells.append(render_mode_cell(mode_name, output, mode_metrics))

    title_h = 70
    cell_w, cell_h = cells[0].size
    sheet = Image.new("RGBA", (cell_w * len(cells), title_h + cell_h), (12, 13, 16, 255))
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 10), f"{case['id']} - {case['label']}", fill=(248, 241, 222, 255), font=font(18))
    draw.text((12, 36), f"key={case['key']} rect={rect} source={source_label}", fill=(188, 181, 164, 255), font=font(11))
    for index, cell in enumerate(cells):
        sheet.alpha_composite(cell, (index * cell_w, title_h))

    output_path = output_dir / f"{case['id']}.png"
    save_image_atomic(sheet, output_path)
    return {
        "id": case["id"],
        "label": case["label"],
        "source": source_label,
        "rect": rect,
        "key": case["key"],
        "notes": case.get("notes", ""),
        "image": rel(output_path),
        "modes": results,
    }


def crop_from_case(path_value: str, rect: list[int] | None) -> Image.Image:
    source = Image.open(project_path(path_value)).convert("RGBA")
    if rect is None:
        return source
    x, y, width, height = [int(value) for value in rect]
    return source.crop((x, y, x + width, y + height))


def apply_source_background(image: Image.Image, case: dict[str, Any]) -> Image.Image:
    if "source_background" not in case:
        return image.convert("RGBA")
    background = parse_color(case["source_background"])
    if "source_key" in case:
        keyed = key_to_alpha(
            image,
            key=parse_color(case["source_key"]),
            aggressive_visible_decontaminate=True,
            remove_key_holes=bool(case.get("source_remove_key_holes", False)),
        )
        return flatten_on_background(keyed, background)
    return flatten_on_background(image, background)


def source_image_for_case(case: dict[str, Any]) -> tuple[Image.Image, str, list[int]]:
    rect = [int(value) for value in case.get("rect", [])]
    if "source" in case:
        source_path = project_path(case["source"])
        source = Image.open(source_path).convert("RGBA")
        if rect:
            x, y, width, height = rect
            return apply_source_background(source.crop((x, y, x + width, y + height)), case), case["source"], rect
        return apply_source_background(source, case), case["source"], []

    source_bg = parse_color(case.get("source_background", "#00ff00"))
    if case.get("procedural_source") == "wooden_sign":
        truth = make_procedural_sign_truth()
        return flatten_on_background(truth, source_bg), "procedural:wooden_sign", []

    if "alpha_source" in case:
        truth = crop_from_case(case["alpha_source"], rect or None)
        if case.get("add_shadow", False):
            shadow = case.get("shadow", {})
            truth = add_drop_shadow(
                truth,
                offset=tuple(shadow.get("offset", [18, 24])),
                blur=int(shadow.get("blur", 18)),
                opacity=int(shadow.get("opacity", 96)),
                padding=int(shadow.get("padding", 34)),
            )
        return flatten_on_background(truth, source_bg), case["alpha_source"], rect

    raise ValueError(f"case {case.get('id', '<unknown>')} must define source, alpha_source, or procedural_source")


def process_dual_plate_case(case: dict[str, Any], output_dir: Path) -> dict[str, Any]:
    key = parse_color(case.get("key", "#ffffff"))
    bg_light = parse_color(case.get("bg_light", "#ffffff"))
    bg_dark = parse_color(case.get("bg_dark", "#000000"))
    rect = [int(value) for value in case.get("rect", [])] or None
    plate_dir = output_dir / "dual_plates"
    plate_dir.mkdir(parents=True, exist_ok=True)

    if case.get("procedural_source") == "wooden_sign":
        truth = make_procedural_sign_truth()
        light_plate = flatten_on_background(truth, bg_light)
        dark_plate = flatten_on_background(truth, bg_dark)
        truth_path = plate_dir / f"{case['id']}_alpha_truth.png"
        light_path = plate_dir / f"{case['id']}_light.png"
        dark_path = plate_dir / f"{case['id']}_dark.png"
        save_image_atomic(truth, truth_path)
        save_image_atomic(light_plate, light_path)
        save_image_atomic(dark_plate, dark_path)
    elif "alpha_source" in case:
        truth = crop_from_case(case["alpha_source"], rect)
        if case.get("add_shadow", True):
            shadow = case.get("shadow", {})
            truth = add_drop_shadow(
                truth,
                offset=tuple(shadow.get("offset", [18, 24])),
                blur=int(shadow.get("blur", 18)),
                opacity=int(shadow.get("opacity", 96)),
                padding=int(shadow.get("padding", 34)),
            )
        light_plate = flatten_on_background(truth, bg_light)
        dark_plate = flatten_on_background(truth, bg_dark)
        truth_path = plate_dir / f"{case['id']}_alpha_truth.png"
        light_path = plate_dir / f"{case['id']}_light.png"
        dark_path = plate_dir / f"{case['id']}_dark.png"
        save_image_atomic(truth, truth_path)
        save_image_atomic(light_plate, light_path)
        save_image_atomic(dark_plate, dark_path)
    else:
        light_path = project_path(case["light_source"])
        dark_path = project_path(case["dark_source"])
        light_plate = Image.open(light_path).convert("RGBA")
        dark_plate = Image.open(dark_path).convert("RGBA")
        truth = light_plate
        truth_path = light_path

    mode_outputs: list[tuple[str, Image.Image]] = [
        ("alpha truth", truth),
        ("light plate", light_plate),
        ("dark plate", dark_plate),
        ("white key", key_to_alpha(light_plate, key=key, aggressive_visible_decontaminate=True, remove_key_holes=True)),
        ("soft white", soft_chroma_matte(light_plate, key, exact_tolerance=10, matte_tolerance=96)),
    ]
    started = perf_counter()
    dual_output = extract_dual_plate_alpha(
        light_plate,
        dark_plate,
        bg_light=bg_light,
        bg_dark=bg_dark,
        alpha_combine=case.get("alpha_combine", "min"),
        recovery_source=case.get("recovery_source", "average"),
        alpha_cutoff=int(case.get("alpha_cutoff", 0)),
        alpha_hardening=int(case.get("alpha_hardening", 0)),
    )
    dual_time = round((perf_counter() - started) * 1000, 3)
    mode_outputs.append(("dual plate", dual_output))

    results: list[dict[str, Any]] = []
    cells: list[Image.Image] = []
    for mode_name, output in mode_outputs:
        mode_metrics = metrics(output, key)
        mode_metrics["time_ms"] = dual_time if mode_name == "dual plate" else 0.0
        mode_metrics["mode"] = mode_name
        results.append(mode_metrics)
        cells.append(render_mode_cell(mode_name, output, mode_metrics))

    title_h = 76
    cell_w, cell_h = cells[0].size
    sheet = Image.new("RGBA", (cell_w * len(cells), title_h + cell_h), (12, 13, 16, 255))
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 10), f"{case['id']} - {case['label']}", fill=(248, 241, 222, 255), font=font(18))
    draw.text(
        (12, 36),
        f"dual plate bg_light={case.get('bg_light', '#ffffff')} bg_dark={case.get('bg_dark', '#000000')}",
        fill=(188, 181, 164, 255),
        font=font(11),
    )
    draw.text((12, 52), f"truth={rel(truth_path)} light={rel(light_path)} dark={rel(dark_path)}", fill=(188, 181, 164, 255), font=font(10))
    for index, cell in enumerate(cells):
        sheet.alpha_composite(cell, (index * cell_w, title_h))

    output_path = output_dir / f"{case['id']}.png"
    save_image_atomic(sheet, output_path)
    return {
        "id": case["id"],
        "label": case["label"],
        "source": case.get("alpha_source", case.get("light_source", case.get("procedural_source", ""))),
        "rect": rect or [],
        "key": case.get("key", "#ffffff"),
        "notes": case.get("notes", ""),
        "image": rel(output_path),
        "generated_dual_plate_inputs": {
            "alpha_truth": rel(truth_path),
            "light": rel(light_path),
            "dark": rel(dark_path),
        },
        "modes": results,
    }


def process_case(case: dict[str, Any], output_dir: Path) -> dict[str, Any]:
    if case.get("type") == "dual_plate":
        return process_dual_plate_case(case, output_dir)
    return process_single_background_case(case, output_dir)


def build_overview(case_reports: list[dict[str, Any]], output_dir: Path) -> Path:
    images = [Image.open(project_path(report["image"])).convert("RGBA") for report in case_reports]
    if not images:
        raise SystemExit("no benchmark images produced")
    width = max(image.width for image in images)
    height = sum(image.height for image in images) + (len(images) - 1) * 16
    overview = Image.new("RGBA", (width, height), (8, 9, 11, 255))
    y = 0
    for image in images:
        overview.alpha_composite(image, ((width - image.width) // 2, y))
        y += image.height + 16
    path = output_dir / "overview.png"
    save_image_atomic(overview, path)
    return path


def markdown_report(report: dict[str, Any]) -> str:
    lines = [
        "# Cutout Mode Benchmark",
        "",
        f"Overview: `{report['overview']}`",
        "",
        "## Modes",
        "- `source`: original crop, no alpha extraction.",
        "- `current`: current conservative border-connected key extraction.",
        "- `aggressive`: current extraction with visible decontamination.",
        "- `holes`: exact key holes removed as well as border background.",
        "- `soft matte`: distance-based soft alpha matte plus despill cleanup.",
        "- `pymatting`: optional PyMatting closed-form alpha from chroma trimap.",
        "- `dual plate`: alpha reconstructed from pixel-aligned light and dark background plates.",
        "",
        "## Cases",
    ]
    for case in report["cases"]:
        lines.extend(
            [
                "",
                f"### {case['id']}",
                "",
                f"Image: `{case['image']}`",
                f"Source: `{case['source']}`",
                f"Notes: {case.get('notes', '')}",
                "",
                "| mode | visible | key px | green spill | source-key spill | hidden rgb | time ms |",
                "|---|---:|---:|---:|---:|---:|---:|",
            ]
        )
        for mode in case["modes"]:
            lines.append(
                f"| {mode['mode']} | {mode['visible_pixels']} | {mode['visible_key_pixels']} | "
                f"{mode['green_spill_pixels']} | {mode['source_key_spill_pixels']} | "
                f"{mode['transparent_nonzero_rgb']} | {mode['time_ms']} |"
            )
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Render visual comparison sheets for cutout modes.")
    parser.add_argument(
        "--fixtures",
        type=Path,
        default=Path("gamedesign/projects/mine-cards/reviews/cutout_benchmark/fixtures.json"),
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("gamedesign/projects/mine-cards/reviews/cutout_benchmark"),
    )
    args = parser.parse_args()

    fixtures_path = project_path(args.fixtures)
    output_dir = project_path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    fixtures = json.loads(fixtures_path.read_text(encoding="utf-8"))
    cases = fixtures.get("cases")
    if not isinstance(cases, list) or not cases:
        raise SystemExit("fixtures must contain non-empty cases")
    case_reports = [process_case(case, output_dir) for case in cases]
    overview_path = build_overview(case_reports, output_dir)
    report = {
        "schema": "game.cutout_mode_benchmark",
        "version": 1,
        "fixtures": rel(fixtures_path),
        "overview": rel(overview_path),
        "cases": case_reports,
    }
    write_json_atomic(output_dir / "benchmark_results.json", report)
    write_text_atomic(output_dir / "benchmark_report.md", markdown_report(report))
    print(f"wrote overview: {rel(overview_path)}")
    print(f"wrote report: {rel(output_dir / 'benchmark_report.md')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
