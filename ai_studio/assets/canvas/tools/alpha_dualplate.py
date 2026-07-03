#!/usr/bin/env python3
"""Dual-plate alpha cutout: a white-plate + black-plate PAIR -> ONE RGBA PNG.

This is the canvas module's OWN Python entry (not a raster2d step), parallel to
tools/alpha_cutout.py. It REUSES the image-tools dual-plate modules verbatim — the
translation aligner (pair_align.align_pair), the pair-consistency gate
(dual_plate_pair_gate.evaluate), and the Smith & Blinn (1996) projection extractor
(dual_plate_alpha.extract_dual_plate_alpha / build_report) — so there is no matte
logic duplication and no second implementation.

T0243: chained gpt-image edits (black = edit of the white plate) routinely drift the
subject a few px between plates. Before gating, the dark plate is translation-aligned
to the light plate (align_pair, searching the SAME inconsistency metric the gate
uses) — a shift that never makes the pair worse and leaves an already-aligned pair
unchanged. The gate below then judges the ALIGNED pair, not the raw input.

ops.alphaDualPlate writes a spec JSON (the two plate elements' absolute source paths)
and spawns this script once through the shared warm worker.

Spec (schema ai_studio.canvas.alpha_dualplate_spec.v1):
  {plateA, plateB, output, report?}

Role detection (light=white-background plate vs dark=black-background plate) is NOT
part of the input — the two canvas elements arrive as an unordered pair (the caller
picked 2 image elements; nothing on the canvas tags "this one is the white plate"). We
pick by comparing each plate's overall mean RGB brightness (the background dominates
the frame, so a white-bg plate reads far brighter than a black-bg plate) — a thin
ordering step, NOT a second matte/consistency implementation; the actual extraction and
the pair-consistency check are the reused modules, untouched.

The pair gate's own verdict decides refusal (schema game.dual_plate_pair_gate):
  pass/align  -> plates agree enough; proceed to extraction
  regenerate  -> subject redrawn/misaligned between plates; LOUD refusal, no extraction

A post-extraction sanity check (dual_plate_alpha.build_report, also reused unmodified)
catches a degenerate result (e.g. no visible alpha pixels) even when the pair gate
passed.

CPU + numpy/Pillow only, through the studio venv. Missing dual-plate modules are a LOUD
import error (no-fallbacks law).
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

# LAW (lead, 2026-07-02): no silent fallbacks. The canvas dual-plate op reuses the
# image-tools dual-plate modules; if they cannot be imported we raise loudly instead of
# degrading to a hand-rolled extractor.
try:
    from ai_studio.assets.tools.image.alpha_dualplate.dual_plate_alpha import build_report, extract_dual_plate_alpha
    from ai_studio.assets.tools.image.alpha_dualplate.dual_plate_pair_gate import evaluate as evaluate_pair
    from ai_studio.assets.tools.image.alpha_dualplate.pair_align import align_pair
except ImportError as exc:  # pragma: no cover - environment/setup failure
    raise RuntimeError(
        "canvas dual-plate alpha requires the image-tools dual-plate modules "
        "(ai_studio.assets.tools.image.alpha_dualplate.dual_plate_alpha, "
        ".dual_plate_pair_gate, and .pair_align) but they could not be imported; "
        "install/repair the studio Python deps: "
        "node ai_studio/assets/tools/image/_bridge/setup_python.mjs "
        f"({exc})"
    ) from exc


def mean_brightness(image: Image.Image) -> float:
    """Overall mean RGB value — the background (which fills most of the frame) dominates
    this number, so a white-bg plate reads far brighter than a black-bg plate."""
    return float(np.asarray(image.convert("RGB"), dtype=np.float32).mean())


def detect_roles(image_a: Image.Image, image_b: Image.Image) -> tuple[Image.Image, Image.Image, str, str]:
    """Return (light_image, dark_image, light_label, dark_label) where light_label/
    dark_label are "A"/"B" naming which input plate was chosen for each role."""
    mean_a = mean_brightness(image_a)
    mean_b = mean_brightness(image_b)
    if abs(mean_a - mean_b) < 1e-6:
        raise RuntimeError(
            "dual-plate pair: both plates have the same overall brightness "
            f"({mean_a:.1f}) — cannot tell which is the WHITE background plate and "
            "which is the BLACK background plate. Select the SAME art rendered on a "
            "flat white plate and a flat black plate."
        )
    if mean_a > mean_b:
        return image_a, image_b, "A", "B"
    return image_b, image_a, "B", "A"


def run(spec: dict[str, Any]) -> dict[str, Any]:
    plate_a_path = Path(spec["plateA"])
    plate_b_path = Path(spec["plateB"])
    if not plate_a_path.exists():
        raise FileNotFoundError(f"plate A image missing: {plate_a_path}")
    if not plate_b_path.exists():
        raise FileNotFoundError(f"plate B image missing: {plate_b_path}")
    image_a = Image.open(plate_a_path).convert("RGBA")
    image_b = Image.open(plate_b_path).convert("RGBA")

    light_image, dark_image, light_label, dark_label = detect_roles(image_a, image_b)

    if dark_image.size != light_image.size:
        # Independent AI generations differ by a few px; align canvas to the light
        # plate (mirrors dual_plate_alpha.py's own tolerance) — true subject
        # misalignment is still caught by the pair gate below.
        dark_image = dark_image.resize(light_image.size, Image.Resampling.LANCZOS)

    # T0243: chained gpt-image edits (black = edit of the white plate) routinely
    # drift the subject a few px between plates — the pair gate's own "align"
    # verdict says as much ("a translation align may rescue it"). Search for that
    # rescue BEFORE gating: align_pair reuses the gate's own inconsistency metric
    # as its objective, never returns a shift worse than (0, 0), and a pair that
    # was already aligned comes back unchanged.
    align_fraction_before = evaluate_pair(light_image, dark_image)["inconsistent_fraction"]
    align_dx, align_dy, align_fraction_after, dark_image = align_pair(light_image, dark_image)

    # Pipeline gate (reused, unmodified): a misaligned/redrawn pair produces ghosted
    # alpha, so refuse it here instead of letting a bad pair become a "cut" element.
    # Gates the ALIGNED pair — a shift that rescues an "align" verdict into "pass"
    # proceeds; one that can't (subject actually redrawn) still refuses.
    pair = evaluate_pair(light_image, dark_image)
    if pair["verdict"] == "regenerate":
        raise RuntimeError(
            "dual-plate pair failed the consistency gate "
            f"(inconsistent_fraction={pair['inconsistent_fraction']}, "
            f"mean_edge_chroma={pair['mean_edge_chroma']}): {pair['advice']}"
        )

    result = extract_dual_plate_alpha(light_image, dark_image)
    stats_report = build_report(result, removed_blob_pixels=0)
    if stats_report["verdict"] != "pass":
        raise RuntimeError(f"dual-plate extraction failed: {'; '.join(stats_report['problems'])}")

    output_path = Path(spec["output"])
    output_path.parent.mkdir(parents=True, exist_ok=True)
    save_image_atomic(result, output_path)

    report = {
        "schema": "ai_studio.canvas.alpha_dualplate_report.v1",
        "plateA": plate_a_path.as_posix(),
        "plateB": plate_b_path.as_posix(),
        "light_plate": light_label,
        "dark_plate": dark_label,
        "output": output_path.as_posix(),
        "width": result.width,
        "height": result.height,
        "visible_pixels": stats_report["visible_pixels"],
        "alpha_bbox": stats_report["alpha_bbox"],
        "pair_gate": {key: pair[key] for key in ("verdict", "inconsistent_fraction", "mean_edge_chroma")},
        "align": {
            "dx": align_dx,
            "dy": align_dy,
            "fraction_before": round(align_fraction_before, 4),
            "fraction_after": round(align_fraction_after, 4),
        },
    }
    report_path = spec.get("report")
    if report_path:
        write_json_atomic(Path(report_path), report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Dual-plate alpha cutout: a white-plate + black-plate PAIR -> one RGBA "
        "PNG, via the image-tools dual_plate_alpha + pair-gate modules (reused unmodified)."
    )
    parser.add_argument("--spec", required=True, type=Path, help="dual-plate spec JSON (ai_studio.canvas.alpha_dualplate_spec.v1)")
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text(encoding="utf-8"))
    try:
        report = run(spec)
    except (RuntimeError, ValueError, FileNotFoundError) as exc:
        # Deliberate refusals (pair-gate regenerate verdict, ambiguous roles, a failed
        # post-extraction sanity check, a missing plate) travel the worker's SystemExit
        # path as ONE clean message — the operator-facing toast must not show a Python
        # traceback. Unexpected bugs still traceback loudly.
        raise SystemExit(str(exc)) from exc
    print(
        f"pass: dual-plate alpha (plate {report['light_plate']}=light/white, "
        f"plate {report['dark_plate']}=dark/black, {report['visible_pixels']} visible pixels)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
