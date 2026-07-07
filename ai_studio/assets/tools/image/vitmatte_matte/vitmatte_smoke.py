#!/usr/bin/env python3
"""LIVE smoke test for vitmatte_matte (tool venv, real GPU) -- no mocks, no
fixtures shortcut. Runs the actual model on the glow-wings magenta plate from
the alpha-methods-portfolio bench (tmp/alpha_bench/fixtures/
wings_magenta_native.png, 1254x1254, native resolution -- the SAME fixture the
bench's run_vitmatte.py used for the "glow" class) and asserts the three
things a broken pipeline would get wrong silently:

  1. alpha is FRACTIONAL (a meaningful band of pixels strictly between 0 and 1)
     -- not just a binarized silhouette, which would mean the trimap/model
     path collapsed to a hard cutout and lost the whole reason to use ViTMatte
     here.
  2. a background corner is transparent (near 0) -- the auto-trimap correctly
     read the flat magenta plate as background.
  3. the subject is opaque somewhere (near 1) -- the model actually resolved
     a solid foreground core, not just noise.

Run: .venv/Scripts/python.exe ai_studio/assets/tools/image/vitmatte_matte/vitmatte_smoke.py
(from the repo root, using THIS TOOL's own venv, not the shared repo one).
"""
from __future__ import annotations

import sys
import time
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[5]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.image.vitmatte_matte.matte_math import (  # noqa: E402
    DESPILL_ALPHA_CLAMP,
    build_auto_trimap,
    despill,
    trimap_stats,
)
from ai_studio.assets.tools.image.vitmatte_matte.vitmatte_matte import (  # noqa: E402
    load_model,
    predict_alpha_with_oom_fallback,
)

FIXTURE = ROOT / "tmp" / "alpha_bench" / "fixtures" / "wings_magenta_native.png"
KEY = (255, 0, 255)
OUT_DIR = ROOT / "tmp" / "alpha_bench" / "vitmatte_matte_smoke"
OUT_PATH = OUT_DIR / "wings_glow_smoke_rgba.png"
CORNER_PATCH = 24  # px; averaged so one stray pixel can't pass/fail the smoke


def main() -> int:
    if not FIXTURE.exists():
        raise SystemExit(f"smoke fixture missing: {FIXTURE}")

    plate = Image.open(FIXTURE).convert("RGB")
    trimap = build_auto_trimap(plate, key=KEY)
    stats = trimap_stats(trimap)

    processor, model, device = load_model()
    start = time.perf_counter()
    alpha, device_used = predict_alpha_with_oom_fallback(processor, model, device, plate, trimap)
    seconds = time.perf_counter() - start

    plate_u8 = np.asarray(plate, dtype=np.uint8)
    rgb_out = despill(plate_u8, alpha, KEY)
    alpha_out = np.where(alpha < DESPILL_ALPHA_CLAMP, 0.0, alpha)
    alpha_u8 = np.clip(np.rint(alpha_out * 255.0), 0, 255).astype(np.uint8)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    Image.fromarray(np.dstack([rgb_out, alpha_u8]), "RGBA").save(OUT_PATH)

    fractional = (alpha > 0.05) & (alpha < 0.95)
    fractional_pct = 100.0 * float(fractional.mean())
    corner_mean = float(alpha[:CORNER_PATCH, :CORNER_PATCH].mean())
    subject_max = float(alpha.max())

    print(f"[vitmatte_smoke] {FIXTURE.name} {plate.size} device={device_used} {seconds:.2f}s")
    print(f"[vitmatte_smoke] trimap={stats}")
    print(f"[vitmatte_smoke] fractional_pct={fractional_pct:.2f}  corner_mean={corner_mean:.4f}  subject_max={subject_max:.4f}")
    print(f"[vitmatte_smoke] saved -> {OUT_PATH}")

    assert fractional_pct > 1.0, f"expected a real fractional-alpha band on the glow fixture, got {fractional_pct:.2f}%"
    assert corner_mean < 0.05, f"expected the background corner near-transparent, got mean alpha {corner_mean:.4f}"
    assert subject_max > 0.9, f"expected an opaque subject core, got max alpha {subject_max:.4f}"
    print("[vitmatte_smoke] PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
