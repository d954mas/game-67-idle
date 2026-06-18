#!/usr/bin/env python3
"""Timing harness for the two cutout paths, for optimization work.

Path 1 (key_matte): per-step breakdown (prep+trimap, closed-form alpha, ML
foreground, finalize) at a couple of input sizes — the closed-form solve scales
with pixel count, so size matters.
Path 2 (dual_plate): extraction (Theorem-4 proj) + the pair consistency gate.

Generation timing is external (gpt-image-2 via API, ~30-60 s/image) and is not
re-measured here to avoid burning quota; see the note printed at the end.

Run: py -3.12 tools/assets/profile_cutout.py
"""
from __future__ import annotations

import statistics
import sys
from pathlib import Path
from time import perf_counter

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.assets.dual_plate_alpha import extract_dual_plate_alpha
from tools.assets.dual_plate_pair_gate import evaluate
from tools.assets.key_matte import key_matte_cutout

CAND = ROOT / "gamedesign/projects/mine-cards/art/candidates"
PER_ASSET = ROOT / "gamedesign/projects/mine-cards/reviews/cutout_benchmark/dual_plate_transfer/per_asset"
RUNS = 5


def median_ms(fn) -> float:
    samples = []
    for _ in range(RUNS):
        start = perf_counter()
        fn()
        samples.append((perf_counter() - start) * 1000.0)
    return round(statistics.median(samples), 1)


def profile_key_matte(name: str, crop: Image.Image) -> None:
    timings: dict = {}
    key_matte_cutout(crop, (0, 255, 0), timings=timings)  # warmup (numba JIT) + fill steps
    total = median_ms(lambda: key_matte_cutout(crop, (0, 255, 0)))
    print(f"  path1 key_matte [{name}] {crop.width}x{crop.height}: {total} ms total")
    for step, ms in timings.items():
        print(f"      {step:28s} {ms} ms")


def main() -> int:
    print("=== PATH 1 — key_matte (single-background) ===")
    ring = Image.open(CAND.parent / "candidates/mine-cards-equipment-source-v001-candidate-b-normalized.png").convert("RGBA").crop((66, 450, 66 + 235, 450 + 186))
    profile_key_matte("ring small", ring)
    profile_key_matte("ring 2x", ring.resize((ring.width * 2, ring.height * 2), Image.LANCZOS))

    print("\n=== PATH 2 — dual_plate (white/black) ===")
    white = Image.open(PER_ASSET / "green_floor_shadow_bad/ai_white_from_source.png").convert("RGBA")
    black = Image.open(PER_ASSET / "green_floor_shadow_bad/ai_black_from_source.png").convert("RGBA")
    if black.size != white.size:
        black = black.resize(white.size, Image.LANCZOS)
    extract_dual_plate_alpha(white, black, recovery_source="average")  # warmup
    extract_ms = median_ms(lambda: extract_dual_plate_alpha(white, black, recovery_source="average"))
    gate_ms = median_ms(lambda: evaluate(white, black))
    print(f"  path2 dual extract [floor] {white.width}x{white.height}: {extract_ms} ms")
    print(f"  path2 pair gate: {gate_ms} ms")

    print("\nnote: generation (gpt-image-2) is API-bound, ~30-60 s/image and not")
    print("locally optimizable; it dominates the whole pipeline by 1000x+.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
