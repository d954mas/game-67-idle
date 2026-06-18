#!/usr/bin/env python3
"""Ground-truth alpha benchmark for the two cutout paths.

The visual cutout benchmark only measures leftover-pixel proxies, so a mode that
deletes the subject can score "clean". This benchmark instead builds synthetic
sprites with a KNOWN alpha channel, composites each onto the flat key colour
(path 1 input) and onto white+black plates (path 2 input), runs every mode, and
scores the recovered alpha against the known truth. Lower alpha SAD/RMSE/gradient
error and higher mask IoU are better. This is what proves a change is actually
better and not just different.

Run: py -3.12 tools/assets/benchmark_alpha_truth.py
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Any, Callable

from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

try:
    import numpy as np
except ImportError:  # pragma: no cover - this benchmark needs numpy.
    np = None

from tools.assets.atomic_io import save_image_atomic, write_json_atomic, write_text_atomic
from tools.assets._dev.benchmark_cutout_modes import (
    checkerboard,
    contain,
    flatten_on_background,
    font,
    key_trimap_matte,
    pymatting_trimap,
    rel,
    soft_chroma_matte,
)
from tools.assets.chroma_key_alpha import key_to_alpha
from tools.assets.cutout.dual_plate_alpha import extract_dual_plate_alpha
from PIL import ImageDraw

RGB = tuple[int, int, int]
KEY: RGB = (0, 255, 0)


def _solid_rgba(alpha: "np.ndarray", color: RGB) -> Image.Image:
    array = np.zeros((*alpha.shape, 4), dtype=np.uint8)
    array[..., 0] = color[0]
    array[..., 1] = color[1]
    array[..., 2] = color[2]
    array[..., 3] = np.clip(np.rint(alpha), 0, 255).astype(np.uint8)
    return Image.fromarray(array, "RGBA")


def _radius(size: int) -> tuple["np.ndarray", float, float]:
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    cx = cy = (size - 1) / 2.0
    return np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2), cx, cy


def make_ring_hole(size: int = 256) -> Image.Image:
    """Opaque metal ring with a TRANSPARENT centre hole (interior key case)."""
    r, _cx, _cy = _radius(size)
    outer, inner, aa = size * 0.44, size * 0.24, 1.6
    annulus = np.clip((outer - r) / aa, 0, 1) * np.clip((r - inner) / aa, 0, 1)
    return _solid_rgba(annulus * 255.0, (150, 162, 178))


def make_soft_shadow(size: int = 256) -> Image.Image:
    """Soft semi-transparent cast shadow (alpha blends into the background)."""
    r, _cx, _cy = _radius(size)
    falloff = np.exp(-((r / (size * 0.34)) ** 2))
    falloff[falloff < 0.02] = 0.0
    return _solid_rgba(falloff * 205.0, (24, 20, 16))


def make_glow(size: int = 256) -> Image.Image:
    """Warm glow / particle halo: bright core, soft transparent falloff."""
    r, _cx, _cy = _radius(size)
    core = np.clip(1.35 * np.exp(-((r / (size * 0.30)) ** 2)) - 0.04, 0, 1)
    return _solid_rgba(core * 255.0, (255, 208, 120))


def make_hard_gear(size: int = 256) -> Image.Image:
    """Crisp opaque gear with anti-aliased rim (tests thin AA edges)."""
    scale = 4
    big = Image.new("L", (size * scale, size * scale), 0)
    draw = ImageDraw.Draw(big)
    cx = cy = size * scale / 2
    teeth = 12
    outer = size * scale * 0.44
    inner = size * scale * 0.30
    points = []
    for index in range(teeth * 2):
        ang = index * 3.14159265 / teeth
        rad = outer if index % 2 == 0 else inner
        points.append((cx + rad * np.cos(ang), cy + rad * np.sin(ang)))
    draw.polygon(points, fill=255)
    draw.ellipse((cx - size * scale * 0.12, cy - size * scale * 0.12, cx + size * scale * 0.12, cy + size * scale * 0.12), fill=0)
    alpha = np.asarray(big.resize((size, size), Image.Resampling.LANCZOS), dtype=np.float64)
    return _solid_rgba(alpha, (180, 120, 70))


def make_glass_orb(size: int = 256) -> Image.Image:
    """Semi-transparent glass disc with an opaque rim (constant fractional alpha)."""
    r, _cx, _cy = _radius(size)
    aa = 1.6
    disc = np.clip((size * 0.40 - r) / aa, 0, 1)
    rim = np.clip((r - size * 0.34) / aa, 0, 1) * disc
    alpha = disc * 120.0
    alpha = np.maximum(alpha, rim * 235.0)
    return _solid_rgba(alpha, (120, 190, 220))


TRUTHS: list[tuple[str, Callable[[], Image.Image]]] = [
    ("ring_hole", make_ring_hole),
    ("soft_shadow", make_soft_shadow),
    ("glow", make_glow),
    ("hard_gear", make_hard_gear),
    ("glass_orb", make_glass_orb),
]

# Path 1 (single-background) is only well-posed for opaque art + flat-key holes.
# Fractional alpha (soft shadow / glow / glass) is mathematically unrecoverable
# from one background and belongs to path 2 (dual-plate). Group the truth sprites
# so "best on its own domain" is visible instead of averaged away.
GROUPS: dict[str, set[str]] = {
    "hard_edge_or_holes": {"ring_hole", "hard_gear"},
    "soft_or_transparent": {"soft_shadow", "glow", "glass_orb"},
}


SINGLE_MODES: list[tuple[str, Callable[[Image.Image], Image.Image]]] = [
    ("current", lambda crop: key_to_alpha(crop, key=KEY)),
    ("aggressive", lambda crop: key_to_alpha(crop, key=KEY, aggressive_visible_decontaminate=True)),
    ("holes", lambda crop: key_to_alpha(crop, key=KEY, aggressive_visible_decontaminate=True, remove_key_holes=True)),
    ("soft matte", lambda crop: soft_chroma_matte(crop, KEY)),
    ("pymatting240", lambda crop: pymatting_trimap(crop, KEY)),
    ("key matte", lambda crop: key_trimap_matte(crop, KEY)),
]


def grad_mag(channel: "np.ndarray") -> "np.ndarray":
    gy, gx = np.gradient(channel)
    return np.sqrt(gx * gx + gy * gy)


def alpha_quality(output: Image.Image, truth: Image.Image) -> dict[str, float]:
    out_alpha = np.asarray(output.convert("RGBA").getchannel("A"), dtype=np.float64)
    truth_alpha = np.asarray(truth.convert("RGBA").getchannel("A").resize(output.size, Image.Resampling.LANCZOS), dtype=np.float64)
    sad = float(np.mean(np.abs(out_alpha - truth_alpha)))
    rmse = float(np.sqrt(np.mean((out_alpha - truth_alpha) ** 2)))
    grad = float(np.mean(np.abs(grad_mag(out_alpha) - grad_mag(truth_alpha))))
    out_mask = out_alpha > 12
    truth_mask = truth_alpha > 12
    union = int(np.count_nonzero(out_mask | truth_mask))
    iou = float(np.count_nonzero(out_mask & truth_mask) / union) if union else 1.0
    return {"alpha_sad": round(sad, 2), "alpha_rmse": round(rmse, 2), "grad_sad": round(grad, 3), "mask_iou": round(iou, 3)}


def preview_on_checker(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    board = checkerboard(size, 12)
    preview = contain(image, (size[0] - 8, size[1] - 8))
    board.alpha_composite(preview, ((board.width - preview.width) // 2, (board.height - preview.height) // 2))
    return board


def render_row(name: str, truth: Image.Image, cells: list[tuple[str, Image.Image, dict[str, float]]]) -> Image.Image:
    cell_w, cell_h = 150, 200
    columns = [("truth", truth, None)] + cells
    row = Image.new("RGBA", (cell_w * len(columns), cell_h + 26), (16, 17, 20, 255))
    draw = ImageDraw.Draw(row)
    draw.text((8, 6), name, fill=(244, 238, 220, 255), font=font(14))
    for index, (label, image, quality) in enumerate(columns):
        x = index * cell_w
        row.alpha_composite(preview_on_checker(image, (cell_w - 8, cell_h - 44)), (x + 4, 26))
        draw.text((x + 6, cell_h - 14), label, fill=(230, 224, 208, 255), font=font(11))
        if quality is not None:
            draw.text((x + 6, cell_h - 1), f"SAD {quality['alpha_sad']} IoU {quality['mask_iou']}", fill=(150, 200, 150, 255), font=font(10))
    return row


def main() -> int:
    if np is None:
        raise SystemExit("benchmark_alpha_truth requires numpy")
    parser = argparse.ArgumentParser(description="Ground-truth alpha benchmark for cutout paths.")
    parser.add_argument("--output-dir", type=Path, default=Path("gamedesign/projects/mine-cards/reviews/cutout_benchmark/alpha_truth"))
    parser.add_argument("--size", type=int, default=256)
    args = parser.parse_args()

    output_dir = args.output_dir if args.output_dir.is_absolute() else ROOT / args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)

    cases: list[dict[str, Any]] = []
    rows: list[Image.Image] = []

    for truth_name, factory in TRUTHS:
        truth = factory()
        green = flatten_on_background(truth, KEY)
        white = flatten_on_background(truth, (255, 255, 255))
        black = flatten_on_background(truth, (0, 0, 0))

        mode_results: list[dict[str, Any]] = []
        cells: list[tuple[str, Image.Image, dict[str, float]]] = []
        for mode_name, mode_fn in SINGLE_MODES:
            output = mode_fn(green)
            quality = alpha_quality(output, truth)
            mode_results.append({"mode": mode_name, "path": "single", **quality})
            cells.append((mode_name, output, quality))

        for combine in ("min", "proj"):
            dual = extract_dual_plate_alpha(white, black, bg_light=(255, 255, 255), bg_dark=(0, 0, 0), alpha_combine=combine, recovery_source="average")
            quality = alpha_quality(dual, truth)
            label = f"dual {combine}"
            mode_results.append({"mode": label, "path": "dual", **quality})
            cells.append((label, dual, quality))

        cases.append({"truth": truth_name, "modes": mode_results})
        rows.append(render_row(truth_name, truth, cells))

    width = max(row.width for row in rows)
    height = sum(row.height for row in rows) + 12 * (len(rows) - 1)
    overview = Image.new("RGBA", (width, height), (8, 9, 11, 255))
    y = 0
    for row in rows:
        overview.alpha_composite(row, (0, y))
        y += row.height + 12
    overview_path = output_dir / "alpha_truth_overview.png"
    save_image_atomic(overview, overview_path)

    def mean(values: list[float]) -> float:
        return round(sum(values) / len(values), 2) if values else 0.0

    # mode -> {truth -> alpha_sad}
    sad_by_mode: dict[str, dict[str, float]] = {}
    for case in cases:
        for mode in case["modes"]:
            sad_by_mode.setdefault(mode["mode"], {})[case["truth"]] = mode["alpha_sad"]

    overall = {mode: mean(list(truths.values())) for mode, truths in sad_by_mode.items()}
    by_group: dict[str, dict[str, float]] = {}
    for group_name, members in GROUPS.items():
        by_group[group_name] = {
            mode: mean([sad for truth, sad in truths.items() if truth in members])
            for mode, truths in sad_by_mode.items()
        }

    report = {
        "schema": "game.alpha_truth_benchmark",
        "version": 1,
        "metric": "alpha_sad is mean |alpha-truth| in 0..255; lower is better. mask_iou higher is better.",
        "overview": rel(overview_path),
        "groups": {name: sorted(members) for name, members in GROUPS.items()},
        "mean_alpha_sad_overall": overall,
        "mean_alpha_sad_by_group": by_group,
        "cases": cases,
    }
    write_json_atomic(output_dir / "alpha_truth_benchmark.json", report)

    def sad_table(title: str, scores: dict[str, float]) -> list[str]:
        out = ["", f"## {title} (mean alpha SAD, lower is better)", "", "| mode | mean alpha SAD |", "|---|---:|"]
        for name, value in sorted(scores.items(), key=lambda item: item[1]):
            out.append(f"| {name} | {value} |")
        return out

    lines = [
        "# Ground-Truth Alpha Benchmark",
        "",
        f"Overview: `{rel(overview_path)}`",
        "",
        "`alpha_sad` = mean |alpha - truth| (0..255, lower better). `mask_iou` higher better.",
        "Path 1 (single-background) is well-posed only for opaque art + flat-key holes;",
        "fractional alpha (soft shadow / glow / glass) needs path 2 (dual-plate).",
    ]
    lines += sad_table("hard_edge_or_holes — PATH 1's domain", by_group["hard_edge_or_holes"])
    lines += sad_table("soft_or_transparent — PATH 2's domain", by_group["soft_or_transparent"])
    lines += sad_table("overall (mixed — for reference only)", overall)
    lines.append("")
    for case in cases:
        lines.extend(["", f"### {case['truth']}", "", "| mode | path | alpha SAD | alpha RMSE | grad SAD | mask IoU |", "|---|---|---:|---:|---:|---:|"])
        for mode in case["modes"]:
            lines.append(f"| {mode['mode']} | {mode['path']} | {mode['alpha_sad']} | {mode['alpha_rmse']} | {mode['grad_sad']} | {mode['mask_iou']} |")
    lines.append("")
    write_text_atomic(output_dir / "alpha_truth_benchmark.md", "\n".join(lines))

    for group_name in ("hard_edge_or_holes", "soft_or_transparent"):
        print(f"{group_name} (mean alpha SAD, lower better):")
        for name, value in sorted(by_group[group_name].items(), key=lambda item: item[1]):
            print(f"  {name:14s} {value}")
    print(f"wrote {rel(overview_path)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
