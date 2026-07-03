"""Canvas 9-slice math: the exact Python twin of the sibling ``slice9.mjs`` — keep
the clamp formula identical; parity is the contract. Imported by ``render_group.py``
(its ``paint_element`` slice9 branch, T0233 design section 4.2). Pure, no I/O.

v1 model (T0233; lead ask: «слайс9 картинкой, чтобы проверить а работает ли
слайс9»): ``element.slice9 = {left, top, right, bottom, scale?}``, insets in SOURCE
PIXELS (integers >= 0). ``scale`` (T0233 scope addition, lead: «важно чтобы я мог
скейлить края, иногда мне нужно больше или меньше») is an optional multiplier > 0
(default 1) applied to the DESTINATION corner/edge band size only — the SOURCE crop
never changes size. Absent slice9 on the element = today's plain resize everywhere
(additive, zero migration); that branch lives in render_group.py, not here.
"""
from __future__ import annotations

from typing import Any

# Engine precedent (nt_sprite_renderer.c:697): `sl + sr < source_w && st + sb <
# source_h` — the same set-time loud invariant ops.mjs's validateSlice9 enforces.
MAX_SLICE9_SCALE = 16


def validate(insets: dict[str, Any], source_w: float, source_h: float) -> dict[str, float]:
    """Defense-in-depth mirror of ops.mjs's ``validateSlice9``. The JS op is the
    PRIMARY loud-validation gate at SET time (from either client); this only guards
    a corrupt/foreign spec reaching the renderer. Returns
    ``{left, top, right, bottom[, scale]}`` (scale present only when != 1)."""
    if not isinstance(insets, dict):
        raise ValueError(f"slice9 insets must be an object, got {insets!r}")
    src_w = float(source_w)
    src_h = float(source_h)
    if not (src_w > 0) or not (src_h > 0):
        raise ValueError(f"slice9 requires a positive source size, got {src_w}x{src_h}")

    def inset(field: str) -> float:
        raw = insets.get(field)
        try:
            num = float(raw)
        except (TypeError, ValueError):
            num = float("nan")
        if not (num == num) or num < 0 or round(num) != num:  # num == num is the NaN guard
            raise ValueError(f"slice9 {field} must be a non-negative integer, got {raw!r}")
        return num

    left = inset("left")
    top = inset("top")
    right = inset("right")
    bottom = inset("bottom")
    if left + right >= src_w:
        raise ValueError(f"slice9 left+right ({left + right}) must be < source width {src_w}")
    if top + bottom >= src_h:
        raise ValueError(f"slice9 top+bottom ({top + bottom}) must be < source height {src_h}")

    out: dict[str, float] = {"left": left, "top": top, "right": right, "bottom": bottom}
    raw_scale = insets.get("scale")
    if raw_scale is not None:
        scale = float(raw_scale)
        if not (scale > 0) or scale > MAX_SLICE9_SCALE:
            raise ValueError(f"slice9 scale must be a finite number in (0, {MAX_SLICE9_SCALE}], got {raw_scale!r}")
        if scale != 1:
            out["scale"] = scale
    return out


def slice9_patches(
    insets: dict[str, Any], src_w: float, src_h: float, dst_w: float, dst_h: float
) -> list[dict[str, float]]:
    """Mirror of ``slice9.mjs``'s ``slice9Patches`` — keep this identical; parity is
    the contract. Source bands (``sxs``/``sys``) are FIXED, derived only from
    src_w/src_h + the raw (un-scaled) insets — ``scale`` never touches source
    pixels. Destination bands (``dxs``/``dys``) multiply each inset by ``scale``
    FIRST (fatter/thinner corners/edges), then apply the SAME proportional clamp as
    scale=1 so corners never overlap a box smaller than the (scaled) corner sum.
    Zero-area patches are dropped. dst coords are element-local world units; the
    caller (render_group.py) scales them to its own device space."""
    left = float(insets.get("left") or 0)
    top = float(insets.get("top") or 0)
    right = float(insets.get("right") or 0)
    bottom = float(insets.get("bottom") or 0)
    raw_scale = insets.get("scale")
    scale = float(raw_scale) if raw_scale is not None and float(raw_scale) > 0 else 1.0

    w = float(src_w)
    h = float(src_h)
    dw0 = max(0.0, float(dst_w))
    dh0 = max(0.0, float(dst_h))

    sxs = [0.0, left, w - right, w]
    sys = [0.0, top, h - bottom, h]

    d_left, d_right = left * scale, right * scale
    if d_left + d_right > dw0 and d_left + d_right > 0:
        f = dw0 / (d_left + d_right)
        d_left *= f
        d_right *= f
    d_top, d_bottom = top * scale, bottom * scale
    if d_top + d_bottom > dh0 and d_top + d_bottom > 0:
        f = dh0 / (d_top + d_bottom)
        d_top *= f
        d_bottom *= f
    dxs = [0.0, d_left, dw0 - d_right, dw0]
    dys = [0.0, d_top, dh0 - d_bottom, dh0]

    patches: list[dict[str, float]] = []
    for c in range(3):
        for r in range(3):
            sw = sxs[c + 1] - sxs[c]
            sh = sys[r + 1] - sys[r]
            dw = dxs[c + 1] - dxs[c]
            dh = dys[r + 1] - dys[r]
            if sw <= 0 or sh <= 0 or dw <= 0 or dh <= 0:
                continue  # skip an empty band
            patches.append({"sx": sxs[c], "sy": sys[r], "sw": sw, "sh": sh, "dx": dxs[c], "dy": dys[r], "dw": dw, "dh": dh})
    return patches
