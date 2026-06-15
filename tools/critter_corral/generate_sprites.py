#!/usr/bin/env python3
"""Generate soft, alpha, placeholder sprites for Critter Corral.

DIRECTION: bright, high-contrast, friendly, soft-edged, tactile, "looks like a
game". These are placeholders (Codex refines later) — quality bar = reads as a
game, not a debug screen. Output PNGs are RGBA with transparent backgrounds.

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
    """Round blob: radial-gradient body, soft darker rim, highlight, two eyes."""
    xs, ys, cx, cy = _grid(size)
    r = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2)
    radius = size * 0.46
    out = np.zeros((size, size, 4), np.float32)

    # soft alpha falloff at the edge for a tactile, anti-aliased rim
    alpha = 1.0 - _smoothstep(radius - 2.0, radius + 1.5, r)

    # body: radial gradient (brighter centre -> rim color near edge)
    t = np.clip(r / radius, 0.0, 1.0)[..., None]
    body = np.array(body_rgb, np.float32)
    rim = np.array(rim_rgb, np.float32)
    # lift centre for a glossy, rounded read
    centre = np.clip(body * 1.18, 0, 255)
    rgb = centre * (1.0 - t) + rim * t

    # soft top-left highlight (cartoon shine)
    hx, hy = cx - radius * 0.34, cy - radius * 0.34
    hr = radius * 0.42
    hd = np.sqrt((xs - hx) ** 2 + (ys - hy) ** 2)
    shine = (1.0 - _smoothstep(0.0, hr, hd)) * 0.55
    rgb = rgb + (255.0 - rgb) * shine[..., None]

    out[..., :3] = rgb
    out[..., 3] = alpha * 255.0

    # eyes: white sclera + dark pupil, biased toward travel direction
    ex = cx + eye_dir * radius * 0.16
    ey = cy - radius * 0.16
    gap = radius * 0.30
    for sign in (-1.0, 1.0):
        scx = ex + sign * gap
        sd = np.sqrt((xs - scx) ** 2 + (ys - ey) ** 2)
        sr = radius * 0.20
        eye_a = (1.0 - _smoothstep(sr - 1.5, sr + 1.0, sd))
        for c in range(3):
            out[..., c] = out[..., c] * (1.0 - eye_a) + 255.0 * eye_a
        out[..., 3] = np.maximum(out[..., 3], eye_a * 255.0)
        # pupil
        pcx = scx + eye_dir * sr * 0.35
        pd = np.sqrt((xs - pcx) ** 2 + (ys - ey) ** 2)
        pr = sr * 0.52
        pup_a = (1.0 - _smoothstep(pr - 1.0, pr + 0.8, pd))
        pup = np.array([26.0, 26.0, 36.0], np.float32)
        for c in range(3):
            out[..., c] = out[..., c] * (1.0 - pup_a) + pup[c] * pup_a
    return out


def _round_rect_mask(xs, ys, x0, y0, x1, y1, radius, soft=1.5):
    """Signed-distance rounded-rect -> soft [0,1] coverage mask."""
    rx = np.maximum(np.maximum(x0 + radius - xs, xs - (x1 - radius)), 0.0)
    ry = np.maximum(np.maximum(y0 + radius - ys, ys - (y1 - radius)), 0.0)
    # distance outside the rounded rect
    d = np.sqrt(rx ** 2 + ry ** 2) - radius
    return 1.0 - _smoothstep(0.0, soft, d)


def pen(w: int, h: int):
    """Soft rounded pen panel, white/tintable, with an open top gate + posts."""
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    out = np.zeros((h, w, 4), np.float32)
    pad = 6.0
    radius = 26.0
    panel = _round_rect_mask(xs, ys, pad, pad, w - pad, h - pad, radius, soft=2.0)

    # light fill with a soft vertical sheen (top brighter)
    sheen = 0.86 + 0.14 * (1.0 - ys / h)
    fill_rgb = np.stack([np.full((h, w), 246.0) * sheen,
                         np.full((h, w), 248.0) * sheen,
                         np.full((h, w), 250.0) * sheen], axis=-1)
    out[..., :3] = fill_rgb
    out[..., 3] = panel * 255.0

    # darker rounded rim ring so the panel reads as a tactile object
    inner = _round_rect_mask(xs, ys, pad + 12, pad + 12, w - pad - 12, h - pad - 12, radius * 0.7, soft=2.0)
    rim = np.clip(panel - inner, 0.0, 1.0)
    rim_rgb = np.array([205.0, 212.0, 220.0], np.float32)
    for c in range(3):
        out[..., c] = out[..., c] * (1.0 - rim * 0.9) + rim_rgb[c] * rim * 0.9

    # open top gate: carve a notch out of the top-centre so the entrance reads
    gate_w = w * 0.42
    gx0 = (w - gate_w) * 0.5
    gx1 = gx0 + gate_w
    gate = ((xs > gx0) & (xs < gx1) & (ys < pad + 30.0)).astype(np.float32)
    gate_soft = _smoothstep(0.0, 6.0, np.minimum(xs - gx0, gx1 - xs)) * (1.0 - _smoothstep(pad + 22.0, pad + 32.0, ys))
    carve = np.clip(gate * gate_soft, 0.0, 1.0)
    out[..., 3] = out[..., 3] * (1.0 - carve)

    # bright rounded posts flanking the gate
    for px in (gx0, gx1):
        pd = np.sqrt((xs - px) ** 2 + (ys - (pad + 18.0)) ** 2)
        pr = 13.0
        post_a = (1.0 - _smoothstep(pr - 2.0, pr + 1.0, pd))
        post_rgb = np.array([255.0, 255.0, 255.0], np.float32)
        for c in range(3):
            out[..., c] = out[..., c] * (1.0 - post_a) + post_rgb[c] * post_a
        out[..., 3] = np.maximum(out[..., 3], post_a * 255.0)
    return out


def grass(size: int):
    """Seamless-ish soft pasture tile: green base + gentle lighter blotches."""
    rng = np.random.default_rng(7)
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)
    base = np.array([124.0, 196.0, 102.0], np.float32)  # bright friendly green
    out = np.zeros((size, size, 4), np.float32)
    field = np.tile(base, (size, size, 1))
    # soft sine blotches (tileable) for gentle texture
    tex = (np.sin(xs / size * math.tau * 3.0) * np.cos(ys / size * math.tau * 3.0))
    tex += 0.6 * np.sin(xs / size * math.tau * 7.0 + 1.3) * np.cos(ys / size * math.tau * 5.0)
    tex = tex * 10.0
    field += tex[..., None] * np.array([0.6, 1.0, 0.5], np.float32)
    out[..., :3] = field
    out[..., 3] = 255.0
    return out


def lure(size: int):
    """Soft glowing ring/orb with soft alpha falloff (warm gold)."""
    xs, ys, cx, cy = _grid(size)
    r = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2)
    radius = size * 0.46
    out = np.zeros((size, size, 4), np.float32)
    # glowing ring near the rim
    ring_r = radius * 0.78
    ring = np.exp(-((r - ring_r) ** 2) / (2.0 * (radius * 0.12) ** 2))
    # warm core glow
    core = np.exp(-(r ** 2) / (2.0 * (radius * 0.30) ** 2))
    glow = np.clip(ring * 0.9 + core * 0.7, 0.0, 1.0)
    gold = np.array([255.0, 226.0, 120.0], np.float32)
    hot = np.array([255.0, 250.0, 220.0], np.float32)
    rgb = gold[None, None, :] * (1.0 - core[..., None]) + hot[None, None, :] * core[..., None]
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
    out[..., :3] = 255.0  # white; tinted at emit time by vertex color
    out[..., 3] = np.clip(glow * 255.0, 0, 255)
    return out


def main() -> None:
    print(f"Generating Critter Corral sprites -> {OUT_DIR}")
    # critter_a: warm coral; critter_b: teal. Bold, high-contrast.
    _save(critter(96, body_rgb=(255, 122, 86), rim_rgb=(206, 70, 52), eye_dir=1.0), "critter_a.png")
    _save(critter(96, body_rgb=(70, 198, 196), rim_rgb=(34, 130, 150), eye_dir=-1.0), "critter_b.png")
    _save(pen(256, 192), "pen.png")
    _save(grass(256), "grass.png")
    _save(lure(128), "lure.png")
    _save(spark(32), "spark.png")
    print("Done.")


if __name__ == "__main__":
    main()
