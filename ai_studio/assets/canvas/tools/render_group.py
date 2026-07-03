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

from PIL import Image, ImageDraw, ImageFont

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
    """Paste one element image onto `out` at its scaled offset (relative to `origin`),
    honoring an optional rotation/flip (T0232 increment 3a).

    Pasting onto a same-size transparent layer clips negative/overflow offsets to `out`'s
    bounds, then alpha-composites for correct z-order blending — so a clip layer (which is
    the group's own box size) crops any child that sticks out.

    Rotation/flip parity contract (the ONE thing the canvas's own paintElement must agree
    with byte-for-byte on the sign/center convention — see README "Rotation & flip"):
      * composition order is resize -> flip -> rotate -> paste centered (flip innermost).
      * `rotation` is DEGREES CLOCKWISE ON SCREEN, matching the canvas's `ctx.rotate(+theta)`
        on a Y-down canvas; PIL's own `Image.rotate()` is CCW-positive, so this negates.
      * `Image.rotate(angle, expand=True)` returns an image whose OWN center is the original
        image's (pre-rotation) center — pasting that image so ITS center lands on the
        element's box center therefore reproduces "rotate about the box center" exactly.
      * geometry (center/angle/size) is exact by construction; PIL is the single source of
        rendered truth, so a ~1px edge-AA difference from the canvas's own resample is the
        declared acceptable approximation (same stance as text rendering)."""
    source = Path(node["src"])
    if not source.exists():
        raise FileNotFoundError(f"element image missing: {source}")
    image = Image.open(source).convert("RGBA")
    box_w = scaled_len(node.get("w") or image.width, scale)
    box_h = scaled_len(node.get("h") or image.height, scale)
    if (image.width, image.height) != (box_w, box_h):
        image = image.resize((box_w, box_h), Image.Resampling.LANCZOS)
    if node.get("flipH"):
        image = image.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    if node.get("flipV"):
        image = image.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    rotation = float(node.get("rotation") or 0)
    if rotation:
        image = image.rotate(-rotation, resample=Image.Resampling.BICUBIC, expand=True)
    cx = round((float(node.get("x") or 0) + float(node.get("w") or 0) / 2 - origin[0]) * scale)
    cy = round((float(node.get("y") or 0) + float(node.get("h") or 0) / 2 - origin[1]) * scale)
    layer = Image.new("RGBA", out.size, (0, 0, 0, 0))
    layer.paste(image, (cx - image.width // 2, cy - image.height // 2), image)
    out.alpha_composite(layer)


_FONT_CACHE: dict[tuple[str, int], Any] = {}


def load_font(path: str, size: int) -> Any:
    key = (path, size)
    font = _FONT_CACHE.get(key)
    if font is None:
        font = ImageFont.truetype(path, size)
        _FONT_CACHE[key] = font
    return font


def paint_text(out: Image.Image, node: dict[str, Any], origin: tuple[float, float], scale: float) -> None:
    """Draw a text node's lines onto `out` (an RGBA layer whose (0,0) maps to world `origin`).

    NOTE (T0232 increment 3a): the spec forwards `rotation` on a text node too (a text
    element's `rotation` field is valid, "rotates the box"), but glyph pixel rotation is
    NOT yet applied here — deferred to a follow-up increment, scoped out to avoid touching
    the auto-width live-measurement path. `node.get("rotation")` is intentionally unread.

    Parity with the browser canvas (the PIL side is the source of rendered truth):
      * textBaseline top / PIL anchor='la'; per-line origin y = top + i*(fontSize*lineHeight)
      * AUTO-WIDTH: measure every line, align each within the max line width (L/C/R)
      * the stroke GROWS OUTWARD (PIL stroke_width) — the page draws it UNDER the fill with
        lineWidth = 2 x style.stroke.width so the two match
      * the HARD offset shadow is the same glyphs in the shadow color drawn FIRST (blur is
        stored but always 0 in v1)."""
    size = max(1, round(float(node["fontSize"]) * scale))
    font = load_font(node["fontFile"], size)
    draw = ImageDraw.Draw(out)
    lines = node.get("lines") or [""]
    line_step = float(node["fontSize"]) * float(node["lineHeight"]) * scale
    widths = [draw.textlength(line, font=font) for line in lines]
    box_w = max(widths) if widths else 0.0
    align = node.get("align", "left")

    ox, oy = origin
    base_x = (float(node.get("x") or 0) - ox) * scale
    base_y = (float(node.get("y") or 0) - oy) * scale

    def line_x(i: int) -> float:
        if align == "center":
            return base_x + (box_w - widths[i]) / 2.0
        if align == "right":
            return base_x + (box_w - widths[i])
        return base_x

    stroke = node.get("stroke")
    stroke_w = round(float(stroke["width"]) * scale) if stroke else 0
    stroke_fill = stroke["color"] if stroke else None
    shadow = node.get("shadow")

    # Shadow pass FIRST (hard offset, fill only) so the main text always sits on top.
    if shadow:
        sdx = float(shadow.get("dx") or 0) * scale
        sdy = float(shadow.get("dy") or 0) * scale
        for i, line in enumerate(lines):
            draw.text(
                (line_x(i) + sdx, base_y + i * line_step + sdy),
                line,
                font=font,
                fill=shadow["color"],
                anchor="la",
            )
    # Main pass: PIL draws the stroke OUTWARD then the fill inside in one call.
    for i, line in enumerate(lines):
        draw.text(
            (line_x(i), base_y + i * line_step),
            line,
            font=font,
            fill=node["color"],
            anchor="la",
            stroke_width=stroke_w,
            stroke_fill=stroke_fill,
        )


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
        elif kind == "text":
            paint_text(out, node, origin, scale)
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
