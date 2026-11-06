#!/usr/bin/env python3
"""Build the game font: the engine's Lilita One Rus with the letters it does
not draw in its own hand.

    node ai_studio/dev_environment/python_run.mjs         features/localization/scripts/merge_font_glyphs.py         --base external/neotolis-engine/assets/fonts/LilitaOne-RussianChineseKo.ttf         --out build/native-debug/generated/fonts/GameDisplay.ttf

The build runs this; the result is NOT committed, because it is 14 MB that the
recipe reproduces byte for byte. A game that wants its own face passes its own
--family/--postscript and keeps calling this from its pack step.

Two kinds of letter are fixed here. Seventeen codepoints the base does not carry
at all -- five Turkish (U+011E U+011F U+015E U+015F U+0130) and twelve Polish
(Ą ą Ć ć Ę ę Ś ś Ź ź Ż ż) -- and four it does carry but not in its own hand:
Ń ń Ё ё came in with the Cyrillic/CJK merge from a lighter, narrower typeface
and read as a second font mid-word (engine issue #441). Every one of them is a
letter the base already draws plus a mark, so they are COMPOSED from the base's
own outlines rather than copied out of another typeface. The one mark the base
owns nowhere is the ogonek, mirrored from its own cedilla -- same hand, same
weight, no foreign curve.

Vietnamese is rebuilt the same way: the base carries every letter, but the
stacked ones came from a lighter typeface too. Its tones are this font's own
grave, acute and tilde; the hook above is its question mark without the dot, the
dot below its i dot, the horn its comma, the bar of d its hyphen.

Ё is the one that cannot be written around: Russian copy in this studio spells
е (features/localization), but a portal hands back player names that carry ё.

Placement follows what the designer already did, measured from the base itself
rather than guessed: every uppercase acute in the font sits at y 729..911 and
every lowercase one at y 529..711, horizontally centred on the letter. So a
mark keeps its own vertical position and only moves sideways, which is why the
new letters line up with the accented ones the font already had.

Deterministic: the same inputs produce the same bytes (no timestamp recalc, no
dict-order dependency), so a rebuild that changes nothing changes no file.
"""

from __future__ import annotations

import argparse
import sys
import unicodedata
from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.ttLib.tables import ttProgram
from fontTools.ttLib.tables._g_l_y_f import Glyph, GlyphCoordinates

# The merged font is a Modified Version under the OFL, and both parents reserve
# their names ("Lilita One", "Titan One"), so it may not carry either.
DEFAULT_FAMILY_NAME = "Game Display"
DEFAULT_POSTSCRIPT_NAME = "GameDisplay"

# Name records that spell the family. 0 (copyright), 7 (trademark), 13 and 14
# (license) are notices the OFL requires a derivative to carry unchanged.
FAMILY_NAME_IDS = (1, 4, 16)
UNIQUE_ID_NAME_ID = 3
POSTSCRIPT_NAME_ID = 6

CAP_TOP = 704  # OS/2 sCapHeight of the base


class MergeError(Exception):
    pass


# --- outline plumbing -------------------------------------------------------


def contours_of(font: TTFont, glyph_name: str) -> list[list[tuple[int, int, int]]]:
    """Contours as [(x, y, on_curve)], the form the glyf table stores. Working in
    points rather than through a pen keeps the quadratic curves bit-exact: a
    round trip through cubic pens would resample every outline it copies."""
    glyf = font["glyf"]
    glyph = glyf[glyph_name]
    if glyph.numberOfContours <= 0:
        raise MergeError(f"{glyph_name}: no simple outline to copy")
    coords, end_points, flags = glyph.getCoordinates(glyf)
    contours = []
    start = 0
    for end in end_points:
        contours.append(
            [(int(round(x)), int(round(y)), int(flag) & 0x01)
             for (x, y), flag in zip(coords[start:end + 1], flags[start:end + 1])]
        )
        start = end + 1
    return contours


def bounds(contours) -> tuple[int, int, int, int]:
    points = [p for contour in contours for p in contour]
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    return min(xs), min(ys), max(xs), max(ys)


def translated(contours, dx: int, dy: int):
    return [[(x + dx, y + dy, on) for (x, y, on) in contour] for contour in contours]


def scaled(contours, factor: float):
    return [[(int(round(x * factor)), int(round(y * factor)), on) for (x, y, on) in contour]
            for contour in contours]


def mirrored(contours):
    """Mirror about x=0 and reverse each contour, so the winding direction that
    fills the shape survives the flip."""
    return [[(-x, y, on) for (x, y, on) in reversed(contour)] for contour in contours]


def make_glyph(font: TTFont, contours) -> Glyph:
    glyph = Glyph()
    glyph.numberOfContours = len(contours)
    points = [p for contour in contours for p in contour]
    glyph.coordinates = GlyphCoordinates([(x, y) for (x, y, _) in points])
    glyph.flags = bytearray(on for (_, _, on) in points)
    ends = []
    total = 0
    for contour in contours:
        total += len(contour)
        ends.append(total - 1)
    glyph.endPtsOfContours = ends
    glyph.program = ttProgram.Program()
    glyph.program.fromBytecode(b"")
    glyph.recalcBounds(font["glyf"])
    return glyph


# --- mark library -----------------------------------------------------------


def glyph_for(font: TTFont, codepoint: int) -> str:
    name = font.getBestCmap().get(codepoint)
    if name is None:
        raise MergeError(f"U+{codepoint:04X} is missing from the source font")
    return name


def mark_from(font: TTFont, composed_cp: int, base_cp: int):
    """The contours a composed letter has above its own base letter -- the
    font's own mark, at the font's own height."""
    composed = contours_of(font, glyph_for(font, composed_cp))
    base_top = bounds(contours_of(font, glyph_for(font, base_cp)))[3]
    mark = [contour for contour in composed if bounds([contour])[1] > base_top - 40]
    if not mark:
        raise MergeError(f"U+{composed_cp:04X}: no mark contour above U+{base_cp:04X}")
    return mark


def dot_from(font: TTFont, codepoint: int, above: int):
    """The dot of a dotted letter: every contour that starts above `above`."""
    contours = contours_of(font, glyph_for(font, codepoint))
    dots = [contour for contour in contours if bounds([contour])[1] > above]
    if not dots:
        raise MergeError(f"U+{codepoint:04X}: no dot above {above}")
    return dots


def build_marks(font: TTFont) -> dict:
    """Every mark the new letters need, taken from the letters the font already
    draws. The cedilla is the standalone U+00B8 outline; the ogonek is that same
    outline mirrored, because the base font draws no ogonek anywhere and a
    mirrored cedilla is the only hook in this font's own hand."""
    cedilla = contours_of(font, glyph_for(font, 0x00B8))
    ogonek = mirrored(cedilla)
    return {
        "acute_upper": mark_from(font, 0x00D3, 0x004F),   # Ó
        "acute_lower": mark_from(font, 0x00F3, 0x006F),   # ó
        "breve_upper": mark_from(font, 0x0419, 0x0418),   # Й
        "breve_lower": mark_from(font, 0x0439, 0x0438),   # й
        "dot_upper": [dot_from(font, 0x00CF, CAP_TOP)[0]],  # one Ï dot
        "dot_lower": dot_from(font, 0x0069, CAP_TOP - 200),  # the i dot
        "dieresis_upper": mark_from(font, 0x00CF, 0x0049),  # both Ï dots
        "dieresis_lower": mark_from(font, 0x00EB, 0x0065),  # both ë dots
        "cedilla": cedilla,
        "ogonek": ogonek,
    }


# --- placement --------------------------------------------------------------

ABOVE = "above"      # centred on the letter, at the mark's own height
BELOW = "below"      # centred under the letter
TAIL = "tail"        # hung off the letter's bottom right


CEDILLA_JOIN = 3  # how far the base font's Ç sinks its C into the cedilla
OGONEK_JOIN = 15  # a tail reads as part of the letter, so it bites deeper


def place(base_contours, mark_contours, anchor: str):
    bx0, by0, bx1, by1 = bounds(base_contours)
    mx0, my0, mx1, my1 = bounds(mark_contours)
    if anchor == ABOVE:
        # Height is already right: this font puts every uppercase mark at the
        # same y and every lowercase one at another, whatever letter is under
        # it, so an above-mark only ever moves sideways.
        return translated(mark_contours, round(((bx0 + bx1) - (mx0 + mx1)) / 2), 0)
    if anchor == BELOW:
        return translated(mark_contours, round(((bx0 + bx1) - (mx0 + mx1)) / 2),
                          (by0 + CEDILLA_JOIN) - my1)
    # The ogonek hangs under the letter's right edge: in every reference face
    # its right side lines up with the letter's, which places it under the flat
    # bar of an E and under the right leg of an A with one rule.
    return translated(mark_contours, bx1 - mx1, (by0 + OGONEK_JOIN) - my1)


# (codepoint, glyph name, base codepoint, mark, anchor)
TARGETS = (
    (0x011E, "Gbreve", 0x0047, "breve_upper", ABOVE),
    (0x011F, "gbreve", 0x0067, "breve_lower", ABOVE),
    (0x015E, "Scedilla", 0x0053, "cedilla", BELOW),
    (0x015F, "scedilla", 0x0073, "cedilla", BELOW),
    (0x0130, "Idotaccent", 0x0049, "dot_upper", ABOVE),
    (0x0104, "Aogonek", 0x0041, "ogonek", TAIL),
    (0x0105, "aogonek", 0x0061, "ogonek", TAIL),
    (0x0106, "Cacute", 0x0043, "acute_upper", ABOVE),
    (0x0107, "cacute", 0x0063, "acute_lower", ABOVE),
    (0x0118, "Eogonek", 0x0045, "ogonek", TAIL),
    (0x0119, "eogonek", 0x0065, "ogonek", TAIL),
    (0x015A, "Sacute", 0x0053, "acute_upper", ABOVE),
    (0x015B, "sacute", 0x0073, "acute_lower", ABOVE),
    (0x0179, "Zacute", 0x005A, "acute_upper", ABOVE),
    (0x017A, "zacute", 0x007A, "acute_lower", ABOVE),
    (0x017B, "Zdotaccent", 0x005A, "dot_upper", ABOVE),
    (0x017C, "zdotaccent", 0x007A, "dot_lower", ABOVE),
)

# Letters the base font DOES draw, but not in its own hand: they came in with
# the Cyrillic/CJK merge from a lighter, narrower typeface, and next to the
# letters around them they read as a second font mid-word. The Polish pair and
# the Russian Ё pair are rebuilt here -- the same is true of Ě Ň Ā Ē Ī Ō Ū and
# their lowercase forms, which no shipping language needs yet.
# Ё is the loudest of them: the copy never writes it, but a portal hands back
# player names that do, and the thin donor letter lands mid-word in a heavy
# display face.
# The glyph keeps its id: the base font's kern and morx tables address it.
REPLACEMENTS = (
    (0x0143, 0x004E, "acute_upper", ABOVE),
    (0x0144, 0x006E, "acute_lower", ABOVE),
    (0x0401, 0x0415, "dieresis_upper", ABOVE),
    (0x0451, 0x0435, "dieresis_lower", ABOVE),
)


# --- Vietnamese ---------------------------------------------------------------

MARK_GAP = 24         # between a tone and the mark it stacks over
STACKED_TONE = 0.8    # a tone over another mark, so the pair stays inside the line
SIDE_TONE_SHIFT = 0.5  # an acute, grave or hook beside a circumflex, in circumflex widths
HORN_SCALE = 0.62     # the comma, down to the height of a horn
HORN_BITE = 34        # how far a horn sinks into the letter's shoulder
DOT_DROP = 40         # the dot below the baseline
BAR_W, BAR_H = 280, 96  # the bar of đ and Đ, the font's hyphen stretched

# Rebuilt by decomposition: U+1EA0..U+1EF9 and the bases they stack on that the
# donor typeface drew. Precomposed Latin-1 (à á ã â ê ô ...) is already this font's.
VIETNAMESE = (*range(0x1EA0, 0x1EFA), 0x0102, 0x0103, 0x0128, 0x0129, 0x0168, 0x0169, 0x01A0, 0x01A1, 0x01AF, 0x01B0, 0x0110, 0x0111)


def vietnamese_marks(font: TTFont) -> dict:
    question = contours_of(font, glyph_for(font, 0x003F))
    hook = max(question, key=lambda contour: bounds([contour])[3] - bounds([contour])[1])
    comma = contours_of(font, glyph_for(font, 0x002C))
    marks = {
        "grave_upper": mark_from(font, 0x00D3 - 1, 0x004F), "grave_lower": mark_from(font, 0x00F2, 0x006F),  # Ò ò
        "tilde_upper": mark_from(font, 0x00D5, 0x004F), "tilde_lower": mark_from(font, 0x00F5, 0x006F),  # Õ õ
        "circumflex_upper": mark_from(font, 0x00D4, 0x004F), "circumflex_lower": mark_from(font, 0x00F4, 0x006F),  # Ô ô
        "dot": dot_from(font, 0x0069, CAP_TOP - 200),
        "horn": scaled(comma, HORN_SCALE),
    }
    for case in ("upper", "lower"):
        # The hook takes the acute's place: same floor, same height.
        acute = bounds(mark_from(font, 0x00D3 if case == "upper" else 0x00F3, 0x004F if case == "upper" else 0x006F))
        small = scaled([hook], (acute[3] - acute[1]) / (bounds([hook])[3] - bounds([hook])[1]))
        marks[f"hook_{case}"] = translated(small, 0, acute[1] - bounds(small)[1])
    return marks


def compose_vietnamese(font: TTFont, codepoint: int, marks: dict) -> tuple[str, list]:
    if codepoint in (0x0110, 0x0111):
        base_name = glyph_for(font, 0x0044 if codepoint == 0x0110 else 0x0064)
        base = contours_of(font, base_name)
        bx0, by0, bx1, by1 = bounds(base)
        # The bar crosses the stem: left and mid-height on D, right and high on d.
        centre_x, centre_y = (bx0 + 130, (by0 + by1) // 2) if codepoint == 0x0110 else (bx1 - 110, by1 - 170)
        bar = contours_of(font, glyph_for(font, 0x002D))
        hx0, hy0, hx1, hy1 = bounds(bar)
        stretched = [[(round((x - hx0) * BAR_W / (hx1 - hx0)) + centre_x - BAR_W // 2,
                       round((y - hy0) * BAR_H / (hy1 - hy0)) + centre_y - BAR_H // 2, on) for x, y, on in contour] for contour in bar]
        return base_name, base + stretched
    letters = unicodedata.normalize("NFD", chr(codepoint))
    base_char = letters[0]
    case = "upper" if base_char.isupper() else "lower"
    if base_char == "i":
        base_char = "\u0131"  # a mark above an i takes the dot's place, except the dot below
        if "\u0323" in letters:
            base_char = "i"
    base_name = glyph_for(font, ord(base_char))
    base = contours_of(font, base_name)
    contours = list(base)
    top = None
    if "\u0302" in letters or "\u0306" in letters:
        key = "circumflex" if "\u0302" in letters else "breve"
        top = place(base, marks[f"{key}_{case}"], ABOVE)
        contours += top
    if "\u031b" in letters:
        horn = marks["horn"]
        bx0, by0, bx1, by1 = bounds(base)
        hx0, hy0, hx1, hy1 = bounds(horn)
        contours += translated(horn, bx1 - HORN_BITE - hx0, by1 - HORN_BITE - hy0 + HORN_BITE // 2)
    if "\u0323" in letters:
        bx0, by0, bx1, by1 = bounds(base)
        dx0, dy0, dx1, dy1 = bounds(marks["dot"])
        contours += translated(marks["dot"], round(((bx0 + bx1) - (dx0 + dx1)) / 2), by0 - DOT_DROP - dy1)
    tone = next((name for mark, name in (("\u0300", "grave"), ("\u0301", "acute"), ("\u0303", "tilde"), ("\u0309", "hook"))
                 if mark in letters), None)
    if tone is not None:
        mark = marks[f"{tone}_{case}"]
        if top is None:
            contours += place(base, mark, ABOVE)
        else:
            small = scaled(mark, STACKED_TONE)
            tx0, ty0, tx1, ty1 = bounds(small)
            mx0, my0, mx1, my1 = bounds(top)
            beside = "\u0302" in letters and tone != "tilde"
            if beside:
                # Beside the circumflex's slope, as Vietnamese type sets it: a
                # stack this tall would leave the line, and the tone still reads.
                side = -1 if tone == "grave" else 1
                dx = round(((mx0 + mx1) - (tx0 + tx1)) / 2 + side * (mx1 - mx0) * SIDE_TONE_SHIFT)
                contours += translated(small, dx, my0 + (my1 - my0) // 2 - ty0)
            else:
                contours += translated(small, round(((mx0 + mx1) - (tx0 + tx1)) / 2), my1 + MARK_GAP - ty0)
    return base_name, contours


def merge(base_path: Path, out_path: Path, family: str = DEFAULT_FAMILY_NAME,
          postscript: str = DEFAULT_POSTSCRIPT_NAME) -> list[str]:
    font = TTFont(base_path, recalcTimestamp=False)
    cmap = font.getBestCmap()
    marks = build_marks(font)
    glyf = font["glyf"]
    hmtx = font["hmtx"]
    order = list(font.getGlyphOrder())
    report = []

    for codepoint, name, base_cp, mark_key, anchor in TARGETS:
        if codepoint in cmap:
            raise MergeError(f"U+{codepoint:04X} is already in the base font; drop it from TARGETS")
        if name in order:
            raise MergeError(f"glyph name {name} already exists in the base font")
        base_name = glyph_for(font, base_cp)
        base_contours = contours_of(font, base_name)
        mark = place(base_contours, marks[mark_key], anchor)
        glyph = make_glyph(font, base_contours + mark)
        # New glyphs go at the END of the glyph order: the base font's kern and
        # morx tables address glyphs by id, and inserting anywhere else would
        # silently repoint every pair in them.
        order.append(name)
        glyf.glyphs[name] = glyph
        # Marks never widen a letter (the base font's own Á is exactly as wide
        # as its A), so the accented form keeps the base advance and inherits
        # its kerning behaviour by construction.
        hmtx.metrics[name] = (hmtx.metrics[base_name][0], glyph.xMin)
        report.append(f"U+{codepoint:04X} {name}: {base_name} + {mark_key} "
                      f"adv={hmtx.metrics[name][0]} bbox=({glyph.xMin},{glyph.yMin},{glyph.xMax},{glyph.yMax})")

    for codepoint, base_cp, mark_key, anchor in REPLACEMENTS:
        if codepoint not in cmap:
            raise MergeError(f"U+{codepoint:04X} is not in the base font; move it to TARGETS")
        name = cmap[codepoint]
        base_name = glyph_for(font, base_cp)
        base_contours = contours_of(font, base_name)
        mark = place(base_contours, marks[mark_key], anchor)
        glyph = make_glyph(font, base_contours + mark)
        glyf.glyphs[name] = glyph
        hmtx.metrics[name] = (hmtx.metrics[base_name][0], glyph.xMin)
        report.append(f"U+{codepoint:04X} {name}: redrawn as {base_name} + {mark_key} "
                      f"adv={hmtx.metrics[name][0]} bbox=({glyph.xMin},{glyph.yMin},{glyph.xMax},{glyph.yMax})")

    vietnamese = {**marks, **vietnamese_marks(font)}
    for codepoint in VIETNAMESE:
        name = cmap.get(codepoint)
        if name is None:
            raise MergeError(f"U+{codepoint:04X} is not in the base font")
        base_name, contours = compose_vietnamese(font, codepoint, vietnamese)
        glyph = make_glyph(font, contours)
        glyf.glyphs[name] = glyph
        hmtx.metrics[name] = (hmtx.metrics[base_name][0], glyph.xMin)
        report.append(f"U+{codepoint:04X} {name}: redrawn from {base_name} adv={hmtx.metrics[name][0]} "
                      f"bbox=({glyph.xMin},{glyph.yMin},{glyph.xMax},{glyph.yMax})")

    font.setGlyphOrder(order)
    glyf.glyphOrder = order
    font["maxp"].numGlyphs = len(order)
    for table in font["cmap"].tables:
        if not table.isUnicode():
            continue
        for codepoint, name, *_ in TARGETS:
            table.cmap[codepoint] = name

    name_table = font["name"]
    for record in list(name_table.names):
        if record.nameID in FAMILY_NAME_IDS:
            name_table.setName(family, record.nameID, record.platformID, record.platEncID, record.langID)
        elif record.nameID == POSTSCRIPT_NAME_ID:
            name_table.setName(postscript, record.nameID, record.platformID, record.platEncID, record.langID)
        elif record.nameID == UNIQUE_ID_NAME_ID:
            name_table.setName(f"{family}: derived from Lilita One Rus",
                               record.nameID, record.platformID, record.platEncID, record.langID)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    font.save(out_path)
    return report


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base", required=True, type=Path, help="LilitaOne-RussianChineseKo.ttf")
    parser.add_argument("--out", required=True, type=Path, help="merged TTF to write")
    # The OFL forbids a Modified Version from carrying the parent's Reserved Font
    # Name, so the output is never called Lilita.
    parser.add_argument("--family", default=DEFAULT_FAMILY_NAME, help="family name for the output")
    parser.add_argument("--postscript", default=DEFAULT_POSTSCRIPT_NAME, help="PostScript name for the output")
    args = parser.parse_args(argv)

    try:
        for line in merge(args.base, args.out, args.family, args.postscript):
            print(line)
    except MergeError as error:
        print(f"merge_font_glyphs: {error}", file=sys.stderr)
        return 1
    print(f"wrote {args.out} ({args.out.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
