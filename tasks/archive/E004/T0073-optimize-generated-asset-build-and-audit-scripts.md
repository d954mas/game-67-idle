---
id: T0073
title: Optimize generated asset build and audit scripts
status: done
epic: E004
priority: P1
tags: [performance, assets, generated-art]
created: 2026-06-16
updated: 2026-06-16
---

## What
Continue the generated-asset pipeline performance work after T0071 by
optimizing the scripts around `key_to_alpha`, not only the helper itself.

Observed follow-up bottlenecks:
- `tools/assets/audit_generated_source_derivation.py` spent seconds keying and
  comparing large source crops even when the output dimensions already made the
  asset impossible to audit directly.
- `tools/assets/build_runtime_assets_from_crop_plan.py` keyed each crop
  independently instead of using the intended two-stage pipeline
  (large source sheet cleanup once, then small crop cleanup/audit).

## Done when

- [x] Source-derivation audit rejects dimension-mismatched generated assets
      before expensive chroma cleanup.
- [x] Same-size source derivation compare is vectorized when NumPy is
      available.
- [x] Runtime crop-plan builder keys each source sheet once per chroma key and
      reuses that keyed source for crops.
- [x] Targeted generated-asset tests and T0070 smoke/audit pass.

## Open questions

## Log

- 2026-06-16: Baseline `audit_generated_source_derivation.py` against the T0070
  crop manifest took 6.603s and failed with five expected dimension-mismatch
  problems. This was wasted work because the size mismatch is knowable before
  `key_to_alpha`.
- 2026-06-16: Updated `audit_generated_source_derivation.py` with an early
  output-size guard, source image cache, and NumPy `compare_images`. Same T0070
  manifest now reaches the same expected fail in 0.154s.
- 2026-06-16: Updated `build_runtime_assets_from_crop_plan.py` to cache
  `key_to_alpha(source_sheet, key)` per key color and crop from the keyed source
  instead of re-keying every crop.
- 2026-06-16: Validation passed:
  `python -m tools.assets.chroma_key_alpha_test` (10/10);
  `python -m unittest tools.assets.audit_generated_source_derivation_test tools.assets.build_runtime_assets_from_crop_plan_test`
  (5/5); `tools/critter_corral/import_generated_core_assets.py` (~1.28s);
  generated UI asset audit pass, 11 assets, zero problems, slowest asset
  9.428ms.
- 2026-06-16: Profiler note: current-scope review confidence is not clean
  because the T0070 source-derivation benchmark intentionally returns non-zero
  to prove the fast expected failure path. Treat the command output and JSON
  timing as raw benchmark evidence, not as a clean profiler review guard.
- 2026-06-16: Closed after fast source-derivation guard, NumPy compare, crop-plan keyed-source cache, tests, importer smoke, and generated UI audit.
