---
id: T0075
title: Make native generated asset importer production-capable
status: doing
epic: E004
priority: P1
tags: [performance, assets, generated-art, native]
created: 2026-06-16
updated: 2026-06-16
---

## What

Make the generated core asset native path production-correct without creating a
second source of truth for asset semantics.

The corrected design is:

- Python `tools/critter_corral/import_generated_core_assets.py` remains the
  orchestrator and the only owner of `ASSETS`, crop/runtime manifests, contact
  sheet, crop/resize/tint logic, and production output rules.
- Native `tools/critter_corral/import_generated_core_assets_native.c` is a
  worker for heavy source chroma-keying only.
- Native mode writes temporary raw RGBA keyed source caches, then Python runs
  the same crop/resize/tint/manifest path as normal Python mode.

This avoids the bad two-importer split where C duplicated JSON metadata and
produced visually-valid but non-identical PNGs.

## Done when

- [x] Python mode writes the production PNGs/manifests/contact sheet and passes
      generated asset audit.
- [x] Native mode writes the same production PNGs/manifests/contact sheet
      through the same Python orchestration path.
- [x] Native output is pixel-identical to Python output for all 11 generated
      runtime PNGs.
- [x] Native worker supports configurable row threading for source keying.
- [x] Benchmarks record Python full path, native full path, and native worker
      key-stage timing.

## Open questions

- Explicit SIMD is not implemented yet. Current native worker is scalar C with
  row threading; compiler auto-vectorization may happen, but there is no
  hand-written SSE2/AVX2/NEON path. SIMD should be a separate optimization with
  runtime dispatch and bit-identical parity tests.
- Full-transform native path can be faster, but it is not production-correct
  yet because STB resize/cleanup/tint differs from PIL output. Keep it as a
  benchmark/spike path only unless exact parity is solved.

## Log

- 2026-06-16: Rejected the "two full production importers" direction after
  native full-transform output passed audit but differed heavily from Python
  PNGs (`changed_ratio` roughly 0.32-0.58 across most assets). Root cause:
  duplicated resize/cleanup/tint semantics.
- 2026-06-16: Changed Python importer to `--mode python|native`. Native mode
  now uses C only for source chroma-keying and keeps Python as the single
  production orchestrator.
- 2026-06-16: Replaced intermediate keyed PNGs with temporary raw RGBA cache
  files to avoid PNG encode/decode overhead.
- 2026-06-16: Added `--threads N` to the native worker and
  `--native-threads N` to the Python orchestrator.
- 2026-06-16: Benchmark evidence:
  - Python full path: 0.999s wall-clock on the measured run.
  - Native full path through Python orchestrator, raw RGBA cache: 1.006s
    wall-clock on the measured run.
  - Native worker key stage, write raw cache, best of 5:
    - 1 thread: 0.277s
    - 2 threads: 0.261s
    - 4 threads: 0.253s
    - 8 threads: 0.248s
  - Native worker key stage, no write, best of 5:
    - 1 thread: 0.269s
    - 2 threads: 0.248s
    - 4 threads: 0.242s
    - 8 threads: 0.241s
- 2026-06-16: Validation evidence:
  - Python audit: `pass: checked 11 generated UI asset(s)`.
  - Native threaded audit: `pass: checked 11 generated UI asset(s)`.
  - Pixel parity: all 11 generated runtime PNGs identical between Python and
    native mode.
- 2026-06-16: Follow-up profile found the new bottleneck in runtime canvas
  cleanup, not native keying:
  - Before reducing bleed passes: Python full path about 0.999s; native full
    path about 1.006s.
  - `bleed_transparent_rgb_numpy` consumed about 0.39s of the run.
  - Reduced importer-local `bleed_transparent_rgb` from 16 passes to 2 passes.
  - After reduction: Python full path 0.709s; native full path 0.664s.
  - Audit still passed for 11 generated UI assets.
  - Python/native PNG parity remained pixel-identical for all 11 runtime PNGs.
- 2026-06-16: Added persistent temp caches for keyed source RGBA and runtime
  output validity:
  - Keyed source cache invalidates on source path, source mtime, source byte
    size, key color, and key algorithm version.
  - Runtime output cache invalidates on the full asset spec, source
    fingerprint, and runtime algorithm version.
  - Native worker now runs only for stale sources that are actually needed by
    stale runtime outputs.
  - Measured native full-cold after cache changes: 0.667s.
  - Measured native warm/no-change after cache changes: 0.188s.
  - Measured Python warm/no-change after cache changes: 0.188s.
  - Audit still passed for 11 generated UI assets.
  - Python/native warm PNG parity remained pixel-identical for all 11 runtime
    PNGs.
- 2026-06-16: Warm/no-change profile showed the remaining importer time was
  mostly module import overhead. Made NumPy, PIL tint helpers, and
  `tools.assets.chroma_key_alpha` imports lazy so cache-hit runs do not pay for
  pixel-processing modules:
  - Warm native before lazy imports: about 0.188s.
  - Warm native after lazy imports: 0.140-0.145s.
  - Warm Python after lazy imports: 0.146s.
  - Native full-cold remained about 0.664s.
  - Audit still passed for 11 generated UI assets.
  - Python/native warm PNG parity remained pixel-identical for all 11 runtime
    PNGs.
- 2026-06-16: Added a fast import stamp for true no-op runs:
  - Stamp covers runtime output files, crop manifest, asset manifest, contact
    sheet, asset specs, and algorithm versions.
  - Fast path exits before importing PIL/NumPy/native worker helpers when all
    outputs are current.
  - Missing-output invalidation was tested by moving `icon_chain.png`; the
    importer rebuilt it and refreshed manifests/contact sheet/stamp.
  - Warm native no-change after import stamp and lazy atomic-IO imports:
    0.062-0.066s wall-clock.
  - cProfile no-change time inside Python process: 0.019s; remaining cost is
    mostly process startup, `pathlib`/JSON/stat checks, and file metadata
    validation.
  - Native full-cold rebuild after these changes: 0.626s.
  - Python full-cold rebuild after these changes: 0.666s.
  - Pixel parity: all 11 generated runtime PNGs remained identical between
    native and Python paths.
  - Audit still passed for 11 generated UI assets; slowest audited asset was
    `generated_upgrade_card` at about 9.6ms.
