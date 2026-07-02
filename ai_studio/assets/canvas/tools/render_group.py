#!/usr/bin/env python3
"""Composite a canvas group's visible member elements into ONE screen PNG.

This is the canvas module's OWN Python tool (not a raster2d step). ops.mjs writes
a render spec JSON (absolute paths, group bounds, scale, background, a RECURSIVE
z-ordered `children` tree) and spawns this script directly. Compositing rules mirror
ops.compositeGroup:

  * canvas size = round(group.w * scale) x round(group.h * scale)
  * background  = transparent, or a solid "#rrggbb" fill (the top group's band)
  * `children` are painted BACK -> FRONT; an element draws at its display box
    (element.w/h) scaled by `scale`, offset relative to the top group origin, and
    alpha-composited so overlap + transparency are correct; a nested group paints
    its own background band (if any) then its children at absolute offsets
  * a clip:false subgroup paints into the parent layer so overflow is preserved; a
    clip:true subgroup composites its background band + subtree onto its OWN box-sized
    layer (cropping overflow) then pastes that cropped layer into the parent at its
    offset — nested clips intersect naturally as each cropped layer pastes into the next
  * anything outside the top group box is clipped (the canvas IS the group box)

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


def paint_element(out: Image.Image, node: dict[str, Any], origin: tuple[float, float], scale: float) -> None:
    """Paste one element image onto `out` at its scaled offset (relative to `origin`).

    Pasting onto a same-size transparent layer clips negative/overflow offsets to `out`'s
    bounds, then alpha-composites for correct z-order blending — so a clip layer (which is
    the group's own box size) crops any child that sticks out."""
    source = Path(node["src"])
    if not source.exists():
        raise FileNotFoundError(f"element image missing: {source}")
    image = Image.open(source).convert("RGBA")
    box_w = scaled_len(node.get("w") or image.width, scale)
    box_h = scaled_len(node.get("h") or image.height, scale)
    if (image.width, image.height) != (box_w, box_h):
        image = image.resize((box_w, box_h), Image.Resampling.LANCZOS)
    px = round((float(node.get("x") or 0) - origin[0]) * scale)
    py = round((float(node.get("y") or 0) - origin[1]) * scale)
    layer = Image.new("RGBA", out.size, (0, 0, 0, 0))
    layer.paste(image, (px, py), image)
    out.alpha_composite(layer)


def paint_children(out: Image.Image, children: Any, origin: tuple[float, float], scale: float) -> int:
    """Paint `children` BACK -> FRONT directly onto `out`, an RGBA layer whose (0,0) maps to
    world `origin`. Returns the count of image elements drawn.

    A clip:true subgroup composites its background band + subtree onto its OWN box-sized
    layer (so descendants outside the box are cropped), then pastes that cropped layer into
    `out` at the group offset. A clip:false subgroup fills its band + paints its children
    directly onto `out` at absolute offsets (overflow preserved). Nested clips intersect
    naturally: an inner clip layer pastes into the outer clip layer, which pastes into `out`.
    """
    ox, oy = origin
    drawn = 0
    for node in children or []:
        kind = node.get("kind")
        if kind == "element":
            paint_element(out, node, origin, scale)
            drawn += 1
        elif kind == "group":
            gx = round((float(node.get("x") or 0) - ox) * scale)
            gy = round((float(node.get("y") or 0) - oy) * scale)
            gw = scaled_len(node.get("w") or 1, scale)
            gh = scaled_len(node.get("h") or 1, scale)
            if node.get("clip"):
                # The clip box IS the sub-layer: its background fills it, children paint in
                # the group's local coords, and anything past its edges is cropped.
                sub = Image.new("RGBA", (gw, gh), parse_background(node.get("background")))
                drawn += paint_children(sub, node.get("children"), (float(node.get("x") or 0), float(node.get("y") or 0)), scale)
                layer = Image.new("RGBA", out.size, (0, 0, 0, 0))
                layer.paste(sub, (gx, gy))  # paste clips the cropped sub-layer to out's bounds
                out.alpha_composite(layer)
            else:
                if node.get("background"):
                    band = Image.new("RGBA", (gw, gh), parse_background(node.get("background")))
                    layer = Image.new("RGBA", out.size, (0, 0, 0, 0))
                    layer.paste(band, (gx, gy))
                    out.alpha_composite(layer)
                drawn += paint_children(out, node.get("children"), origin, scale)
    return drawn


def render(spec: dict[str, Any]) -> dict[str, Any]:
    scale = float(spec.get("scale") or 1)
    group = spec.get("group") or {}
    origin_x = float(group.get("x") or 0)
    origin_y = float(group.get("y") or 0)
    width = scaled_len(group.get("w") or 1, scale)
    height = scaled_len(group.get("h") or 1, scale)

    canvas = Image.new("RGBA", (width, height), parse_background(spec.get("background")))

    # Accept the recursive `children` tree; fall back to a flat `elements` list for any
    # older/simple spec (each element becomes an element node). The top group is always
    # cropped to its bounds (the canvas IS the group box); subgroup clip is per-node.
    children = spec.get("children")
    if children is None:
        children = [{"kind": "element", **element} for element in spec.get("elements") or []]
    drawn = paint_children(canvas, children, (origin_x, origin_y), scale)

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
