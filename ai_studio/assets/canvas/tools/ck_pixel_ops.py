#!/usr/bin/env python3
"""CorridorKey pixel helpers (T0262/T0263 follow-up): the ONE canvas-side Python pixel
tool the corridorkey alpha method's two v1 gaps were blocked on (ops.mjs comments,
research_corridorkey_magenta_2026-07-05.md). Two independent operations, dispatched by
`spec["op"]`, sharing this one file/one entry point rather than two:

  * "hue_shift"       — rotate an image's RGB hue by +180 degrees (value-preserving HSV
                        rotation), alpha (if any) copied through BYTE-EXACT. hue+180 is
                        its OWN inverse (mod 360), so this ONE function serves BOTH
                        directions of the magenta shim: (a) staging a magenta source as a
                        green-keyed frame before CorridorKey runs (rotate magenta 300deg ->
                        green 120deg), and (b) un-rotating CorridorKey's green-space FG back
                        to the original hue afterwards (rotate 120deg -> 300deg). Ported
                        from the verified experiment runner
                        `C:\\projects\\video_gen_experiment\\static_eval\\trick_run.py`'s
                        `hue180()` (cv2.COLOR_RGB2HSV / HSV2RGB) — this repo's venv has no
                        cv2, so the standard HSV<->RGB algorithm cv2 implements for float32
                        is ported here in pure numpy (verified byte-for-byte equivalent on
                        primary hues; see ck_pixel_ops_test.py's round-trip test).
  * "compose_regions" — paste a full-frame CorridorKey result INTO the requested regions of
                        a copy of the ORIGINAL source, leaving everything outside those
                        regions at the original opaque pixels. CorridorKey is architecturally
                        whole-frame-only (no per-region seam of its own), so region-scoped
                        corridorkey runs CK ONCE on the whole element and this composites the
                        result in — reusing alpha_cutout.py's `region_mask`/`clamp_rect`
                        (imported, not duplicated) so the mask/paste contract is BIT-FOR-BIT
                        the same one key_matte's region path already uses.

Placement rationale: both operations are canvas-side pixel plumbing for the SAME feature
(corridorkey's two v1 gaps) and neither is heavy enough to justify a second new file; one
helper, two dispatched ops, mirrors how alpha_cutout.py and alpha_dualplate.py each hold
several related functions behind one --spec entry point.

Spec (schema ai_studio.canvas.ck_pixel_ops_spec.v1):
  hue_shift:       {op:"hue_shift", source, output, report?}
  compose_regions: {op:"compose_regions", source, keyed, regions:[{id, rect, polygon?}], output, report?}

CPU + numpy/Pillow only, through the studio venv (same interpreter as the other canvas
python tools; no extra dependency — no cv2 needed since the HSV math is ported).
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[4]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import save_image_atomic, write_json_atomic

# LAW (lead, 2026-07-02): no silent fallbacks. Region composition reuses alpha_cutout.py's
# own region-mask machinery (verbatim, via import) so there is no second mask/paste
# implementation; if it cannot be imported we raise loudly instead of reimplementing it.
try:
    from ai_studio.assets.canvas.tools.alpha_cutout import clamp_rect, region_mask
except ImportError as exc:  # pragma: no cover - environment/setup failure
    raise RuntimeError(
        "ck_pixel_ops compose_regions requires ai_studio.assets.canvas.tools.alpha_cutout "
        f"(clamp_rect, region_mask) but it could not be imported ({exc})"
    ) from exc

OPS = ("hue_shift", "compose_regions")


# ---- hue+180 shim (magenta <-> green) ----------------------------------------

def _rgb01_to_hsv(rgb01: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Standard RGB->HSV (H in degrees [0,360), S/V in [0,1]) — the same algorithm
    cv2.cvtColor(..., COLOR_RGB2HSV) implements for float32 input."""
    r, g, b = rgb01[..., 0], rgb01[..., 1], rgb01[..., 2]
    cmax = np.maximum(np.maximum(r, g), b)
    cmin = np.minimum(np.minimum(r, g), b)
    delta = cmax - cmin
    safe_delta = np.where(delta == 0, 1.0, delta)

    hue_r = 60.0 * (((g - b) / safe_delta) % 6.0)
    hue_g = 60.0 * (((b - r) / safe_delta) + 2.0)
    hue_b = 60.0 * (((r - g) / safe_delta) + 4.0)
    hue = np.where(cmax == r, hue_r, np.where(cmax == g, hue_g, hue_b))
    hue = np.where(delta == 0, 0.0, hue) % 360.0

    safe_cmax = np.where(cmax == 0, 1.0, cmax)
    sat = np.where(cmax == 0, 0.0, delta / safe_cmax)
    val = cmax
    return hue, sat, val


def _hsv_to_rgb01(hue: np.ndarray, sat: np.ndarray, val: np.ndarray) -> np.ndarray:
    """Inverse of _rgb01_to_hsv — the same algorithm cv2.cvtColor(..., COLOR_HSV2RGB)
    implements for float32 input."""
    c = val * sat
    x = c * (1.0 - np.abs(((hue / 60.0) % 2.0) - 1.0))
    m = val - c

    sector = (np.floor(hue / 60.0).astype(np.int64) % 6)[..., None]
    zeros = np.zeros_like(c)
    r_opts = np.stack([c, x, zeros, zeros, x, c], axis=-1)
    g_opts = np.stack([x, c, c, x, zeros, zeros], axis=-1)
    b_opts = np.stack([zeros, zeros, x, c, c, x], axis=-1)

    r_prime = np.take_along_axis(r_opts, sector, axis=-1)[..., 0]
    g_prime = np.take_along_axis(g_opts, sector, axis=-1)[..., 0]
    b_prime = np.take_along_axis(b_opts, sector, axis=-1)[..., 0]
    return np.stack([r_prime + m, g_prime + m, b_prime + m], axis=-1)


def hue_rotate_180_rgb01(rgb01: np.ndarray) -> np.ndarray:
    """Rotate hue by +180 degrees, value-preserving (S and V untouched) — ported EXACTLY
    from trick_run.py's `hue180()`: magenta (hue 300) <-> green (hue 120). Grayscale
    pixels (r==g==b, S=0) are hue-stable and pass through unchanged."""
    hue, sat, val = _rgb01_to_hsv(np.clip(rgb01, 0.0, 1.0).astype(np.float32))
    hue = (hue + 180.0) % 360.0
    return np.clip(_hsv_to_rgb01(hue, sat, val), 0.0, 1.0)


def hue_shift_image(image: Image.Image) -> Image.Image:
    """Rotate ONLY the RGB hue of `image` by 180 degrees. If the image carries an alpha
    channel it is copied through BYTE-EXACT (read straight from the source array, never
    derived from the float HSV round trip) — CorridorKey's alpha estimate must never be
    touched by the color shim. Serves BOTH shim directions (see module docstring)."""
    has_alpha = image.mode in ("RGBA", "LA") or "transparency" in image.info
    rgba = image.convert("RGBA")
    array = np.asarray(rgba)
    rgb01 = array[..., :3].astype(np.float32) / 255.0
    rotated01 = hue_rotate_180_rgb01(rgb01)
    rotated_u8 = np.clip(np.round(rotated01 * 255.0), 0, 255).astype(np.uint8)
    if has_alpha:
        out = np.dstack([rotated_u8, array[..., 3]])  # original alpha, byte-exact
        return Image.fromarray(out, "RGBA")
    return Image.fromarray(rotated_u8, "RGB")


def run_hue_shift(spec: dict[str, Any]) -> dict[str, Any]:
    source_path = Path(spec["source"])
    if not source_path.exists():
        raise FileNotFoundError(f"hue_shift source image missing: {source_path}")
    shifted = hue_shift_image(Image.open(source_path))

    output_path = Path(spec["output"])
    output_path.parent.mkdir(parents=True, exist_ok=True)
    save_image_atomic(shifted, output_path)

    return {
        "schema": "ai_studio.canvas.ck_pixel_ops_report.v1",
        "op": "hue_shift",
        "source": source_path.as_posix(),
        "output": output_path.as_posix(),
        "width": shifted.width,
        "height": shifted.height,
        "had_alpha": shifted.mode == "RGBA",
    }


# ---- region-mask composite (region-scoped corridorkey) -----------------------

def compose_regions(
    source: Image.Image, keyed: Image.Image, regions: list[dict[str, Any]]
) -> tuple[Image.Image, list[dict[str, Any]]]:
    """Paste `keyed` (a full-frame CorridorKey RGBA result, SAME dimensions as `source`)
    into a copy of `source` — ONLY inside each region's mask (rect, or its polygon when
    present); everywhere else keeps the ORIGINAL source pixels untouched. This is the
    exact region_mask/paste contract alpha_cutout.py's regions branch uses for key_matte,
    reused verbatim (clamp_rect + region_mask imported, not reimplemented) — the only
    difference is the crop comes from an EXTERNALLY-keyed whole-frame image instead of
    being re-keyed per region."""
    if keyed.size != source.size:
        raise ValueError(f"keyed image size {keyed.size} does not match source size {source.size}")
    out = source.convert("RGBA").copy()
    keyed_rgba = keyed.convert("RGBA")
    region_reports: list[dict[str, Any]] = []
    for index, region in enumerate(regions, start=1):
        label = str(region.get("id") or f"region_{index:03d}")
        left, top, right, bottom = clamp_rect(region.get("rect"), source.size)
        crop = keyed_rgba.crop((left, top, right, bottom))
        mask = region_mask(crop.size, region, (left, top))
        out.paste(crop, (left, top), mask)  # mask None = paste the whole rect verbatim
        region_reports.append({"id": label, "rect": [left, top, right - left, bottom - top]})
    return out, region_reports


def run_compose_regions(spec: dict[str, Any]) -> dict[str, Any]:
    source_path = Path(spec["source"])
    keyed_path = Path(spec["keyed"])
    if not source_path.exists():
        raise FileNotFoundError(f"compose_regions source image missing: {source_path}")
    if not keyed_path.exists():
        raise FileNotFoundError(f"compose_regions keyed (CorridorKey result) image missing: {keyed_path}")
    regions = spec.get("regions") or []
    if not regions:
        raise ValueError("compose_regions requires a non-empty 'regions' list")

    source = Image.open(source_path).convert("RGBA")
    keyed = Image.open(keyed_path).convert("RGBA")
    out, region_reports = compose_regions(source, keyed, regions)

    output_path = Path(spec["output"])
    output_path.parent.mkdir(parents=True, exist_ok=True)
    save_image_atomic(out, output_path)

    return {
        "schema": "ai_studio.canvas.ck_pixel_ops_report.v1",
        "op": "compose_regions",
        "source": source_path.as_posix(),
        "keyed": keyed_path.as_posix(),
        "output": output_path.as_posix(),
        "width": out.width,
        "height": out.height,
        "region_count": len(regions),
        "regions": region_reports,
    }


def run(spec: dict[str, Any]) -> dict[str, Any]:
    op = str(spec.get("op") or "").strip()
    if op not in OPS:
        raise ValueError(f"unknown ck_pixel_ops op {op!r} (expected one of {OPS})")
    report = run_hue_shift(spec) if op == "hue_shift" else run_compose_regions(spec)
    report_path = spec.get("report")
    if report_path:
        write_json_atomic(Path(report_path), report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(
        description="CorridorKey pixel helpers: hue+180 shim (magenta<->green, byte-exact "
        "alpha) and region-mask compositing for region-scoped corridorkey (reuses "
        "alpha_cutout.region_mask/clamp_rect verbatim)."
    )
    parser.add_argument("--spec", required=True, type=Path, help="ck_pixel_ops spec JSON (ai_studio.canvas.ck_pixel_ops_spec.v1)")
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text(encoding="utf-8"))
    try:
        report = run(spec)
    except (RuntimeError, ValueError, FileNotFoundError) as exc:
        # Deliberate refusals (bad op, missing source/keyed, empty regions) travel the
        # worker's SystemExit path as ONE clean message — no Python traceback in the
        # operator-facing toast. Unexpected bugs still traceback loudly.
        raise SystemExit(str(exc)) from exc
    if report["op"] == "hue_shift":
        print(f"pass: ck hue shift ({report['width']}x{report['height']}, had_alpha={report['had_alpha']})")
    else:
        print(f"pass: ck compose regions ({report['region_count']} region(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
