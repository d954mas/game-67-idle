#!/usr/bin/env python3
"""Flat-light-background REPORT for the AUTOMATIC dual-plate alpha flow (T0238/T0248).

ops.alphaDualPlateGenerate needs to know whether an existing image element's
CURRENT pixels are already a usable dual-plate LIGHT plate (flat + light
border) or need a white plate generated FIRST (arbitrary art — a subject
already composited onto a busy scene, a gradient, a dark backdrop, ...). This
tool answers that question; it does NOT decide the flow.

T0248: this is REPORT-ONLY. Earlier (T0238) a non-flat/non-light border was a
loud refusal (SystemExit) — the lead correctly called that wrong on real art:
the reference script .codex/skills/nt-asset-image-generation/scripts/
gen_dual_plate.sh never assumes a flat background either; it GENERATES the
white plate from arbitrary source art first. So this tool always writes its
verdict and exits 0; the caller (ops.alphaDualPlateGenerate) reads
`flat_light` and ROUTES: true keeps the one-codex-call path (the element's own
pixels are the light plate); false generates the white plate first, exactly
like gen_dual_plate.sh's chain. A genuine tool/environment failure (missing or
corrupt source image) still raises loudly — only the flat/not-flat JUDGMENT
stopped refusing.

Border-ring sampling mirrors the idea in
ai_studio/assets/tools/image/alpha_dualplate/pair_align.py's _border_median: the
outer few pixels on every edge are assumed to be pure background — the subject
sits away from the frame edge, as every dual-plate/matte source in this repo
does (a canvas element with the subject touching the frame reads as non-flat
too, which is expected: there is no flat border to sample).

Spec (schema ai_studio.canvas.check_flat_bg_spec.v1): {source, report?}

CPU + numpy/Pillow only, through the studio venv (same interpreter as the other
canvas python tools; no extra dependency).
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

from ai_studio.assets.tools.lib.atomic_io import write_json_atomic

# Below this median border luma (0-255) the background reads as too dark/mid-tone
# to be a "flat LIGHT" plate (lead spec, T0238: "~200").
MEDIAN_LUMA_MIN = 200.0
# Above this per-channel std across the sampled border ring, the background is
# "clearly non-flat" (gradient, texture, busy scene) rather than a solid fill.
SPREAD_MAX = 24.0
BORDER_MARGIN = 4


def border_ring(array: np.ndarray, margin: int = BORDER_MARGIN) -> np.ndarray:
    """The outer `margin`-px ring of an (H, W, C) array, flattened to (N, C) — the
    same sampling idea as pair_align._border_median (a plate's background estimate)."""
    height, width = array.shape[:2]
    m = max(1, min(margin, height // 2, width // 2))
    channels = array.shape[-1]
    return np.concatenate(
        [
            array[:m, :].reshape(-1, channels),
            array[-m:, :].reshape(-1, channels),
            array[:, :m].reshape(-1, channels),
            array[:, -m:].reshape(-1, channels),
        ],
        axis=0,
    )


def run(spec: dict[str, Any]) -> dict[str, Any]:
    source = Path(spec["source"])
    if not source.exists():
        raise FileNotFoundError(f"source image missing: {source}")
    image = Image.open(source).convert("RGB")
    array = np.asarray(image, dtype=np.float32)
    ring = border_ring(array)
    median_rgb = np.median(ring, axis=0)
    median_luma = float(median_rgb.mean())
    spread = float(ring.std())
    flat_light = median_luma >= MEDIAN_LUMA_MIN and spread <= SPREAD_MAX

    report = {
        "schema": "ai_studio.canvas.check_flat_bg_report.v1",
        "source": source.as_posix(),
        "median_rgb": [round(float(channel), 2) for channel in median_rgb],
        "median_luma": round(median_luma, 2),
        "spread": round(spread, 2),
        "flat_light": flat_light,
    }
    report_path = spec.get("report")
    if report_path:
        write_json_atomic(Path(report_path), report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Report whether an element's border ring reads as a flat light background (report-only, T0248 — the caller routes, this never refuses)."
    )
    parser.add_argument("--spec", required=True, type=Path, help="check spec JSON (ai_studio.canvas.check_flat_bg_spec.v1)")
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text(encoding="utf-8"))
    try:
        report = run(spec)
    except (ValueError, FileNotFoundError) as exc:
        # Genuine usage/environment failure (missing or corrupt source) still travels the
        # worker's SystemExit path as ONE clean message — no Python traceback in the
        # operator-facing toast. The flat/not-flat JUDGMENT itself no longer refuses (T0248).
        raise SystemExit(str(exc)) from exc
    print(f"report: flat_light={report['flat_light']} (median luma {report['median_luma']:.0f}, spread {report['spread']:.1f})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
