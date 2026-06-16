---
id: T0074
title: Refactor generated asset runtime cleanup hot path
status: done
epic: E004
priority: P1
tags: [performance, assets, generated-art]
created: 2026-06-16
updated: 2026-06-16
---

## What
After T0071/T0073, the remaining generated-asset importer cost was not trim or
resize. `cProfile` showed `fit_to_canvas` dominated by strict cleanup on small
runtime PNGs:
- `bleed_transparent_rgb`
- `repair_transparent_edge_rgb`
- `remove_green_screen_spill`
- `repair_visible_halo`
- other spill/halo passes

The lead asked to try the same path fully native and provide benchmarks.

## Done when

- [x] Public Python strict-cleanup helpers have NumPy fast paths where safe.
- [x] A native batch generated-core importer exists for the current T0070 asset
      set and runs source decode -> chroma clear -> crop -> bbox/resize ->
      strict cleanup -> optional PNG write without Python/PIL.
- [x] Native output passes the generated UI asset audit.
- [x] Benchmarks compare Python importer, native no-write, and native write.

## Open questions

## Log

- 2026-06-16: Committed previous snapshot first:
  `0a95919 Improve Critter Corral generated art pipeline`.
- 2026-06-16: Measured trim/resize: crop about 0.008s, resize about 0.048s,
  alpha bbox about 0.014s inside a profiled Python importer run. Strict cleanup
  was the real remaining Python cost.
- 2026-06-16: Added NumPy fast paths to shared cleanup helpers in
  `tools/assets/chroma_key_alpha.py`: edge fringe, source-key spill,
  green-screen spill, visible halo repair, and transparent-edge RGB repair.
  Python importer improved to about 0.98-1.03s for 11 runtime assets.
- 2026-06-16: Added native batch benchmark/import tool
  `tools/critter_corral/import_generated_core_assets_native.c` and CMake target
  `import_generated_core_assets_native`. It is benchmark-oriented: can write
  PNGs to an output dir or run `--no-write`; it does not yet replace the Python
  manifest/provenance writer.
- 2026-06-16: Native benchmark after audit-safe cleanup:
  `--no-write` best 0.410s for 11 assets; PNG write best 0.474s. Python importer
  comparison run: 0.977s. Native output generated UI audit passed: 11 assets,
  zero problems, slowest audited asset 9.48ms.
- 2026-06-16: Build note: one failed build was caused by a self-inflicted
  Windows exe lock from running benchmark while linking the same binary; retry
  passed after sequencing build before benchmark.
- 2026-06-16: Closed after NumPy cleanup fast paths, native batch importer, benchmarks, native output audit, and focused validation.
