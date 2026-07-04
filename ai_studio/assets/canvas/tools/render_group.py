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

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[4]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import save_image_atomic, write_json_atomic
# T0233: the exact Python twin of slice9.mjs — see its module docstring for the
# shared model/algorithm; paint_element's slice9 branch below calls slice9_patches.
from ai_studio.assets.canvas.tools.slice9 import slice9_patches


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


def apply_filters(image: Image.Image, filters: dict[str, Any] | None) -> Image.Image:
    """Apply the canonical non-destructive filter chain — brightness -> saturate ->
    contrast -> tint — to an RGBA image's COLOR channels; alpha is never touched (a
    fully transparent pixel stays fully transparent, a semi-transparent one keeps its
    alpha exactly). This is the ONE canonical math both renderers implement identically
    (see README "Image filters"): PIL here is the source of rendered truth; the canvas's
    own paintElement approximates the SAME formulas via the browser's spec'd CSS
    `filter: brightness() saturate() contrast()` plus an offscreen source-atop tint scrim.

    Order (matches `site/workspace.js` byte-for-byte, same non-premultiplied sRGB [0,1]
    channel math):
      1. brightness: C' = clamp(C * b)
      2. saturate (SVG matrix, luma 0.2126/0.7152/0.0722 — NOT PIL's default 0.299/0.587/
         0.114 grayscale luma; saturation=0 must land on the FORMER, the whole reason this
         is hand-rolled in numpy instead of `ImageEnhance.Color`)
      3. contrast: C' = clamp((C - 0.5) * c + 0.5)
      4. tint: linear RGB mix toward `tint.color` by `tint.strength`, alpha untouched

    No-op (returns `image` unchanged, no numpy work) when `filters` is empty/absent or
    every value is already at its default — an unfiltered element pays zero extra cost,
    the pre-existing fast path."""
    if not filters:
        return image
    brightness = float(filters.get("brightness", 1))
    saturation = float(filters.get("saturation", 1))
    contrast = float(filters.get("contrast", 1))
    tint = filters.get("tint")
    tint_strength = float(tint.get("strength", 0)) if tint else 0.0
    if brightness == 1 and saturation == 1 and contrast == 1 and tint_strength <= 0:
        return image

    src = np.asarray(image, dtype=np.uint8)
    alpha = src[..., 3]  # untouched, reassembled verbatim at the end
    rgb = src[..., :3].astype(np.float32) / 255.0

    if brightness != 1:
        rgb = rgb * brightness

    if saturation != 1:
        s = saturation
        r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
        rgb = np.stack(
            [
                (0.2126 + 0.7874 * s) * r + (0.7152 - 0.7152 * s) * g + (0.0722 - 0.0722 * s) * b,
                (0.2126 - 0.2126 * s) * r + (0.7152 + 0.2848 * s) * g + (0.0722 - 0.0722 * s) * b,
                (0.2126 - 0.2126 * s) * r + (0.7152 - 0.7152 * s) * g + (0.0722 + 0.9278 * s) * b,
            ],
            axis=-1,
        )

    if contrast != 1:
        rgb = (rgb - 0.5) * contrast + 0.5

    rgb = np.clip(rgb, 0.0, 1.0)

    if tint_strength > 0:
        hexs = str(tint["color"]).lstrip("#")
        tint_rgb = np.array([int(hexs[i : i + 2], 16) / 255.0 for i in (0, 2, 4)], dtype=np.float32)
        rgb = rgb * (1 - tint_strength) + tint_rgb * tint_strength
        rgb = np.clip(rgb, 0.0, 1.0)

    out_rgb = np.round(rgb * 255.0).astype(np.uint8)
    out = np.dstack([out_rgb, alpha])
    return Image.fromarray(out, mode="RGBA")


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
        declared acceptable approximation (same stance as text rendering).

    Slice-9 (T0233): a `slice9` node REPLACES the plain resize with a 9-patch
    assembly at the SAME box_w x box_h, so the result flows into the UNCHANGED
    flip -> rotate -> paste chain below unmodified — the drop-in composition
    contract (design section 4.0): `image` is assigned BEFORE those steps, exactly
    like the plain resize it replaces, so a rotated/flipped slice9 panel composes
    for free with no extra code here.

    Image filters (brightness/saturation/contrast/tint): applied via `apply_filters`
    right after the resize/slice9 step, BEFORE flip/rotate — per-pixel color ops commute
    with geometry, so this ordering is a free choice; it is picked here so filters read
    on the UNTRANSFORMED (post-resize) pixels, mirroring the canvas's own paintElement
    (which applies ctx.filter / the tint offscreen to the same pre-rotate image). See
    README "Image filters" for the canonical shared math."""
    source = Path(node["src"])
    if not source.exists():
        raise FileNotFoundError(f"element image missing: {source}")
    image = Image.open(source).convert("RGBA")
    box_w = scaled_len(node.get("w") or image.width, scale)
    box_h = scaled_len(node.get("h") or image.height, scale)
    slice9 = node.get("slice9")
    if slice9:
        # Patches are computed in ELEMENT-LOCAL (unscaled) units against the node's
        # display box, then each patch's crop/paste is scaled by the render `scale`
        # individually — matching how the plain (non-slice9) resize above scales the
        # whole box, and how the canvas's own paintElement scales each patch by
        # vp.scale (see slice9.mjs / workspace.js).
        patches = slice9_patches(slice9, image.width, image.height, node.get("w") or image.width, node.get("h") or image.height)
        sliced = Image.new("RGBA", (box_w, box_h), (0, 0, 0, 0))
        for p in patches:
            crop = image.crop((round(p["sx"]), round(p["sy"]), round(p["sx"] + p["sw"]), round(p["sy"] + p["sh"])))
            dw, dh = max(1, round(p["dw"] * scale)), max(1, round(p["dh"] * scale))
            if (crop.width, crop.height) != (dw, dh):
                crop = crop.resize((dw, dh), Image.Resampling.LANCZOS)
            sliced.paste(crop, (round(p["dx"] * scale), round(p["dy"] * scale)), crop)
        image = sliced
    elif (image.width, image.height) != (box_w, box_h):
        image = image.resize((box_w, box_h), Image.Resampling.LANCZOS)
    # Image filters (brightness/saturation/contrast/tint) are pure per-pixel color ops,
    # so they commute with the flip/rotate geometry below — applied HERE, right after the
    # resize/slice9 step, so a rotated/flipped/sliced filtered element pays no extra code
    # path (same drop-in-before-flip contract as slice9 above; see README "Image filters").
    image = apply_filters(image, node.get("filters"))
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
    # T0273 fix (pre-existing bug, found while testing tint-vs-alpha parity): NO mask here.
    # `layer.paste(image, pos, image)` (passing `image` as its OWN mask) does not copy
    # `image`'s pixels verbatim — PIL blends EVERY channel (RGB **and** alpha) of the
    # destination toward `image` by the mask fraction (image's own alpha / 255). Against
    # this fully-transparent `layer`, that squares any non-opaque source alpha (e.g. a
    # soft-edged alpha-cutout, or a filtered image with a semi-transparent source pixel):
    # alpha 128 pasted this way lands at ~64, not 128. A plain `paste(image, pos)` (no
    # mask) instead COPIES `image`'s bytes exactly into this dedicated, otherwise-empty
    # layer (verified: negative/overflow offsets still clip correctly with no mask), so
    # the one `alpha_composite` below is the single, correct source-over blend.
    layer.paste(image, (cx - image.width // 2, cy - image.height // 2))
    # T0260: static element.opacity ([0,1], absent = 1) scales the element alpha before
    # compositing — parity with the canvas's ctx.globalAlpha. Scaled on the pasted LAYER
    # (whose RGB is straight after a full-opacity paste), NOT on `image` beforehand: reducing
    # image alpha first would make the paste's own alpha-mask premultiply the RGB and then
    # alpha_composite would blend a second time. On the layer, alpha_composite blends exactly
    # once = source-over at that alpha. Clamped defensively; ops.mjs is the loud gate.
    opacity = node.get("opacity")
    if opacity is not None and float(opacity) < 1:
        factor = max(0.0, min(1.0, float(opacity)))
        layer.putalpha(layer.getchannel("A").point(lambda a: round(a * factor)))
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
