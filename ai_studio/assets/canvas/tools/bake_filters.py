#!/usr/bin/env python3
"""Bake a canvas element's CURRENT non-destructive filters + opacity into a NEW source
PNG (T0274 "Apply" — Photoshop-rasterize semantics: "принял -> получил новый арт ->
ползунки снова в 0"). This is the canvas module's OWN Python entry (not a raster2d
step), mirroring alpha_cutout.py's shape: ops.bakeFilters writes a spec JSON (absolute
source path + the element's current filters/opacity) and spawns this script once
through the shared warm worker.

Spec (schema ai_studio.canvas.bake_filters_spec.v1):
  {source, output, filters?: {brightness?,saturation?,contrast?,tint?}, opacity?, report?}

Math (single source of truth — see README "Image filters"):
  1. Load the SOURCE file at its full source resolution (no resize; output dimensions
     always equal the source, exactly like alpha_cutout.py, so the element's box/regions
     never change on a src swap).
  2. `filters_math.apply_filters` — the SAME canonical brightness -> saturate -> contrast
     -> tint chain `render_group.py`'s live render uses (imported, never re-derived).
  3. Opacity -> alpha: multiplies the alpha channel by `opacity` using the EXACT formula
     `render_group.py`'s `paint_element` applies at composite time
     (`image.putalpha(image.getchannel("A").point(lambda a: round(a * factor)))`,
     `factor` clamped to [0,1]) — so the baked pixels are bit-identical to what the
     on-canvas preview showed. This is the ONLY place opacity is burned into pixels;
     `render_group.py` still applies opacity at composite time for every OTHER
     (non-baked) element, so nothing here double-applies it.

CPU + numpy/Pillow only, through the studio venv.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from PIL import Image

ROOT = Path(__file__).resolve().parents[4]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import save_image_atomic, write_json_atomic
from ai_studio.assets.canvas.tools.filters_math import apply_filters


def run(spec: dict[str, Any]) -> dict[str, Any]:
    source = Path(spec["source"])
    if not source.exists():
        raise FileNotFoundError(f"source image missing: {source}")
    image = Image.open(source).convert("RGBA")

    filters = spec.get("filters")
    image = apply_filters(image, filters)

    # Opacity -> alpha (T0274; opacity-to-alpha lives ONLY here — render_group.py applies
    # it at composite time for every other, non-baked element). Absent/1 is a no-op, same
    # "absent means the default" convention `element.opacity` already uses.
    opacity_value = spec.get("opacity")
    baked_opacity = 1.0
    if opacity_value is not None:
        baked_opacity = float(opacity_value)
        if baked_opacity < 1:
            factor = max(0.0, min(1.0, baked_opacity))
            image.putalpha(image.getchannel("A").point(lambda a: round(a * factor)))

    output = Path(spec["output"])
    output.parent.mkdir(parents=True, exist_ok=True)
    save_image_atomic(image, output)

    report = {
        "schema": "ai_studio.canvas.bake_filters_report.v1",
        "source": source.as_posix(),
        "output": output.as_posix(),
        "filters": filters or None,
        "opacity": baked_opacity,
        "width": image.width,
        "height": image.height,
    }
    report_path = spec.get("report")
    if report_path:
        write_json_atomic(Path(report_path), report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description="Bake a canvas element's current filters + opacity into a new source PNG.")
    parser.add_argument("--spec", required=True, type=Path, help="bake spec JSON (ai_studio.canvas.bake_filters_spec.v1)")
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text(encoding="utf-8"))
    try:
        report = run(spec)
    except (RuntimeError, ValueError, FileNotFoundError) as exc:
        # Deliberate refusals (missing source, bad spec) travel the worker's SystemExit
        # path as ONE clean message — the operator-facing toast must not show a Python
        # traceback. Unexpected bugs still traceback loudly.
        raise SystemExit(str(exc)) from exc
    print(f"pass: baked filters -> {report['width']}x{report['height']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
