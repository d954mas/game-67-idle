---
id: T0071
title: Optimize generated asset chroma-key pipeline hot paths
status: done
tags: []
epic: E004
priority: P1
created: 2026-06-16
updated: 2026-06-16
---

## What
The generated-art pipeline hit a severe iteration-loop slowdown while importing
T0070 1.5K generated PNGs. The shared chroma helper timed out at 60s for a few
large source images, while a T0070-specific NumPy path completed in about 1s.

Hot path observed:
- `tools/assets/chroma_key_alpha.py::key_to_alpha`
- per-pixel border flood fill with `deque`
- per-pixel halo/spill repair passes after key removal
- used by `tools/assets/build_runtime_assets_from_crop_plan.py` and
  `tools/assets/audit_generated_source_derivation.py`

Current threading status: no obvious `ThreadPool`, `ProcessPool`,
`multiprocessing`, `pthread`, `OpenMP`, or similar parallel path was found in
`tools/` or `src/` during the quick scan. The T0070 speedup came from
vectorization, not multithreading.

Recommended direction: optimize only the hot chroma-key/halo stages first. Use
NumPy/SciPy-style connected-component/vectorized masks if acceptable, or move
the exact hot path to native C/C++/Rust only if profiling shows Python/NumPy is
still too slow. Do not rewrite the whole asset pipeline until profiling proves
the cost is broader than chroma-key cleanup.

## Done when

- [x] Add a benchmark/profile fixture using representative 1.5K generated
      chroma-key PNGs.
- [x] Replace or accelerate `key_to_alpha` hot loops without regressing existing
      chroma-key tests.
- [x] Decide whether native code is needed after profiling the vectorized path.
- [x] Document whether parallelism is used and where it is intentionally not
      used.

## Open questions

## Log

- 2026-06-16: Created from T0070 visual pass slowdown. Shared helper timed out
  on large generated images; T0070 importer uses a temporary NumPy fast path.
- 2026-06-16: Corrected AI profiling scope with
  `node tools/ai.mjs start T0071 chroma-key-native-optimization` before the
  useful benchmark runs. Current-scope profiler review confidence remains
  broken because two early failed records were caused by invalid profiler labels
  and direct Python import path mistakes; use the command output and benchmark
  JSON as raw timing evidence, not as complete review evidence.
- 2026-06-16: Baseline measured on
  `generated-critter-source-v1.png`: shared `key_to_alpha` took 11.419s on a
  1254x1254 PNG. The full T0070 importer, which used its temporary NumPy
  shortcut, took 1.622s.
- 2026-06-16: Added `tools/assets/benchmark_chroma_key_alpha.py` and a NumPy
  large-image fast path in `tools/assets/chroma_key_alpha.py`: vectorized
  border-connected key mask, edge cleanup, box-sum halo repair, and no wasted
  transparent-RGB bleed before final zeroing. Representative optimized
  `key_to_alpha` timings: critter 1.373s, pen 1.217s, card 1.301s, upgrade
  icons 0.576s.
- 2026-06-16: Added native `asset_chroma_key_native`
  (`tools/assets/native_chroma_key.c`, CMake target) for measured fallback and
  threading experiments. Native best no-write timings with `--threads 8`:
  critter 0.087s, pen 0.108s, card 0.091s, upgrade icons 0.090s. On critter,
  `--threads 1` was 0.137s; row cleanup improved with threads, but PNG load and
  border flood-fill cap the total gain.
- 2026-06-16: Cached source-level chroma cleanup in
  `tools/critter_corral/import_generated_core_assets.py` so repeated variants
  do not reload/re-key the same generated PNG. Full importer improved from
  about 1.62s to about 1.27s.
- 2026-06-16: Validation: `python -m tools.assets.chroma_key_alpha_test` passed
  10/10; `tools/critter_corral/import_generated_core_assets.py` passed after
  optimization; generated UI asset audit passed 11 assets, zero problems,
  slowest asset 8.556ms.
- 2026-06-16: Closed after Python vectorized key_to_alpha benchmark, native chroma utility, threading benchmark, unit tests, importer smoke, and generated UI asset audit.
