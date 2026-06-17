---
id: T0014
title: Add product-gate state matrix loader
status: done
epic: E001
priority: P1
tags: [pipeline, product-gate, state-matrix, validation, reusable]
created: 2026-06-17
updated: 2026-06-17
---

## What

Speed up state-covered product gates by allowing `tools/product_gate/review.mjs`
to read a reusable JSON state matrix file. Agents should not have to hand-type
every `--require-state`, `--covered-state`, and `--not-covered-state` argument
for broad UI review.

## Done when

- [x] Product gate accepts a matrix file argument and merges matrix states into
      `state_coverage`.
- [x] Matrix format supports required, covered, and not-covered/debt states.
- [x] CLI arguments can still add or override matrix coverage for one-off gates.
- [x] Tests cover matrix pass and matrix missing-state failure.
- [x] Docs/templates explain the matrix format for future games.

## Open questions

## Log
- 2026-06-17 created after universal state coverage landed; this is the speed
  layer so future agents can use a matrix file instead of long manual CLI args.
- 2026-06-17 implemented `--state-matrix` in
  `tools/product_gate/review.mjs`, added Voxelheim JSON fixture
  `gamedesign/projects/voxelheim/visual/live_state_acceptance_matrix.json`, and
  documented matrix JSON in `gamedesign/knowledge/live_state_acceptance_matrix.md`.
  Evidence: `node --test tools/product_gate/test.mjs` 25/25 pass; matrix smoke
  gate wrote `tmp/matrix_smoke_gate.json`; `node --test tools/ai.test.mjs`,
  `node tools/skills_eval.mjs`, and `node tools/taskboard/cli.mjs validate`
  passed.
