#!/usr/bin/env python3
"""Generate soft, alpha, placeholder sprites for Critter Corral.

DIRECTION: bright, high-contrast, friendly, soft-edged, tactile, "looks like a
game". These are placeholders (Codex refines later) — quality bar = reads as a
game, not a debug screen. Output PNGs are RGBA with transparent backgrounds.

READABILITY PASS:
  - pen.png is white/near-white so the runtime emit-color tint reproduces the
    *exact* critter hue, with a darker fence rim and a clear open gate so
    "red critters go in the red pen" reads instantly.
  - grass.png is softer and lower-contrast so the field recedes and the
    critters/pens/lure are the focus.
  - critters are bolder (radial gradient + dark rim + clear eyes), drawn bigger
    and with a soft drop shadow at runtime.
  - lure has a soft center (no harsh bright dot), keeping the halo.
  - flag.png is a small white pennant (tinted to the pen hue) used as a marker.

Run: py -3.12 tools/critter_corral/generate_sprites.py
"""
from __future__ import annotations

import math
import os

import numpy as np
from PIL import Image

OUT_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "gamedesign", "projects", "critter-corral", "art", "sprites",
)


def _save(rgba: np.ndarray, name: str) -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    img = Image.fromarray(np.clip(rgba, 0, 255).astype(np.uint8), "RGBA")
    path = os.path.join(OUT_DIR, name)
    img.save(path)
    print(f"  wrote {path} ({img.width}x{img.height})")


def _grid(size: int):
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)
    cx = cy = (size - 1) * 0.5
    return xs, ys, cx, cy


def _smoothstep(edge0: float, edge1: float, x: np.ndarray) -> np.ndarray:
    t = np.clip((x - edge0) / max(1e-6, (edge1 - edge0)), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def critter(size: int, body_rgb, rim_rgb, eye_dir: float):
    """Candy-soft critter: imperfect squishy silhouette, tiny nubs, glossy body,
    expressive eyes and a small smile. Kept readable at gameplay size."""
    xs, ys, cx, cy = _grid(size)
    dx = xs - cx
    dy = ys - cy
    theta = np.arctan2(dy, dx)
    # Not a perfect token: a subtle wobbly outline makes it feel alive.
    wobble = 1.0 + 0.035 * np.sin(theta * 3.0 + 0.4) + 0.025 * np.sin(theta * 5.0 - 0.7)
    r = np.sqrt((dx / 1.03) ** 2 + (dy / 0.94) ** 2) / wobble
    radius = size * 0.46
    out = np.zeros((size, size, 4), np.float32)

    # soft alpha falloff at the edge for a tactile, anti-aliased rim
    alpha = 1.0 - _smoothstep(radius - 2.0, radius + 1.5, r)

    # little side nubs/feet so the silhouette is a character, not a token
    for nx, ny, nr in (
        (cx - radius * 0.56, cy + radius * 0.24, radius * 0.18),
        (cx + radius * 0.56, cy + radius * 0.24, radius * 0.18),
        (cx - radius * 0.22, cy + radius * 0.55, radius * 0.15),
        (cx + radius * 0.22, cy + radius * 0.55, radius * 0.15),
    ):
        alpha = np.maximum(alpha, (1.0 - _smoothstep(nr - 1.2, nr + 1.2,
                                                     np.sqrt((xs - nx) ** 2 + (ys - ny) ** 2))) * 0.95)

    # body: radial gradient (brighter centre -> rim color near edge)
    t = np.clip(r / radius, 0.0, 1.0)[..., None]
    body = np.array(body_rgb, np.float32)
    rim = np.array(rim_rgb, np.float32)
    # lift centre for a glossy, rounded read
    centre = np.clip(body * 1.22, 0, 255)
    rgb = centre * (1.0 - t) + rim * t

    # dark contrast outline ring near the rim so the silhouette stays crisp
    # against bright or busy backgrounds (readability).
    outline = _smoothstep(radius - 4.0, radius - 1.0, r) * (
        1.0 - _smoothstep(radius - 1.0, radius + 1.0, r)
    )
    dark = rim * 0.45
    rgb = rgb * (1.0 - outline[..., None]) + dark[None, None, :] * outline[..., None]

    # soft top-left highlight (cartoon shine) plus a lower bounce-light patch
    hx, hy = cx - radius * 0.34, cy - radius * 0.34
    hr = radius * 0.44
    hd = np.sqrt((xs - hx) ** 2 + (ys - hy) ** 2)
    shine = (1.0 - _smoothstep(0.0, hr, hd)) * 0.6
    rgb = rgb + (255.0 - rgb) * shine[..., None]
    belly = (1.0 - _smoothstep(0.0, radius * 0.58,
                               np.sqrt((xs - cx) ** 2 + (ys - (cy + radius * 0.18)) ** 2))) * 0.18
    rgb = rgb + (255.0 - rgb) * belly[..., None]

    out[..., :3] = rgb
    out[..., 3] = alpha * 255.0

    # eyes: white sclera + dark pupil, biased toward travel direction. Bigger +
    # a thin dark ring so they read clearly even when small on screen.
    ex = cx + eye_dir * radius * 0.14
    ey = cy - radius * 0.14
    gap = radius * 0.32
    for sign in (-1.0, 1.0):
        scx = ex + sign * gap
        sd = np.sqrt((xs - scx) ** 2 + (ys - ey) ** 2)
        sr = radius * 0.24
        # dark eye ring (contrast)
        ring_a = _smoothstep(sr - 0.5, sr + 1.0, sd) * (
            1.0 - _smoothstep(sr + 1.5, sr + 3.0, sd)
        )
        for c in range(3):
            out[..., c] = out[..., c] * (1.0 - ring_a) + 40.0 * ring_a
        # white sclera
        eye_a = (1.0 - _smoothstep(sr - 1.5, sr + 0.5, sd))
        for c in range(3):
            out[..., c] = out[..., c] * (1.0 - eye_a) + 250.0 * eye_a
        out[..., 3] = np.maximum(out[..., 3], np.maximum(eye_a, ring_a) * 255.0)
        # pupil
        pcx = scx + eye_dir * sr * 0.30
        pd = np.sqrt((xs - pcx) ** 2 + (ys - ey) ** 2)
        pr = sr * 0.56
        pup_a = (1.0 - _smoothstep(pr - 1.0, pr + 0.8, pd))
        pup = np.array([24.0, 24.0, 34.0], np.float32)
        for c in range(3):
            out[..., c] = out[..., c] * (1.0 - pup_a) + pup[c] * pup_a
        # tiny catchlight
        ld = np.sqrt((xs - (pcx - pr * 0.4)) ** 2 + (ys - (ey - pr * 0.4)) ** 2)
        lit = (1.0 - _smoothstep(0.0, pr * 0.5, ld)) * 0.9
        for c in range(3):
            out[..., c] = out[..., c] * (1.0 - lit) + 255.0 * lit
    # tiny smile: dark soft arc below the eyes
    mx = xs - cx
    mcy = cy + radius * 0.02
    mouth_r = radius * 0.30
    mouth_d = np.sqrt(mx ** 2 + (ys - mcy) ** 2)
    mouth_band = (np.abs(mouth_d - mouth_r) < 1.8) & (ys > mcy) & (np.abs(mx) < radius * 0.26)
    smile = mouth_band.astype(np.float32) * alpha
    for c in range(3):
        out[..., c] = out[..., c] * (1.0 - smile) + 35.0 * smile
    out[..., 3] = np.maximum(out[..., 3], smile * 255.0)
    return out


def critter_neutral(size: int):
    """Neutral tintable critter with the same squishy character silhouette.
    Body is near-white so runtime emit-color reproduces saturated hues cleanly."""
    xs, ys, cx, cy = _grid(size)
    dx = xs - cx
    dy = ys - cy
    theta = np.arctan2(dy, dx)
    wobble = 1.0 + 0.035 * np.sin(theta * 3.0 + 0.4) + 0.025 * np.sin(theta * 5.0 - 0.7)
    r = np.sqrt((dx / 1.03) ** 2 + (dy / 0.94) ** 2) / wobble
    radius = size * 0.46
    out = np.zeros((size, size, 4), np.float32)

    alpha = 1.0 - _smoothstep(radius - 2.0, radius + 1.5, r)
    for nx, ny, nr in (
        (cx - radius * 0.56, cy + radius * 0.24, radius * 0.18),
        (cx + radius * 0.56, cy + radius * 0.24, radius * 0.18),
        (cx - radius * 0.22, cy + radius * 0.55, radius * 0.15),
        (cx + radius * 0.22, cy + radius * 0.55, radius * 0.15),
    ):
        alpha = np.maximum(alpha, (1.0 - _smoothstep(nr - 1.2, nr + 1.2,
                                                     np.sqrt((xs - nx) ** 2 + (ys - ny) ** 2))) * 0.95)

    # body: near-white radial gradient (slightly darker toward the rim) so the
    # emit tint multiplies to a saturated, even hue with a soft rounded shade.
    t = np.clip(r / radius, 0.0, 1.0)[..., None]
    centre = np.full((size, size, 3), 255.0, np.float32)
    edge = np.full((size, size, 3), 190.0, np.float32)
    rgb = centre * (1.0 - t) + edge * t

    # dark contrast outline ring near the rim (stays dark under any tint).
    outline = _smoothstep(radius - 4.0, radius - 1.0, r) * (
        1.0 - _smoothstep(radius - 1.0, radius + 1.0, r)
    )
    dark = np.array([70.0, 70.0, 78.0], np.float32)
    rgb = rgb * (1.0 - outline[..., None]) + dark[None, None, :] * outline[..., None]

    # soft top-left highlight (cartoon shine) — pushes toward white so it stays
    # bright under tint.
    hx, hy = cx - radius * 0.34, cy - radius * 0.34
    hr = radius * 0.44
    hd = np.sqrt((xs - hx) ** 2 + (ys - hy) ** 2)
    shine = (1.0 - _smoothstep(0.0, hr, hd)) * 0.5
    rgb = rgb + (255.0 - rgb) * shine[..., None]
    belly = (1.0 - _smoothstep(0.0, radius * 0.58,
                               np.sqrt((xs - cx) ** 2 + (ys - (cy + radius * 0.18)) ** 2))) * 0.16
    rgb = rgb + (255.0 - rgb) * belly[..., None]

    out[..., :3] = rgb
    out[..., 3] = alpha * 255.0

    # eyes: white sclera + dark pupil, centred (neutral, no travel bias) so the
    # one shape reads the same for every color.
    ex = cx
    ey = cy - radius * 0.14
    gap = radius * 0.32
    for sign in (-1.0, 1.0):
        scx = ex + sign * gap
        sd = np.sqrt((xs - scx) ** 2 + (ys - ey) ** 2)
        sr = radius * 0.24
        ring_a = _smoothstep(sr - 0.5, sr + 1.0, sd) * (
            1.0 - _smoothstep(sr + 1.5, sr + 3.0, sd)
        )
        for c in range(3):
            out[..., c] = out[..., c] * (1.0 - ring_a) + 40.0 * ring_a
        eye_a = (1.0 - _smoothstep(sr - 1.5, sr + 0.5, sd))
        for c in range(3):
            out[..., c] = out[..., c] * (1.0 - eye_a) + 250.0 * eye_a
        out[..., 3] = np.maximum(out[..., 3], np.maximum(eye_a, ring_a) * 255.0)
        pcx = scx
        pd = np.sqrt((xs - pcx) ** 2 + (ys - ey) ** 2)
        pr = sr * 0.56
        pup_a = (1.0 - _smoothstep(pr - 1.0, pr + 0.8, pd))
        pup = np.array([24.0, 24.0, 34.0], np.float32)
        for c in range(3):
            out[..., c] = out[..., c] * (1.0 - pup_a) + pup[c] * pup_a
        ld = np.sqrt((xs - (pcx - pr * 0.4)) ** 2 + (ys - (ey - pr * 0.4)) ** 2)
        lit = (1.0 - _smoothstep(0.0, pr * 0.5, ld)) * 0.9
        for c in range(3):
            out[..., c] = out[..., c] * (1.0 - lit) + 255.0 * lit
    mx = xs - cx
    mcy = cy + radius * 0.02
    mouth_r = radius * 0.30
    mouth_d = np.sqrt(mx ** 2 + (ys - mcy) ** 2)
    mouth_band = (np.abs(mouth_d - mouth_r) < 1.8) & (ys > mcy) & (np.abs(mx) < radius * 0.26)
    smile = mouth_band.astype(np.float32) * alpha
    for c in range(3):
        out[..., c] = out[..., c] * (1.0 - smile) + 36.0 * smile
    out[..., 3] = np.maximum(out[..., 3], smile * 255.0)
    return out


def _round_rect_mask(xs, ys, x0, y0, x1, y1, radius, soft=1.5):
    """Signed-distance rounded-rect -> soft [0,1] coverage mask."""
    rx = np.maximum(np.maximum(x0 + radius - xs, xs - (x1 - radius)), 0.0)
    ry = np.maximum(np.maximum(y0 + radius - ys, ys - (y1 - radius)), 0.0)
    # distance outside the rounded rect
    d = np.sqrt(rx ** 2 + ry ** 2) - radius
    return 1.0 - _smoothstep(0.0, soft, d)


def pen(w: int, h: int):
    """Tintable toy-fence pen: near-white planks multiply into the target hue,
    with baked darker rails/posts and an open right-face gate."""
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    out = np.zeros((h, w, 4), np.float32)
    pad = 7.0
    radius = 22.0
    panel = _round_rect_mask(xs, ys, pad, pad, w - pad, h - pad, radius, soft=2.0)

    # near-white toy planks with subtle woodgrain. White means runtime tint
    # multiplies cleanly to the exact pen hue.
    plank = (np.floor((xs - pad) / max(1.0, (w - 2 * pad) / 5.0)) % 2.0)
    sheen = 0.90 + 0.10 * (1.0 - ys / h)
    grain = np.sin(ys * 0.16 + plank * 1.7) * 3.0 + np.sin((xs + ys) * 0.045) * 2.0
    base = 252.0 + grain + plank * 5.0
    fill_rgb = np.stack([base * sheen, base * sheen, base * sheen], axis=-1)
    out[..., :3] = fill_rgb
    out[..., 3] = panel * 255.0

    # bold darker fence rim so the panel reads as a framed pen (tactile object).
    inner = _round_rect_mask(xs, ys, pad + 16, pad + 16,
                             w - pad - 16, h - pad - 16, radius * 0.7, soft=2.0)
    rim = np.clip(panel - inner, 0.0, 1.0)
    for c in range(3):
        out[..., c] = out[..., c] * (1.0 - rim * 0.68)

    # chunky horizontal rails: clear "corral" read even when tinted.
    rail_mask = np.zeros((h, w), np.float32)
    for py in (pad + h * 0.25, h - pad - h * 0.25):
        rail_mask = np.maximum(rail_mask, (1.0 - _smoothstep(8.0, 11.5, np.abs(ys - py))) * panel)
    for c in range(3):
        out[..., c] = out[..., c] * (1.0 - rail_mask * 0.38)

    # vertical fence posts inside the rim (gives the "pen" read, stays tintable)
    n_posts = 5
    post_mask = np.zeros((h, w), np.float32)
    for k in range(n_posts):
        px = pad + 18 + (w - 2 * (pad + 18)) * (k / (n_posts - 1))
        post_mask = np.maximum(post_mask, np.exp(-((xs - px) ** 2) / (2.0 * 3.0 ** 2)))
    post_mask *= inner  # only inside the fenced area
    for c in range(3):
        out[..., c] = out[..., c] * (1.0 - post_mask * 0.28)

    # small nail heads on rails/posts, still tintable but darker.
    nail = np.zeros((h, w), np.float32)
    for k in range(n_posts):
        px = pad + 18 + (w - 2 * (pad + 18)) * (k / (n_posts - 1))
        for py in (pad + h * 0.25, h - pad - h * 0.25):
            nail = np.maximum(nail, _disc(xs, ys, px, py, 4.2, soft=1.0))
    for c in range(3):
        out[..., c] = out[..., c] * (1.0 - nail * 0.45)

    # open GATE: carve a tall notch out of the RIGHT face (centre vertically) so
    # the entrance reads as an opening. Runtime mirrors which side faces field.
    gate_h = h * 0.5
    gy0 = (h - gate_h) * 0.5
    gy1 = gy0 + gate_h
    in_band = ((ys > gy0) & (ys < gy1)).astype(np.float32)
    band_soft = _smoothstep(0.0, 8.0, np.minimum(ys - gy0, gy1 - ys))
    # carve from the right edge inward
    carve_x = 1.0 - _smoothstep(w - pad - 30.0, w - pad - 8.0, xs)
    carve_x = 1.0 - carve_x  # 1 near the right edge
    carve = np.clip(in_band * band_soft * carve_x, 0.0, 1.0)
    out[..., 3] = out[..., 3] * (1.0 - carve)

    # bright rounded gate posts flanking the right-face opening
    for py in (gy0, gy1):
        pd = np.sqrt((xs - (w - pad - 10.0)) ** 2 + (ys - py) ** 2)
        pr = 12.0
        post_a = (1.0 - _smoothstep(pr - 2.0, pr + 1.0, pd))
        for c in range(3):
            out[..., c] = out[..., c] * (1.0 - post_a) + 255.0 * post_a
        out[..., 3] = np.maximum(out[..., 3], post_a * 255.0)
    return out


def flag(w: int, h: int):
    """Small tintable pennant on a chunky pole. Marks each pen color."""
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    out = np.zeros((h, w, 4), np.float32)
    # pole on the left
    pole_x = w * 0.16
    pole = (1.0 - _smoothstep(2.0, 4.0, np.abs(xs - pole_x))) * (
        _smoothstep(2.0, 6.0, ys) * (1.0 - _smoothstep(h - 6.0, h - 2.0, ys))
    )
    # triangular pennant pointing right from the top of the pole
    tip_x = w * 0.92
    top = h * 0.10
    bot = h * 0.52
    # flag occupies x in [pole_x, tip_x]; height shrinks toward the tip
    fx = np.clip((xs - pole_x) / max(1.0, (tip_x - pole_x)), 0.0, 1.0)
    half = (bot - top) * 0.5 * (1.0 - fx)
    mid = (top + bot) * 0.5
    in_flag = ((xs >= pole_x) & (xs <= tip_x) &
               (np.abs(ys - mid) <= half)).astype(np.float32)
    flag_soft = (1.0 - _smoothstep(0.0, 2.0, np.abs(ys - mid) - (half - 1.5)))
    flag_a = np.clip(in_flag * flag_soft, 0.0, 1.0)
    # white flag + slightly darker pole so it reads as a marker
    rgb = np.full((h, w, 3), 255.0, np.float32)
    bead = _disc(xs, ys, pole_x, h * 0.83, w * 0.10, soft=1.0)
    a = np.clip(np.maximum(np.maximum(flag_a, pole), bead), 0.0, 1.0)
    # darken the pole and bottom bead a touch
    out[..., :3] = rgb * (1.0 - (pole + bead)[..., None] * 0.35)
    out[..., 3] = a * 255.0
    return out


def grass(size: int):
    """Bright mobile pasture tile: juicy but low-contrast enough to recede."""
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)
    base = np.array([125.0, 191.0, 106.0], np.float32)
    out = np.zeros((size, size, 4), np.float32)
    field = np.tile(base, (size, size, 1))
    # tileable soft mottling + broad mown arcs. Keep contrast restrained.
    tex = (
        np.sin(xs / size * math.tau * 1.0 + 0.4) * np.cos(ys / size * math.tau * 1.0 - 0.7)
        + 0.45 * np.sin((xs + ys) / size * math.tau * 2.0)
        + 0.25 * np.cos((xs - ys) / size * math.tau * 3.0)
    )
    field += tex[..., None] * np.array([4.0, 6.0, 3.0], np.float32)
    stripe = 0.5 + 0.5 * np.sin((xs + ys * 0.42) / size * math.tau * 4.0)
    field += (stripe[..., None] - 0.5) * np.array([7.0, 9.0, 4.0], np.float32)

    # sparse flowers/clover sell the pasture without becoming gameplay noise.
    for fx, fy, rgb, rad in (
        (0.18, 0.28, (255.0, 232.0, 128.0), 3.2),
        (0.72, 0.18, (244.0, 178.0, 215.0), 2.8),
        (0.42, 0.76, (236.0, 250.0, 170.0), 2.4),
        (0.88, 0.62, (255.0, 232.0, 128.0), 2.6),
    ):
        d = _disc(xs, ys, fx * size, fy * size, rad, soft=1.2)
        field = field * (1.0 - d[..., None] * 0.34) + np.array(rgb, np.float32) * d[..., None] * 0.34
    out[..., :3] = field
    out[..., 3] = 255.0
    return out


def lure(size: int):
    """Soft glowing ring/orb. Keeps the halo but the CENTER is soft (no harsh
    bright dot) — gentle warm gold falloff."""
    xs, ys, cx, cy = _grid(size)
    r = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2)
    radius = size * 0.46
    out = np.zeros((size, size, 4), np.float32)
    # glowing ring near the rim (the main affordance)
    ring_r = radius * 0.74
    ring = np.exp(-((r - ring_r) ** 2) / (2.0 * (radius * 0.14) ** 2))
    # very soft, low-intensity core (no hot dot)
    core = np.exp(-(r ** 2) / (2.0 * (radius * 0.40) ** 2)) * 0.35
    star = np.maximum(
        1.0 - _smoothstep(size * 0.015, size * 0.045, np.abs(xs - cx)),
        1.0 - _smoothstep(size * 0.015, size * 0.045, np.abs(ys - cy)),
    ) * (1.0 - _smoothstep(0.0, radius * 0.55, r))
    glow = np.clip(ring * 0.85 + core + star * 0.45, 0.0, 1.0)
    gold = np.array([255.0, 224.0, 130.0], np.float32)
    warm = np.array([255.0, 238.0, 190.0], np.float32)
    cmix = np.clip(core, 0.0, 1.0)[..., None]
    rgb = gold[None, None, :] * (1.0 - cmix) + warm[None, None, :] * cmix
    out[..., :3] = rgb
    out[..., 3] = np.clip(glow * 255.0 * (1.0 - _smoothstep(radius - 2.0, radius + 2.0, r)), 0, 255)
    return out


def spark(size: int):
    """Small soft particle dot (radial glow)."""
    xs, ys, cx, cy = _grid(size)
    r = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2)
    radius = size * 0.46
    out = np.zeros((size, size, 4), np.float32)
    glow = np.exp(-(r ** 2) / (2.0 * (radius * 0.42) ** 2))
    cross = np.maximum(
        1.0 - _smoothstep(size * 0.012, size * 0.045, np.abs(xs - cx)),
        1.0 - _smoothstep(size * 0.012, size * 0.045, np.abs(ys - cy)),
    ) * (1.0 - _smoothstep(0.0, radius, r))
    glow = np.clip(glow * 0.75 + cross * 0.55, 0.0, 1.0)
    out[..., :3] = 255.0  # white; tinted at emit time by vertex color
    out[..., 3] = np.clip(glow * 255.0, 0, 255)
    return out


def pip(size: int):
    """Solid soft rounded square pip (white, tinted at emit). Crisp UI marker
    for score / per-color remaining bars — reads better than a fuzzy dot."""
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)
    out = np.zeros((size, size, 4), np.float32)
    pad = size * 0.12
    radius = size * 0.30
    mask = _round_rect_mask(xs, ys, pad, pad, size - pad, size - pad, radius, soft=1.5)
    shade = 0.84 + 0.16 * (1.0 - ys / size)
    highlight = _round_rect_mask(xs, ys, pad + 3, pad + 3, size - pad - 3,
                                 size * 0.48, radius * 0.75, soft=1.5) * 0.18
    out[..., :3] = np.clip((245.0 * shade + 255.0 * highlight)[..., None], 0, 255)
    out[..., 3] = mask * 255.0
    return out


def _disc(xs, ys, cx, cy, r, soft=1.5):
    """Soft filled disc coverage mask."""
    d = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2)
    return 1.0 - _smoothstep(r - soft, r + soft, d)


def _ring(xs, ys, cx, cy, r, thick, soft=1.5):
    """Soft hollow ring (annulus) coverage mask."""
    d = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2)
    inner = _smoothstep(r - thick - soft, r - thick + soft, d)
    outer = 1.0 - _smoothstep(r + thick - soft, r + thick + soft, d)
    return np.clip(inner * outer, 0.0, 1.0)


def _seg(xs, ys, ax, ay, bx, by, half_w, soft=1.5):
    """Soft thick line segment coverage mask (capsule)."""
    dx = bx - ax
    dy = by - ay
    ll = max(1e-6, dx * dx + dy * dy)
    t = np.clip(((xs - ax) * dx + (ys - ay) * dy) / ll, 0.0, 1.0)
    px = ax + t * dx
    py = ay + t * dy
    d = np.sqrt((xs - px) ** 2 + (ys - py) ** 2)
    return 1.0 - _smoothstep(half_w - soft, half_w + soft, d)


def _icon_base(size: int):
    """White RGBA canvas + grid; icons are white so the runtime emit-color tints
    them (we draw them bright so a card backdrop reads behind)."""
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)
    out = np.zeros((size, size, 4), np.float32)
    out[..., :3] = 255.0
    return out, xs, ys


def _stamp(out, mask):
    out[..., 3] = np.maximum(out[..., 3], np.clip(mask, 0.0, 1.0) * 255.0)


def icon_radius(size: int):
    """A RING = lure radius +. Big soft ring with a small centre dot (the lure)."""
    out, xs, ys = _icon_base(size)
    c = (size - 1) * 0.5
    _stamp(out, _ring(xs, ys, c, c, size * 0.34, size * 0.055))
    _stamp(out, _disc(xs, ys, c, c, size * 0.07))
    return out


def icon_pull(size: int):
    """An ARROW = pull strength +. A bold arrow pointing toward a centre dot."""
    out, xs, ys = _icon_base(size)
    c = (size - 1) * 0.5
    # shaft from upper-left toward centre
    ax, ay = size * 0.22, size * 0.22
    bx, by = size * 0.60, size * 0.60
    _stamp(out, _seg(xs, ys, ax, ay, bx, by, size * 0.05))
    # arrowhead barbs at the tip (pointing to bottom-right toward target)
    _stamp(out, _seg(xs, ys, bx, by, bx - size * 0.18, by, size * 0.05))
    _stamp(out, _seg(xs, ys, bx, by, bx, by - size * 0.18, size * 0.05))
    # the target dot (lure point) it pulls toward
    _stamp(out, _disc(xs, ys, size * 0.74, size * 0.74, size * 0.07))
    return out


def icon_second_lure(size: int):
    """TWO DOTS = a second (trailing) lure. Two rings linked by a faint trail."""
    out, xs, ys = _icon_base(size)
    cy = (size - 1) * 0.5
    x0, x1 = size * 0.34, size * 0.66
    # link trail between them
    _stamp(out, _seg(xs, ys, x0, cy, x1, cy, size * 0.025) * 0.7)
    # leading ring (bigger) + trailing ring (smaller)
    _stamp(out, _ring(xs, ys, x0, cy, size * 0.18, size * 0.05))
    _stamp(out, _disc(xs, ys, x0, cy, size * 0.05))
    _stamp(out, _ring(xs, ys, x1, cy, size * 0.13, size * 0.045))
    _stamp(out, _disc(xs, ys, x1, cy, size * 0.04))
    return out


def icon_gate(size: int):
    """A WIDE GATE = wider pen gates. Two posts with arrows spreading them open."""
    out, xs, ys = _icon_base(size)
    cy = (size - 1) * 0.5
    top, bot = size * 0.24, size * 0.76
    lx, rx = size * 0.30, size * 0.70
    # two gate posts
    _stamp(out, _seg(xs, ys, lx, top, lx, bot, size * 0.05))
    _stamp(out, _seg(xs, ys, rx, top, rx, bot, size * 0.05))
    # outward arrows (the "wider" cue): left arrow pointing left, right pointing right
    _stamp(out, _seg(xs, ys, lx, cy, lx - size * 0.16, cy, size * 0.04))
    _stamp(out, _seg(xs, ys, lx - size * 0.16, cy, lx - size * 0.08, cy - size * 0.08, size * 0.04))
    _stamp(out, _seg(xs, ys, lx - size * 0.16, cy, lx - size * 0.08, cy + size * 0.08, size * 0.04))
    _stamp(out, _seg(xs, ys, rx, cy, rx + size * 0.16, cy, size * 0.04))
    _stamp(out, _seg(xs, ys, rx + size * 0.16, cy, rx + size * 0.08, cy - size * 0.08, size * 0.04))
    _stamp(out, _seg(xs, ys, rx + size * 0.16, cy, rx + size * 0.08, cy + size * 0.08, size * 0.04))
    return out


def icon_calm(size: int):
    """A 'Zzz' = calmer critters. Three descending Z glyphs (sleepy / relaxed)."""
    out, xs, ys = _icon_base(size)

    def draw_z( cx, cy, s, w):
        # top bar, diagonal, bottom bar
        _stamp(out, _seg(xs, ys, cx - s, cy - s, cx + s, cy - s, w))
        _stamp(out, _seg(xs, ys, cx + s, cy - s, cx - s, cy + s, w))
        _stamp(out, _seg(xs, ys, cx - s, cy + s, cx + s, cy + s, w))

    draw_z(size * 0.34, size * 0.66, size * 0.10, size * 0.035)
    draw_z(size * 0.56, size * 0.46, size * 0.075, size * 0.030)
    draw_z(size * 0.72, size * 0.30, size * 0.055, size * 0.024)
    return out


def icon_chain(size: int):
    """A CHAIN = longer chain. Three interlocked ring links across the icon."""
    out, xs, ys = _icon_base(size)
    cy = (size - 1) * 0.5
    for i, x in enumerate((size * 0.30, size * 0.50, size * 0.70)):
        yo = cy + (size * 0.06 if i % 2 else -size * 0.06)
        _stamp(out, _ring(xs, ys, x, yo, size * 0.135, size * 0.05))
    return out


def card(w: int, h: int):
    """Candy UI card backdrop for upgrade choices. Text/icons stay runtime."""
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    out = np.zeros((h, w, 4), np.float32)
    pad = 6.0
    radius = 26.0
    panel = _round_rect_mask(xs, ys, pad, pad, w - pad, h - pad, radius, soft=2.0)
    # warm paper/plastic fill with glossy top and subtle diagonal texture.
    sheen = 0.88 + 0.12 * (1.0 - ys / h)
    diagonal = np.sin((xs + ys * 0.55) * 0.055) * 2.0
    base = 252.0 + diagonal
    out[..., :3] = np.stack([base * sheen, base * sheen, base * sheen], axis=-1)
    out[..., 3] = panel * 255.0

    # darker outer border plus a bright inner bevel so the card reads as a
    # tappable object under any tint.
    inner = _round_rect_mask(xs, ys, pad + 9, pad + 9,
                             w - pad - 9, h - pad - 9, radius * 0.7, soft=2.0)
    rim = np.clip(panel - inner, 0.0, 1.0)
    for c in range(3):
        out[..., c] = out[..., c] * (1.0 - rim * 0.62)

    shine = _round_rect_mask(xs, ys, pad + 14, pad + 12, w - pad - 14,
                             h * 0.34, radius * 0.52, soft=2.0)
    for c in range(3):
        out[..., c] = out[..., c] * (1.0 - shine * 0.18) + 255.0 * shine * 0.18

    return out


def main() -> None:
    print(f"Generating Critter Corral sprites -> {OUT_DIR}")
    # critter_a: warm red/orange; critter_b: cool blue. Bold, distinct hues.
    _save(critter(112, body_rgb=(255, 96, 64), rim_rgb=(196, 44, 36), eye_dir=1.0), "critter_a.png")
    _save(critter(112, body_rgb=(64, 150, 255), rim_rgb=(28, 86, 196), eye_dir=-1.0), "critter_b.png")
    # neutral tintable critter: one shape, N hues via runtime emit color.
    _save(critter_neutral(112), "critter.png")
    _save(pen(256, 200), "pen.png")
    _save(flag(64, 80), "flag.png")
    _save(grass(256), "grass.png")
    _save(lure(128), "lure.png")
    _save(spark(32), "spark.png")
    _save(pip(32), "pip.png")
    # Light-meta upgrade icons (fontless, white -> tinted at emit time). Each
    # icon conveys its effect at a glance for the pick-1-of-3 between waves.
    _save(icon_radius(96), "icon_radius.png")        # ring   = lure radius +
    _save(icon_pull(96), "icon_pull.png")            # arrow  = pull strength +
    _save(icon_second_lure(96), "icon_second_lure.png")  # two dots = 2nd lure
    _save(icon_gate(96), "icon_gate.png")            # gate   = wider pen gates
    _save(icon_calm(96), "icon_calm.png")            # Zzz    = calmer critters
    _save(icon_chain(96), "icon_chain.png")          # chain  = longer chain
    _save(card(192, 240), "card.png")                # rounded card backdrop
    print("Done.")


if __name__ == "__main__":
    main()
