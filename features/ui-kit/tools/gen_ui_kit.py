#!/usr/bin/env python3
"""Draw the slice9 GUI kit from a token sheet.

The tokens are the input, not a constant: `--tokens` names a sheet in the shape
of tokens/studio_default.json, and a game that wants its own face passes its own
copy. What stays fixed is the SYSTEM the art encodes -- one rim thickness, one
lift, art that is grayscale where the runtime tints it and in real token colours
where its role is fixed, and slice9 borders that contain each corner.

Grayscale art (button, slider fill/thumb) exists so a multiply tint yields
fill = the action colour and rim + lift = its deep step; that is why one button
image serves every action role. Art with a fixed role (panel, inset track, tile)
is drawn in real token colours.

The slice9 borders written here must match the ones the consumer's pack builder
declares, at the same export scale. Every curved primitive is supersampled
before its shipping resize.

Usage:
  python gen_ui_kit.py [--tokens <sheet.json>] [--out <assets/ui dir>]
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw

FEATURE_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_TOKENS = FEATURE_ROOT / "tokens" / "studio_default.json"


def rgb(value: str):
    text = value.lstrip("#")
    return (int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16))


class Kit:
    """One token sheet, resolved into the numbers the drawing code needs."""

    def __init__(self, tokens: dict, out: Path):
        colors = tokens["colors"]
        art = tokens["art"]
        self.out = out
        self.shell = rgb(colors["shell"])
        self.panel = rgb(colors["panel"])
        self.inset = rgb(colors["inset"])
        self.inset_rim = rgb(colors["inset_rim"])
        self.tile = rgb(colors["tile"])
        self.tile_rim = rgb(colors["tile_rim"])
        self.header = rgb(colors.get("header", colors["shell"]))
        self.rim = int(tokens["geometry"]["rim"])
        self.lift = int(tokens["geometry"]["lift"])
        self.export_scale = int(art["export_scale"])
        self.supersample = int(art["supersample"])
        self.resample = Image.Resampling[art.get("resample", "BOX")]
        self.center = int(art["center"])
        # Deep step of an action colour; the grayscale art encodes that ratio so
        # one white slice9 tints into every action colour.
        self.deep = int(255 * float(art["deep_step"]))
        # The ledge under a button body: a mid grey, so the tint leaves it a
        # darker shade of the fill rather than black.
        self.ledge = int(255 * float(art.get("ledge_step", art["deep_step"])))
        # Light band along the top of a coloured body: how far its colour travels
        # towards white. 0 leaves the art flat, which is why an omitted key draws
        # exactly what it drew before.
        self.gloss = float(art.get("gloss", 0.0))
        # Rivets: a plate that is bolted down rather than moulded. Absent key =
        # no rivets, which is why an omitted block draws what it drew before.
        rivet = art.get("rivet") or {}
        self.rivet = float(rivet.get("radius", 0.0))
        self.rivet_inset = float(rivet.get("inset", 0.0))
        self.radius = {key: int(value) for key, value in art["radius"].items()}
        self.slice9 = {key: tuple(int(v) for v in value) for key, value in art["slice9"].items()}
        self.draw_scale = self.supersample * self.export_scale
        # What still goes through an engine widget that bakes borders 1:1
        # (neotolis-engine#349) has to stay at design size.
        self.bar_art = ("slider_thumb.png",)
        # A flat glyph never drawn larger than a badge does not need the full export.
        self.half_art = ("icon_play.png",)

    def scaled_box(self, box):
        left, top, right, bottom = box
        return (
            left * self.draw_scale,
            top * self.draw_scale,
            (right + 1) * self.draw_scale - 1,
            (bottom + 1) * self.draw_scale - 1,
        )

    def rounded(self, draw: ImageDraw.ImageDraw, box, radius, fill):
        draw.rounded_rectangle(self.scaled_box(box), radius=radius * self.draw_scale, fill=fill)

    def body(self, img: Image.Image, box, radius, fill, fade, top=None):
        """A body with the kit's light catch along its top edge.

        The ramp finishes inside the top slice9 border and everything below it is
        one flat colour, so the stretched middle of a nine-slice cannot show a
        seam. `top` overrides the lit colour: grayscale art that a runtime tint
        multiplies has no headroom above white, so it ramps the other way --
        white at the edge down to the flat body.
        """
        draw = ImageDraw.Draw(img)
        self.rounded(draw, box, radius, fill + (255,))
        if self.gloss <= 0.0:
            return
        if top is None:
            top = tuple(int(c + (255 - c) * self.gloss) for c in fill)
        left, upper, right, lower = self.scaled_box(box)
        depth = max(1, int(fade * self.draw_scale))
        band = Image.new("RGB", (1, depth))
        for y in range(depth):
            t = y / depth
            # Smoothstep: the ramp meets the flat body with no slope, so the
            # junction cannot show as a line across a stretched nine-slice.
            t = t * t * (3.0 - 2.0 * t)
            band.putpixel((0, y), tuple(int(top[i] + (fill[i] - top[i]) * t) for i in range(3)))
        shape = Image.new("L", img.size, 0)
        ImageDraw.Draw(shape).rounded_rectangle(
            (left, upper, right, lower), radius=radius * self.draw_scale, fill=255
        )
        # Only the ramp's own rows are pasted, so the flat body underneath stays
        # exactly the token colour.
        cut = shape.crop((0, int(upper), img.width, int(upper) + depth))
        img.paste(band.resize((img.width, depth)), (0, int(upper)), cut)

    def dot(self, draw: ImageDraw.ImageDraw, cx, cy, r, fill):
        draw.ellipse(self.scaled_box((cx - r, cy - r, cx + r, cy + r)), fill=fill)

    def rivets(self, draw: ImageDraw.ImageDraw, box, fill):
        """Four bolt heads, one per corner of a plate.

        They sit inside the slice9 CORNER regions, which a nine-slice copies
        rather than stretches, so a plate of any size keeps exactly four.
        """
        if self.rivet <= 0.0:
            return
        left, top, right, bottom = box
        i = self.rivet_inset
        shadow = tuple(int(c * 0.5) for c in fill) + (255,)
        head = tuple(int(c + (255 - c) * 0.45) for c in fill) + (255,)
        for cx, cy in ((left + i, top + i), (right - i, top + i), (left + i, bottom - i), (right - i, bottom - i)):
            self.dot(draw, cx, cy + 0.6, self.rivet + 0.5, shadow)
            self.dot(draw, cx, cy, self.rivet, head)

    def canvas(self, width: int, height: int) -> Image.Image:
        return Image.new("RGBA", (width * self.draw_scale, height * self.draw_scale), (0, 0, 0, 0))

    def compact9(self, img: Image.Image, borders, scale: int) -> Image.Image:
        """Corners + edge strips + a CENTER-wide middle, at export resolution.

        A 9-slice never shows the middle of a shipped image: it samples the
        centre and stretches it, so everything between the borders is atlas
        space the game pays for and never displays.
        """
        left, right, top, bottom = (b * scale for b in borders)
        c = self.center * scale
        w, h = img.size
        assert left + right < w and top + bottom < h, "slice9 borders exceed the art"
        mx = left + (w - left - right - c) // 2
        my = top + (h - top - bottom - c) // 2
        out = Image.new("RGBA", (left + c + right, top + c + bottom), (0, 0, 0, 0))
        out.paste(img.crop((0, 0, left, top)), (0, 0))
        out.paste(img.crop((w - right, 0, w, top)), (left + c, 0))
        out.paste(img.crop((0, h - bottom, left, h)), (0, top + c))
        out.paste(img.crop((w - right, h - bottom, w, h)), (left + c, top + c))
        out.paste(img.crop((mx, 0, mx + c, top)), (left, 0))
        out.paste(img.crop((mx, h - bottom, mx + c, h)), (left, top + c))
        out.paste(img.crop((0, my, left, my + c)), (0, top))
        out.paste(img.crop((w - right, my, w, my + c)), (left + c, top))
        out.paste(img.crop((mx, my, mx + c, my + c)), (left, top))
        return out

    def save(self, img: Image.Image, name: str):
        scale = self.export_scale
        if name in self.bar_art:
            scale = 1
        elif name in self.half_art:
            scale = self.export_scale // 2
        divisor = self.supersample * (self.export_scale // scale)
        img = img.resize((img.width // divisor, img.height // divisor), self.resample)
        key = name[: -len(".png")]
        if key in self.slice9:
            img = self.compact9(img, self.slice9[key], scale)
        self.out.mkdir(parents=True, exist_ok=True)
        img.save(self.out / name)
        print(f"wrote {name} {img.size[0]}x{img.size[1]}")

    def save_downscaled(self, name: str, small_name: str):
        img = Image.open(self.out / name)
        small = img.resize(
            (img.width // self.export_scale, img.height // self.export_scale),
            self.resample,
        )
        small.save(self.out / small_name)
        print(f"wrote {small_name} {small.size[0]}x{small.size[1]}")


def gen_panel(kit: Kit):
    # The full corner radius remains inside the panel's own slice.
    s, r = 64, kit.radius["panel"]
    img = kit.canvas(s, s)
    d = ImageDraw.Draw(img)
    kit.rounded(d, (0, 0, s - 1, s - 1), r, kit.shell + (255,))
    kit.body(img, (kit.rim, kit.rim, s - 1 - kit.rim, s - 1 - kit.rim), r - kit.rim, kit.panel, kit.slice9["panel"][2])
    kit.rivets(ImageDraw.Draw(img), (kit.rim, kit.rim, s - 1 - kit.rim, s - 1 - kit.rim), kit.panel)
    kit.save(img, "panel.png")


def gen_button(kit: Kit):
    # For tint: the rim is the contour colour (white tint keeps it, an action
    # tint only darkens it), the ledge a mid grey, the body white.
    s, r = 64, kit.radius["button"]
    img = kit.canvas(s, s)
    d = ImageDraw.Draw(img)
    kit.rounded(d, (0, 0, s - 1, s - 1), r, kit.shell + (255,))
    kit.rounded(d, (kit.rim, kit.rim, s - 1 - kit.rim, s - 1 - kit.rim), r - kit.rim, (kit.ledge, kit.ledge, kit.ledge, 255))
    # The body sits LIFT higher than the deep base = the pressable ledge. Its
    # flat value is below white so the top edge has somewhere to catch light.
    flat = int(255 * (1.0 - kit.gloss))
    kit.body(
        img,
        (kit.rim, kit.rim, s - 1 - kit.rim, s - 1 - kit.rim - kit.lift),
        r - kit.rim,
        (flat, flat, flat),
        kit.slice9["button"][2],
        top=(255, 255, 255),
    )
    kit.save(img, "button.png")


def gen_tile(kit: Kit):
    # Item/card surface with its own rim; fixed colours, so a rarity or state
    # tint can ride over it later without fighting a baked hue.
    s, r = 64, kit.radius["tile"]
    img = kit.canvas(s, s)
    d = ImageDraw.Draw(img)
    kit.rounded(d, (0, 0, s - 1, s - 1), r, kit.tile_rim + (255,))
    kit.body(img, (kit.rim, kit.rim, s - 1 - kit.rim, s - 1 - kit.rim), r - kit.rim, kit.tile, kit.slice9["tile"][2])
    kit.rivets(ImageDraw.Draw(img), (kit.rim, kit.rim, s - 1 - kit.rim, s - 1 - kit.rim), kit.tile)
    kit.save(img, "tile.png")


def gen_header(kit: Kit):
    # The band across the top of a titled plate, inside the panel's rim: round
    # on top only, the bottom corners pushed past the canvas so they stay square.
    s, r = 64, kit.radius.get("header", kit.radius["panel"] - kit.rim)
    img = kit.canvas(s, s)
    d = ImageDraw.Draw(img)
    kit.rounded(d, (0, 0, s - 1, s - 1 + r), r, kit.header + (255,))
    kit.save(img, "header.png")


def gen_slider(kit: Kit):
    w, h, r = 48, 24, kit.radius["bar"]
    # Track: recessed inset with rim, fixed colours.
    img = kit.canvas(w, h)
    d = ImageDraw.Draw(img)
    kit.rounded(d, (0, 0, w - 1, h - 1), r, kit.inset_rim + (255,))
    kit.rounded(d, (kit.rim, kit.rim, w - 1 - kit.rim, h - 1 - kit.rim), r - kit.rim, kit.inset + (255,))
    kit.save(img, "slider_track.png")

    # Fill: white pill, tinted at runtime. Slightly inset so the track rim stays
    # visible at a full bar.
    img = kit.canvas(w, h)
    d = ImageDraw.Draw(img)
    kit.rounded(
        d,
        (kit.rim, kit.rim, w - 1 - kit.rim, h - 1 - kit.rim),
        r - kit.rim,
        (255, 255, 255, 255),
    )
    kit.save(img, "slider_fill.png")

    # Thumb: white circle with a deep rim, tintable; doubles as a joystick thumb
    # and a milestone dot.
    s = 32
    img = kit.canvas(s, s)
    d = ImageDraw.Draw(img)
    d.ellipse(kit.scaled_box((0, 0, s - 1, s - 1)), fill=kit.shell + (255,))
    d.ellipse(
        kit.scaled_box((kit.rim, kit.rim, s - 1 - kit.rim, s - 1 - kit.rim)),
        fill=(255, 255, 255, 255),
    )
    kit.save(img, "slider_thumb.png")


def gen_play(kit: Kit):
    # White play triangle: a glyph, not art -- it rides a tinted badge.
    s = 32
    img = kit.canvas(s, s)
    d = ImageDraw.Draw(img)
    scale = s * kit.draw_scale
    d.polygon(
        [
            (int(scale * 0.3), int(scale * 0.18)),
            (int(scale * 0.3), int(scale * 0.82)),
            (int(scale * 0.85), int(scale * 0.5)),
        ],
        fill=(255, 255, 255, 255),
    )
    kit.save(img, "icon_play.png")


def generate(tokens: dict, out: Path):
    kit = Kit(tokens, out)
    gen_panel(kit)
    gen_button(kit)
    gen_tile(kit)
    gen_header(kit)
    gen_slider(kit)
    gen_play(kit)
    # The engine slider bakes its slice9 borders at source pixel size, so it gets
    # design-size copies of the same two pills.
    kit.save_downscaled("slider_track.png", "slider_track_sm.png")
    kit.save_downscaled("slider_fill.png", "slider_fill_sm.png")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Draw the slice9 GUI kit from a token sheet.")
    parser.add_argument("--tokens", type=Path, default=DEFAULT_TOKENS, help="Token sheet JSON.")
    parser.add_argument("--out", type=Path, required=True, help="Consumer's assets/ui directory.")
    args = parser.parse_args(argv)
    tokens = json.loads(args.tokens.read_text(encoding="utf-8"))
    if tokens.get("schema") != "ai_studio.ui_kit.tokens.v1":
        raise SystemExit(f"{args.tokens}: expected schema ai_studio.ui_kit.tokens.v1")
    generate(tokens, args.out.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
