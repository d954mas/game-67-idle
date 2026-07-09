#!/usr/bin/env python3
"""Dress layer prep — body-mask placement of ALREADY-CUTOUT garments.

Law: cutout happens on canvas (cli alpha / alpha-dual-generate / recipe-pack-slice),
NOT in this script. Sources for cut RGBA PNGs:
  tmp/dress_gen/canvas_cuts/<name>.png  (preferred; from canvas export)
  assets/dress/<name>.png               (fallback re-place existing cuts)

Body doll: assets/dress/body_base.png or tmp/dress_gen/canvas_cuts/body_base.png

Placement is proportional (uniform scale) + body-band anchors. No force-height
non-uniform stretch, no alpha dilate "seal" hacks — those warps are why cuts
looked crooked.

Run: .venv\\Scripts\\python.exe games/web-dressup/tools/process_dress_art.py
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

# tools/ -> web-dressup/ -> games/ -> repo root
ROOT = Path(__file__).resolve().parents[3]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

W, H = 512, 896

# (y_ref, band0, band1, width_factor, valign)
# Uniform scale only: fit width to body band; cap height to band_h*1.15
SLOT_FIT = {
    "hair": (95, 28, 220, 2.2, "top"),
    "top": (310, 190, 480, 1.12, "top"),
    "bot": (520, 400, 810, 1.12, "top"),
    "shoe": (845, 780, 892, 1.75, "bottom"),
    "acc": (125, 100, 180, 1.2, "center"),
    "acc_glasses": (118, 100, 165, 1.12, "center"),
    "acc_hat": (60, 20, 140, 1.7, "top"),
    "acc_bag": (500, 430, 620, 0.48, "center"),
    "acc_scarf": (195, 160, 280, 1.12, "center"),
    "shoe_sneak": (845, 780, 892, 1.8, "bottom"),
    "shoe_boot": (830, 750, 892, 1.65, "bottom"),
    "shoe_heel": (850, 790, 892, 1.7, "bottom"),
    "shoe_sandal": (850, 790, 892, 1.7, "bottom"),
}


def slot_of(name: str) -> str:
    for p, s in (
        ("hair_", "hair"),
        ("top_", "top"),
        ("bot_", "bot"),
        ("shoe_", "shoe"),
        ("acc_", "acc"),
        ("look_", "look"),
    ):
        if name.startswith(p):
            return s
    return "look"


def resize_canvas(im: Image.Image) -> Image.Image:
    return im.convert("RGBA").resize((W, H), Image.Resampling.LANCZOS)


def opaque_count(im: Image.Image, thr: int = 12) -> int:
    return int(np.count_nonzero(np.asarray(im.split()[3]) > thr))


def content_bbox(im: Image.Image, thr: int = 16) -> tuple[int, int, int, int]:
    a = np.asarray(im.split()[3])
    ys, xs = np.where(a > thr)
    if len(xs) == 0:
        return 0, 0, im.width, im.height
    return int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1


def body_width_at(body_a: np.ndarray, y: int) -> tuple[int, int, int]:
    y = max(0, min(H - 1, y))
    xs = np.where(body_a[y] > 40)[0]
    if len(xs) == 0:
        return W // 2 - 40, W // 2 + 40, 80
    return int(xs.min()), int(xs.max()), int(xs.max() - xs.min())


def max_body_in_band(body_a: np.ndarray, y0: int, y1: int) -> tuple[int, int, int]:
    best = (W // 2 - 40, W // 2 + 40, 80)
    for y in range(max(0, y0), min(H, y1), 2):
        bl, br, bw = body_width_at(body_a, y)
        if bw > best[2]:
            best = (bl, br, bw)
    return best


def clear_hair_face_window(
    layer: Image.Image, body_rgba: np.ndarray | None = None
) -> Image.Image:
    """Punch face oval so body skin shows (gens often paint solid face in hair)."""
    arr = np.asarray(layer.convert("RGBA")).copy()
    a = arr[:, :, 3]
    solid = a > 40
    if not solid.any():
        return layer
    cx, cy, rx, ry = W // 2, 118, 48, 52
    if body_rgba is not None and body_rgba.shape[2] >= 4:
        ba = body_rgba[:, :, 3]
        br, bg, bb = body_rgba[:, :, 0], body_rgba[:, :, 1], body_rgba[:, :, 2]
        y_lo, y_hi = 55, 165
        skin = (
            (ba[y_lo:y_hi] > 40)
            & (br[y_lo:y_hi] > 170)
            & (bg[y_lo:y_hi] > 120)
            & (bb[y_lo:y_hi] > 100)
            & (br[y_lo:y_hi] > bg[y_lo:y_hi])
        )
        if skin.any():
            ys, xs = np.where(skin)
            cx = int(xs.mean())
            cy = int(ys.mean()) + y_lo
            rx = max(36, int((xs.max() - xs.min()) * 0.42))
            ry = max(40, int((ys.max() - ys.min()) * 0.48))
    yy, xx = np.ogrid[:H, :W]
    ellipse = ((xx - cx) / max(1, rx)) ** 2 + ((yy - cy) / max(1, ry)) ** 2 <= 1.0
    bangs_keep = yy < (cy - int(ry * 0.55))
    face_box = (
        (yy >= cy - ry - 4)
        & (yy <= cy + ry + 8)
        & (xx >= cx - rx - 6)
        & (xx <= cx + rx + 6)
    )
    dark = arr[:, :, :3].astype(np.int16).mean(axis=2) < 70
    center_third = np.abs(xx - cx) < int(rx * 0.92)
    punch = (ellipse & ~bangs_keep) | (face_box & dark & center_third & ~bangs_keep)
    arr[punch, 3] = 0
    return Image.fromarray(arr, "RGBA")


def place_cutout(
    cut: Image.Image,
    slot: str,
    name: str = "",
    body_a: np.ndarray | None = None,
    body_rgba: np.ndarray | None = None,
) -> Image.Image:
    """Uniform scale + anchor. Input must already be alpha-cut (canvas)."""
    cut = cut.convert("RGBA")
    # If full-frame transparent outside subject, crop content first
    x0, y0, x1, y1 = content_bbox(cut)
    crop = cut.crop((x0, y0, x1, y1))
    if crop.width < 2 or crop.height < 2:
        return Image.new("RGBA", (W, H), (0, 0, 0, 0))

    key = name if name in SLOT_FIT else slot
    y_ref, band0, band1, w_fac, valign = SLOT_FIT[key]
    if body_a is None:
        bl, br, bw = W // 2 - 80, W // 2 + 80, 160
    else:
        bl, br, bw = max_body_in_band(body_a, band0, band1)
        bl2, br2, bw2 = body_width_at(body_a, y_ref)
        if bw2 > bw:
            bl, br, bw = bl2, br2, bw2

    target_w = max(24, int(bw * w_fac))
    band_h = max(20, band1 - band0)
    # Uniform scale: prefer width, then cap height — never non-uniform stretch
    scale = target_w / crop.width
    if crop.height * scale > band_h * 1.15:
        scale = (band_h * 1.15) / crop.height
    nw = max(1, int(crop.width * scale))
    nh = max(1, int(crop.height * scale))
    fitted = crop.resize((nw, nh), Image.Resampling.LANCZOS)

    if name == "acc_bag":
        cx = br - nw // 5
    else:
        cx = (bl + br) // 2
    px = cx - nw // 2
    if valign == "top":
        py = band0
    elif valign == "bottom":
        py = band1 - nh
    else:
        py = band0 + (band_h - nh) // 2

    canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    canvas.paste(fitted, (max(0, min(W - nw, px)), max(0, min(H - nh, py))), fitted)
    if slot == "hair":
        canvas = clear_hair_face_window(canvas, body_rgba=body_rgba)
    return canvas


def load_rgba(path: Path) -> Image.Image | None:
    if not path.exists():
        return None
    return Image.open(path).convert("RGBA")


def main() -> int:
    game = Path(__file__).resolve().parents[1]
    tmp = game / "tmp" / "dress_gen"
    cuts_dir = tmp / "canvas_cuts"
    out = game / "assets" / "dress"
    out.mkdir(parents=True, exist_ok=True)
    cuts_dir.mkdir(parents=True, exist_ok=True)

    manifest = json.loads((tmp / "sources.json").read_text(encoding="utf-8"))
    names = [e["name"] for e in manifest.get("layers", [])]

    # Body: prefer canvas cut, else existing asset
    body = load_rgba(cuts_dir / "body_base.png") or load_rgba(out / "body_base.png")
    if body is None:
        raise SystemExit(
            "Missing body_base.png. Export canvas body cut to "
            f"{cuts_dir / 'body_base.png'} first."
        )
    body = resize_canvas(body)
    body.save(out / "body_base.png")
    body_rgba = np.asarray(body)
    body_a = body_rgba[:, :, 3]
    print(f"body_base solid={opaque_count(body, 200)}")

    if manifest.get("stage_bg"):
        p = Path(manifest["stage_bg"])
        if p.exists():
            Image.open(p).convert("RGB").resize((W, H), Image.Resampling.LANCZOS).save(
                out / "stage_bg.png"
            )

    missing = []
    for name in names:
        cut = load_rgba(cuts_dir / f"{name}.png")
        if cut is None:
            # fallback: already-placed asset (re-place without stretch) or raw not allowed
            cut = load_rgba(out / f"{name}.png")
            if cut is None:
                missing.append(name)
                print(f"{name:14} MISSING cut")
                continue
            print(f"{name:14} using existing asset (re-place)")
        else:
            print(f"{name:14} canvas cut")

        slot = slot_of(name)
        layer = place_cutout(cut, slot, name=name, body_a=body_a, body_rgba=body_rgba)
        layer.save(out / f"{name}.png")
        thumb = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        thumb.alpha_composite(body)
        thumb.alpha_composite(layer)
        thumb.save(out / f"{name}_full.png")
        print(f"             slot={slot:4} layer_px={opaque_count(layer):6}")

    proof = tmp / "proof_default_stack.png"
    stack = Image.new("RGBA", (W, H), (40, 30, 40, 255))
    stack.alpha_composite(body)
    for n in ("bot_jeans", "top_tee", "shoe_sneak", "hair_bob", "acc_glasses"):
        p = out / f"{n}.png"
        if p.exists():
            stack.alpha_composite(Image.open(p).convert("RGBA"))
    stack.convert("RGB").save(proof)
    print(f"wrote {proof}")
    if missing:
        print(f"WARNING missing canvas cuts: {missing}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
