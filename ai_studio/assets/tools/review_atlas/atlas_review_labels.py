#!/usr/bin/env python3
"""Shared atlas review-label contract for build_review_atlas + audit_review_atlas.

The pack builder writes a `review_label.text` per atlas entry; the auditor
recomputes the expected text and rejects the pack on mismatch. The two MUST
agree on the format or the audit silently passes/fails the wrong thing -- so the
one format lives here, alongside the font + measurement helpers and the
label-box constants both sides shared verbatim.
"""

from __future__ import annotations

from PIL import Image, ImageDraw, ImageFont

DEFAULT_LABEL_FONT_SIZE = 18
LABEL_PAD_X = 4
LABEL_PAD_Y = 2
LABEL_LINE_GAP_Y = 2
LABEL_OUTER_MARGIN = 8
_LABEL_FONTS: dict[int, ImageFont.ImageFont] = {}


def label_font(font_size: int = DEFAULT_LABEL_FONT_SIZE) -> ImageFont.ImageFont:
    if font_size in _LABEL_FONTS:
        return _LABEL_FONTS[font_size]
    for name in ("DejaVuSans.ttf", "Arial.ttf"):
        try:
            _LABEL_FONTS[font_size] = ImageFont.truetype(name, font_size)
            return _LABEL_FONTS[font_size]
        except OSError:
            continue
    _LABEL_FONTS[font_size] = ImageFont.load_default()
    return _LABEL_FONTS[font_size]


def measure_label(label: str, font_size: int = DEFAULT_LABEL_FONT_SIZE) -> tuple[int, int]:
    probe = Image.new("RGBA", (1, 1), (0, 0, 0, 0))
    draw = ImageDraw.Draw(probe)
    bbox = draw.textbbox((0, 0), label, font=label_font(font_size))
    return int(bbox[2] - bbox[0]), int(bbox[3] - bbox[1])


def review_label_text(entry_id: str, alias_ids) -> str:
    """The atlas review-label string: ``<id>``, or ``<id> (+<sorted,aliases>)``
    when the entry has aliases. SINGLE SOURCE -- the pack builder and the auditor
    both call this so their label text cannot drift. ``alias_ids`` is any
    iterable (list or set); it is sorted here.
    """
    aliases = sorted(alias_ids)
    if not aliases:
        return str(entry_id)
    return f"{entry_id} (+{','.join(aliases)})"
