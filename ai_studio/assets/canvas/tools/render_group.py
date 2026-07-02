#!/usr/bin/env python3
"""Composite a canvas group's visible member elements into ONE screen PNG.

This is the canvas module's OWN Python tool (not a raster2d step). ops.mjs writes
a render spec JSON (absolute paths, group bounds, scale, background) and spawns
this script directly. Compositing rules mirror ops.renderGroup:

  * canvas size = round(group.w * scale) x round(group.h * scale)
  * background  = transparent, or a solid "#rrggbb" fill
  * each element is drawn (in spec order = z-order) at its display box
    (element.w/h) scaled by `scale`, offset relative to the group origin, and
    alpha-composited so overlap and transparency are correct
  * anything outside the group box is clipped (the canvas IS the group box)

Outputs are written atomically; a small JSON report is emitted for provenance.
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


def parse_background(value: Any) -> tuple[int, int, int, int]:
    """Return an RGBA fill: transparent when no/empty background, else the solid color."""
    if not value:
        return (0, 0, 0, 0)
    text = str(value).strip().lstrip("#")
    if len(text) != 6:
        raise ValueError("background must be #rrggbb")
    r, g, b = (int(text[i : i + 2], 16) for i in (0, 2, 4))
    return (r, g, b, 255)


def scaled_len(value: float, scale: float) -> int:
    return max(1, round(float(value) * scale))


def render(spec: dict[str, Any]) -> dict[str, Any]:
    scale = float(spec.get("scale") or 1)
    group = spec.get("group") or {}
    origin_x = float(group.get("x") or 0)
    origin_y = float(group.get("y") or 0)
    width = scaled_len(group.get("w") or 1, scale)
    height = scaled_len(group.get("h") or 1, scale)

    canvas = Image.new("RGBA", (width, height), parse_background(spec.get("background")))

    drawn = 0
    for element in spec.get("elements") or []:
        source = Path(element["src"])
        if not source.exists():
            raise FileNotFoundError(f"element image missing: {source}")
        image = Image.open(source).convert("RGBA")
        box_w = scaled_len(element.get("w") or image.width, scale)
        box_h = scaled_len(element.get("h") or image.height, scale)
        if (image.width, image.height) != (box_w, box_h):
            image = image.resize((box_w, box_h), Image.Resampling.LANCZOS)
        px = round((float(element.get("x") or 0) - origin_x) * scale)
        py = round((float(element.get("y") or 0) - origin_y) * scale)
        # Paste onto a full-canvas transparent layer (paste clips negative/overflow
        # offsets to the group box), then alpha-composite for correct z-order blending.
        layer = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        layer.paste(image, (px, py), image)
        canvas = Image.alpha_composite(canvas, layer)
        drawn += 1

    output = Path(spec["output"])
    save_image_atomic(canvas, output)

    report = {
        "schema": "ai_studio.canvas.render_group_report.v1",
        "output": output.as_posix(),
        "width": width,
        "height": height,
        "scale": scale,
        "background": spec.get("background"),
        "element_count": drawn,
    }
    report_path = spec.get("report")
    if report_path:
        write_json_atomic(Path(report_path), report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description="Composite a canvas group's visible elements into one screen PNG.")
    parser.add_argument("--spec", required=True, type=Path, help="render spec JSON (ai_studio.canvas.render_group_spec.v1)")
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text(encoding="utf-8"))
    report = render(spec)
    print(f"pass: rendered {report['element_count']} element(s) -> {report['width']}x{report['height']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
