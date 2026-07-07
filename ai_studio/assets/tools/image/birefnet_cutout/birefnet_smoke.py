#!/usr/bin/env python3
"""LIVE smoke test for birefnet_cutout: runs the REAL BiRefNet-general model
against two bench fixtures. NOT picked up by ``unittest`` discovery (this
file has no ``unittest.TestCase`` / no ``*_test.py`` suffix on purpose) --
it downloads the ~930MB ONNX checkpoint on first run and takes several
seconds per image even once cached, so it must stay opt-in.

Run it manually from the repo root:
    .venv/Scripts/python.exe ai_studio/assets/tools/image/birefnet_cutout/birefnet_smoke.py

Fixtures used (both already tracked under tmp/alpha_bench/, no new download
needed beyond the model itself):
  - tmp/alpha_bench/scav_magenta.png       -- flat magenta key plate (easy case)
  - tmp/alpha_bench/fixtures/char2_busy.png -- the busy-bg LINE-ART fixture
    where the bench measured birefnet's weakest result (alpha-MAE 9.92 vs
    isnet 3.95, see ai_studio/assets/tools/image/birefnet_cutout/README.md);
    still expected to clear the coarse alpha-sanity bar this smoke checks,
    just with worse numbers than the flat-plate fixture.
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

from ai_studio.assets.tools.image.birefnet_cutout.birefnet_cutout import birefnet_cutout

FIXTURES = [
    ROOT / "tmp" / "alpha_bench" / "scav_magenta.png",
    ROOT / "tmp" / "alpha_bench" / "fixtures" / "char2_busy.png",
]

# Corner/center patch size (px) and the coarse pass/fail bars for the alpha
# sanity check. These are deliberately loose -- this is a smoke test for "the
# real model ran and produced a plausible cutout", not a metrics benchmark
# (tmp/alpha_bench/final/ owns the actual scored comparison).
PATCH = 24
BG_ALPHA_MAX = 80.0
FG_ALPHA_MIN = 160.0


def _corner_alpha(alpha: "np.ndarray") -> float:
    h, w = alpha.shape
    p = min(PATCH, h // 2, w // 2)
    corners = [
        alpha[0:p, 0:p],
        alpha[0:p, w - p:w],
        alpha[h - p:h, 0:p],
        alpha[h - p:h, w - p:w],
    ]
    return float(np.mean([c.mean() for c in corners]))


def _center_alpha(alpha: "np.ndarray", *, search_frac: float = 0.25) -> float:
    """Max mean alpha among small patches within a search window around the
    image centre. A single exact-centre pixel is too fragile a probe: the
    char2_busy fixture's line-art figure has plenty of empty negative space
    at its literal geometric centre (verified against its own ground truth --
    tmp/alpha_bench/fixtures/char2_gt.png reads ~19/255 at the exact centre,
    not the subject at all), so checking only that one pixel would fail on
    fixture geometry rather than on cutout quality. This asks the more robust
    question "is there solid subject content SOMEWHERE near the middle of the
    frame", without depending on any external ground-truth file."""
    h, w = alpha.shape
    p = min(PATCH, h // 2, w // 2)
    ry = max(p, int(h * search_frac))
    rx = max(p, int(w * search_frac))
    cy, cx = h // 2, w // 2
    step = max(4, p // 2)
    best = 0.0
    for dy in range(-ry, ry + 1, step):
        y0, y1 = cy + dy - p // 2, cy + dy + p // 2
        if y0 < 0 or y1 > h:
            continue
        for dx in range(-rx, rx + 1, step):
            x0, x1 = cx + dx - p // 2, cx + dx + p // 2
            if x0 < 0 or x1 > w:
                continue
            best = max(best, float(alpha[y0:y1, x0:x1].mean()))
    return best


def main() -> int:
    failures = []
    for path in FIXTURES:
        if not path.exists():
            failures.append(f"fixture missing: {path}")
            continue
        image = Image.open(path).convert("RGB")
        t0 = time.perf_counter()
        result = birefnet_cutout(image)
        seconds = time.perf_counter() - t0

        assert result.mode == "RGBA", f"{path.name}: expected RGBA output, got {result.mode}"
        alpha = np.asarray(result)[..., 3].astype(np.float64)
        bg = _corner_alpha(alpha)
        fg = _center_alpha(alpha)

        print(
            f"{path.name:20} {image.size} -> {result.size}  {seconds:.2f}s  "
            f"corner_alpha={bg:.1f}  center_alpha={fg:.1f}"
        )

        if bg >= BG_ALPHA_MAX:
            failures.append(f"{path.name}: background corner alpha too high ({bg:.1f} >= {BG_ALPHA_MAX})")
        if fg <= FG_ALPHA_MIN:
            failures.append(f"{path.name}: subject-center alpha too low ({fg:.1f} <= {FG_ALPHA_MIN})")

    if failures:
        print("\nSMOKE FAILED:")
        for f in failures:
            print(f" - {f}")
        return 1

    print("\nSMOKE OK: real birefnet-general model ran on both fixtures with plausible alpha.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
