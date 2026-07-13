---
id: T0359
title: Decompose game-state codegen and add a regression benchmark
status: done
project: P001
epic: E015
priority: P1
tags: [game-state, codegen, benchmark]
created: 2026-07-10
updated: 2026-07-11
---

## What

Split the game-state generator into small explicit stages while retaining its
CLI and generated ABI, then protect generation speed and deterministic output
with a focused benchmark.

## Done when

- [x] Schema loading/validation, naming, model construction, rendering/events,
      and output writing are separate modules with direct tests.
- [x] `--schema` is required; output paths derive explicitly from it and no
      process-global namespace or implicit current game remains.
- [x] Generated provenance comments are game-relative and reproducible across
      machines; unchanged input produces byte-identical output.
- [x] Existing fixtures prove CLI/API and generated C compatibility before the
      old implementation is removed.
- [x] A feature-local benchmark uses one frozen representative multi-fragment
      schema fixture, 3 warmups and at least 20 measured warm runs, reports
      median/p90 plus 3-5 cold runs, and treats a local median regression above
      15% as advisory investigation rather than a flaky cross-machine CI gate.

## Open questions

None.
## Log

- 2026-07-10: Mechanical decomposition only; this task does not redesign game
  state or migrate game-owned state into Studio.
- 2026-07-10: T0377 follows this decomposition to avoid overlapping schema/generator edits.
- 2026-07-10: Resolved planning detail: benchmark fixture, warmups, sample counts, and advisory threshold are fixed in Done when.
- 2026-07-11: Checkpoint: the 1,578-line generate_state.py still owns schema validation, naming/model construction, all renderers, writes, CLI defaults, and machine-dependent relative labels in one module; --schema silently falls back to the template. Existing generator regression suite is 31/31 green from the T0357 canonical Python run. Starting mechanical staged decomposition and frozen benchmark only; T0377 schema-contract changes remain out of scope.
- 2026-07-11: Replaced the monolith with a thin compatible facade over explicit schema, naming, model, state renderer, event renderer, and output modules. `--schema` is required; default output and provenance derive from the explicit schema path, with project-relative `state/...` labels for games/templates and deterministic external labels.
- 2026-07-11: Added direct module/CLI/API/determinism/provenance tests and a canonical feature test entrypoint. Goldens changed only in reproducible provenance comments; unchanged inputs remain byte-identical. CMake dependencies now include every generator module for both template and web-dressup.
- 2026-07-11: Verification: canonical aggregate 45/45 passes (31 legacy/golden, 10 module/CLI/API/determinism/provenance, 4 benchmark contracts); seven generated C translation units compile; web native rebuild and CTest 23/23 pass; py_compile, Architecture Map strict (343 mapped / 775 scanned, 0 issues), Taskboard validation, and cached diff check pass.
- 2026-07-11: Benchmark uses one frozen four-fragment fixture with SHA-256 metadata, 3 warmups, 25 measured warm runs, and 3 cold processes. Lead run: median 4.604 ms, p90 4.850 ms, cold 102.9-105.8 ms; compatible local baseline 4.412 ms, +4.34%, advisory none. Cross-platform metadata mismatch disables comparison; the 15% threshold is advisory only.
- 2026-07-11: Independent review cycle 1 found 0 HIGH and 3 actionable MEDIUM: provenance scope, baseline metadata/hash compatibility, and incomplete canonical test entrypoint. All were fixed. Cycle 2 reported 0 HIGH and 0 actionable MEDIUM/LOW across architecture, correctness, ownership, tests, process, performance, and context cost. Lead diff gate additionally removed extraction whitespace and repeated all affected checks.
- 2026-07-11: Quality: QTECH_001=pass; evidence: aggregate generator tests, reproducible goldens, generated C compilation, native build/CTest, frozen benchmark, strict validation, and independent diff review.
- 2026-07-11: Closed after staged generator decomposition, reproducible provenance, ABI/golden/native compatibility, advisory benchmark, and two clean review cycles.
