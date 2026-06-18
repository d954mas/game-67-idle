---
id: T0013
title: "Cutout v2: principled key-matte + Theorem-4 dual-plate + pair gate"
status: doing
epic: ""
priority: P2
tags: [pipeline, cutout, alpha, assets]
created: 2026-06-18
updated: 2026-06-18
---

## What

Make the two AI-art cutout paths better, simpler, and provable.

- Path 1 (single background): replace the brittle magic-number despill heuristics
  with a principled matte (known-key trimap -> closed-form alpha -> ML foreground
  decontamination). New module `tools/assets/key_matte.py` (pymatting optional,
  deterministic `key_to_alpha` fallback).
- Path 2 (dual-plate): default to the Smith & Blinn Theorem-4 joint-channel
  projection (`alpha-combine proj`) instead of the per-channel min/max/avg guess.
- Generation (the chain): a hardened white->black-as-edit-of-white chain script
  plus a deterministic pair acceptance gate so a redrawn/misaligned pair is
  rejected before extraction.
- Proof: a ground-truth alpha benchmark (synthetic sprites with known alpha) so
  "better and not worse" is a number, not an opinion.

Path 1 is for opaque art + flat-key holes; fractional alpha (soft shadow, glow,
glass) is unrecoverable from one background and routes to path 2. See
`gamedesign/projects/mine-cards/reviews/cutout_benchmark/cutout_benchmark_post.md`.

## Done when

- [x] `key_matte.py` exists, tested, and registered as a benchmark mode (`key matte`)
- [x] dual-plate defaults to `proj` (Theorem 4); `min/max/avg` kept for back-compat
- [x] ground-truth benchmark proves key-matte >> heuristic and proj == min (not worse)
- [x] dual-plate pair gate exists and flags real ghosting pairs
- [x] opt-in `chroma_key.method: "key_matte"` wired into the runtime asset builder
- [x] hardened chain generation script (`gen_dual_plate.sh`) added
- [x] all affected unit tests green
- [ ] validate `key matte` on >=3 real generated mine-cards/seed source crops on
      dark/light/warm backgrounds, then flip the builder DEFAULT from `chroma`
      to `key_matte` for opaque families (keep heuristic as labelled fallback)
- [x] wire the pair gate into the dual-plate extraction so a `regenerate` verdict
      fails the run (exit 1) and a bad pair can't become a runtime asset
- [ ] (optional) numpy-FFT translation pre-align for near-miss pairs (no cv2)

## Open questions

- Per-asset routing trigger: detect "soft edges present -> use dual-plate" vs
  "opaque -> key_matte" automatically, or keep it a manual `method` field?
- Flip the production default now or only after real-asset validation? (current
  choice: keep `chroma` default, `key_matte` opt-in, until real-asset proof.)

## Log

- 2026-06-18: Implemented and verified.
  - `tools/assets/dual_plate_alpha.py`: added `proj` (Theorem-4 joint-channel
    projection) in numpy + python paths; CLI/default now `proj` + `recovery=average`.
  - `tools/assets/key_matte.py` (new) + `key_matte_test.py`: trimap -> CF ->
    `estimate_foreground_ml` -> general key-spill decontam -> bleed/repair/zero.
  - `tools/assets/benchmark_cutout_modes.py`: imports `key_matte_cutout` as the
    `key matte` mode.
  - `tools/assets/benchmark_alpha_truth.py` (new): synthetic known-alpha sprites
    (ring/shadow/glow/gear/glass) scored by alpha SAD/RMSE/grad/IoU, grouped by
    domain. Results (mean alpha SAD, lower better):
    - hard_edge_or_holes (path 1 domain): key matte 1.27 (best single-bg) vs
      current/aggressive heuristic 30.82 -> ~24x better.
    - soft_or_transparent (path 2 domain): dual min/proj 0.0 (exact) vs every
      single-bg 55-102 -> proves dual is required for soft alpha; proj == min.
  - `tools/assets/dual_plate_pair_gate.py` (new): channel-uniformity consistency
    gate. Real pairs: ring `pass` (~3-5% inconsistent), angel-wings-glow
    `regenerate` (33%) -> correctly flags the ghosting case.
  - `.codex/skills/delegated-image-generation/scripts/gen_dual_plate.sh` (new):
    white -> black-as-edit-of-white chain + hardened subject-lock prompt + gate.
  - `tools/assets/build_runtime_assets_from_crop_plan.py`: opt-in per-crop
    `chroma_key.method: "key_matte"` (default `chroma` unchanged).
  - Evidence: `py -3.12 -m pytest ... key_matte_test dual_plate_alpha_test
    chroma_key_alpha_test build_runtime_assets_from_crop_plan_test ...` -> 44 passed.
    Benchmark images: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/
    alpha_truth/alpha_truth_overview.png` and `.../green_plain_ring_bad.png`.
- 2026-06-18: Quality fixes after visual review of the benchmark sheets.
  - Blur: `estimate_foreground_ml` was smoothing the whole image. Now keep the
    crisp ORIGINAL RGB where opaque and use the estimate only in the fractional
    edge band (composited at full res). Sign wood + "MINE" text are sharp again.
  - Green halo inside the ring hole: added a sharp Vlahos limit despill keyed on
    the actual key colour (green-dominant pixels pulled to the avg of the others).
    Ring/sign key-matte green-spill pixel count: 0 (was a visible green rim).
  - Residual olive only remains on the sign's SEMI-TRANSPARENT knots/text-shadow
    (alpha ~190-210) -> fractional alpha is the dual-plate domain; path 1 is now
    at the single-background limit there. 44 tests still green.
- 2026-06-18: Post/benchmark visuals.
  - Swapped the wings fixture to the new richer legendary wings
    (`mine-cards-angel-wings-legendary-black-v001.png`) so the single-bg overview
    shows the current art.
  - New `tools/assets/build_dual_plate_overview.py` -> `dual_overview.png`: the
    path-2 counterpart of the single-bg overview, on the SAME real arts, three
    frames each (white plate | black plate | result) + per-row pair-gate verdict.
    Legendary wings use the legendary white/black pair; others use the per-asset
    pairs.
  - New `tools/assets/build_combined_overview.py` -> `combined_overview.png`: ONE
    sheet, every art in a single row showing BOTH paths -- single-bg methods
    (source..key matte) + a divider + dual-plate (white | black | result).
  - Post images available: `combined_overview.png` (everything in one), or the
    pair `overview.png` (path 1) + `dual_overview.png` (path 2).
  - Wings fixture source changed to the real white plate (`...legendary-white...`)
    with key `#ffffff` (no fake green recomposite) so the single-bg comparison is
    honest (it eats the white feathers; dual recovers them).
  - `dual_plates_for_case` (shared by both overview builders): synthetic/procedural
    fixtures (sign, floor shadow) now build PERFECTLY ALIGNED white/black plates
    from the reconstructed truth -> no generator-redraw ghosting in their dual
    result (fixed the "MINE sign doubled/blurry" pair). Real green-screen arts
    still use their per-asset AI pair; wings use the legendary pair.
- 2026-06-18: Pair gate wired into the pipeline + sign pair regenerated live.
  - `dual_plate_alpha.py` now runs the pair consistency gate on every extraction:
    a `regenerate` verdict (subject redrawn/misaligned) sets verdict=fail and
    exits 1, so a ghosted pair can't become a runtime asset. `--skip-pair-gate`
    to bypass; CLI also resizes dark->light so real differently-sized AI pairs
    don't crash (real misalignment still caught by the gate). Proven: sign
    per-asset pair (20% inconsistent) -> fail/exit 1; floor pair (1.6%) -> pass.
  - Floor dual reverted to its REAL per-asset pair (gate pass, no synthetic green
    rim); `dual_plates_for_case` now prefers the real generated pair, truth-derived
    only as fallback.
  - Sign pair regenerated via the CHAIN (black = edit of the padded white plate,
    `generate_image.py` oauth/gpt-image-2, ~61s): inconsistent_fraction 0.0002
    (was 0.20) -> clean sharp dual. Swapped into the per-asset folder; overviews
    refreshed.
- 2026-06-18: Remaining = real-asset validation + flip production default to the
  matte. Kept production default on the heuristic until real-asset proof.
