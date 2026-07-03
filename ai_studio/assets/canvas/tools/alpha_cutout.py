#!/usr/bin/env python3
"""Alpha-cutout a canvas element's CURRENT pixels, whole-image or per-region.

This is the canvas module's OWN Python entry (not a raster2d step). It REUSES the
image-tools alpha modules verbatim — the soft-score router (route/route_cutout) and
the production key_matte keyer (alpha_matte/key_matte) — so there is no logic
duplication and no second matte implementation. ops.alphaCutout writes a spec JSON
(absolute source path + method + the element's selected regions with their exact
rects) and spawns this script once through the shared warm worker.

Spec (schema ai_studio.canvas.alpha_cutout_spec.v1):
  {source, output, method: "auto"|"matte", regions?: [{id, rect, polygon?}], report?}

Method:
  * "auto"  — route the source (soft_score router). Opaque/flat-key art routes to
              key_matte and is keyed. A WIDE soft/semi-transparent zone (glow, glass,
              soft shadow) routes to dual_plate, which needs a white+black plate PAIR —
              a single element has one image, so that is OUT of canvas v1 scope and this
              raises a LOUD error (no silent fallback to a low-quality single-plate key).
  * "matte" — force key_matte (the prod default keyer since 12354465) regardless of the
              routing recommendation; the key colour is still recovered from the border.

Regions: with regions, alpha is applied ONLY inside each region's mask (rect, or the
polygon when present) and the rest of the element is left UNTOUCHED (its original opaque
pixels). Composition happens HERE, in one worker call — never split across node. Without
regions the whole element is keyed. Output dimensions always equal the source (geometry
is preserved), so the element box never changes on a src swap.

CPU + numpy/scipy/Pillow only, through the studio venv. Missing alpha modules are a LOUD
import error (no-fallbacks law).
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[4]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import save_image_atomic, write_json_atomic

# LAW (lead, 2026-07-02): no silent fallbacks. The canvas alpha op reuses the image-tools
# alpha modules; if they cannot be imported we raise loudly instead of degrading.
try:
    from ai_studio.assets.tools.image.alpha_matte.key_matte import key_matte_cutout
    from ai_studio.assets.tools.image.route.route_cutout import route_cutout
except ImportError as exc:  # pragma: no cover - environment/setup failure
    raise RuntimeError(
        "canvas alpha cutout requires the image-tools alpha modules "
        "(ai_studio.assets.tools.image.alpha_matte.key_matte and .route.route_cutout) but "
        "they could not be imported; install/repair the studio Python deps: "
        f"node ai_studio/assets/tools/image/_bridge/setup_python.mjs ({exc})"
    ) from exc

ALPHA_METHODS = ("auto", "matte")


def clamp_rect(rect: list[Any], size: tuple[int, int]) -> tuple[int, int, int, int]:
    """Integer [x, y, w, h] clamped to the image, returned as (left, top, right, bottom)."""
    if not (isinstance(rect, (list, tuple)) and len(rect) == 4):
        raise ValueError(f"region rect must be [x, y, w, h], got {rect!r}")
    x, y, w, h = (int(round(float(value))) for value in rect)
    width, height = size
    left = max(0, min(x, width))
    top = max(0, min(y, height))
    right = max(left, min(x + w, width))
    bottom = max(top, min(y + h, height))
    if right <= left or bottom <= top:
        raise ValueError(f"region rect {rect!r} is empty after clamping to {width}x{height}")
    return left, top, right, bottom


def alpha_one(image: Image.Image, method: str, label: str) -> tuple[Image.Image, dict[str, Any]]:
    """Key ONE image (whole element or a region crop). Routes for the key colour +
    the dual-plate guard, then runs the prod key_matte keyer."""
    decision = route_cutout(image)  # border-estimated key + soft/opaque routing
    if method == "auto" and decision.needs_dual:
        raise RuntimeError(
            f"{label}: auto routing selected dual_plate ({decision.reason}). A wide "
            "soft/semi-transparent zone (glow, glass, soft shadow) needs a white+black "
            "plate PAIR, which a single canvas element cannot provide (out of v1 scope). "
            "Re-run with method=matte to force key_matte, or regenerate the art with a "
            "dual-plate pair (gen_dual_plate.sh)."
        )
    keyed = key_matte_cutout(image, decision.key)
    return keyed, {
        "method": "key_matte",
        "routed": decision.method,
        "needs_dual": bool(decision.needs_dual),
        "soft_score": decision.soft_score,
        "depth90": decision.depth90,
        "key": [int(channel) for channel in decision.key],
    }


def region_mask(size: tuple[int, int], region: dict[str, Any], origin: tuple[int, int]) -> Image.Image | None:
    """A binary paste mask for a region: None (whole rect) for a plain rect, or an L mask
    filled by the region polygon in crop-local coordinates. Ported from crop_regions."""
    polygon = region.get("polygon")
    if not polygon or len(polygon) < 3:
        return None
    ox, oy = origin
    local = [(int(round(float(point[0]))) - ox, int(round(float(point[1]))) - oy) for point in polygon]
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).polygon(local, fill=255)
    return mask


def run(spec: dict[str, Any]) -> dict[str, Any]:
    method = str(spec.get("method") or "auto").strip().lower()
    if method not in ALPHA_METHODS:
        raise ValueError(f"unknown alpha method {method!r} (expected one of {ALPHA_METHODS})")

    source = Path(spec["source"])
    if not source.exists():
        raise FileNotFoundError(f"source image missing: {source}")
    image = Image.open(source).convert("RGBA")

    regions = spec.get("regions") or []
    region_reports: list[dict[str, Any]] = []

    if regions:
        # Regions-scoped: start from the ORIGINAL opaque pixels and replace only each
        # region's mask with its keyed crop, so everything outside the regions is untouched.
        out = image.copy()
        for index, region in enumerate(regions, start=1):
            label = str(region.get("id") or f"region_{index:03d}")
            left, top, right, bottom = clamp_rect(region.get("rect"), image.size)
            crop = image.crop((left, top, right, bottom))
            keyed, meta = alpha_one(crop, method, label)
            mask = region_mask(keyed.size, region, (left, top))
            out.paste(keyed, (left, top), mask)  # mask None = paste the whole rect verbatim
            region_reports.append({"id": label, "rect": [left, top, right - left, bottom - top], **meta})
        key_color = region_reports[0]["key"] if region_reports else None
    else:
        out, meta = alpha_one(image, method, "element")
        region_reports.append({"id": "*element*", **meta})
        key_color = meta["key"]

    output = Path(spec["output"])
    output.parent.mkdir(parents=True, exist_ok=True)
    save_image_atomic(out, output)

    report = {
        "schema": "ai_studio.canvas.alpha_cutout_report.v1",
        "source": source.as_posix(),
        "output": output.as_posix(),
        "method": method,
        "region_count": len(regions),
        "key_color": key_color,
        "width": out.width,
        "height": out.height,
        "regions": region_reports,
    }
    report_path = spec.get("report")
    if report_path:
        write_json_atomic(Path(report_path), report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description="Alpha-cutout a canvas element (whole or per-region) via the image-tools matte pipeline.")
    parser.add_argument("--spec", required=True, type=Path, help="alpha spec JSON (ai_studio.canvas.alpha_cutout_spec.v1)")
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text(encoding="utf-8"))
    try:
        report = run(spec)
    except (RuntimeError, ValueError, FileNotFoundError) as exc:
        # Deliberate refusals (dual-plate guard, bad spec values, missing source) travel
        # the worker's SystemExit path as ONE clean message — the operator-facing toast
        # must not show a Python traceback. Unexpected bugs still traceback loudly.
        raise SystemExit(str(exc)) from exc
    scope = f"{report['region_count']} region(s)" if report["region_count"] else "whole element"
    print(f"pass: alpha cutout ({report['method']}, {scope})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
