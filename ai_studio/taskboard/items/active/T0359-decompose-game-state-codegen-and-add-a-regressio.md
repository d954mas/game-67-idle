---
id: T0359
title: Decompose game-state codegen and add a regression benchmark
status: backlog
project: P001
epic: E015
priority: P1
tags: [game-state, codegen, benchmark]
created: 2026-07-10
updated: 2026-07-10
---

## What

Split the game-state generator into small explicit stages while retaining its
CLI and generated ABI, then protect generation speed and deterministic output
with a focused benchmark.

## Done when

- [ ] Schema loading/validation, naming, model construction, rendering/events,
      and output writing are separate modules with direct tests.
- [ ] `--schema` is required; output paths derive explicitly from it and no
      process-global namespace or implicit current game remains.
- [ ] Generated provenance comments are game-relative and reproducible across
      machines; unchanged input produces byte-identical output.
- [ ] Existing fixtures prove CLI/API and generated C compatibility before the
      old implementation is removed.
- [ ] A feature-local benchmark records cold and no-op generation near the
      generator; before/after local medians flag a regression above 15% for
      investigation, without creating a flaky cross-machine CI timing gate.

## Open questions

- Establish the benchmark fixture and warm-up/sample count before locking the
  15% threshold.

## Log

- 2026-07-10: Mechanical decomposition only; this task does not redesign game
  state or migrate game-owned state into Studio.
