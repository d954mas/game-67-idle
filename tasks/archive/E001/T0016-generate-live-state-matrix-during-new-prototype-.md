---
id: T0016
title: Generate live-state matrix during new prototype startup
status: done
epic: E001
priority: P2
tags: [pipeline, prototype-startup, product-gate, ui, matrix]
created: 2026-06-17
updated: 2026-06-17
---

## What

Add a reusable startup step for future game prototypes that creates a
project-specific live-state acceptance matrix from
`gamedesign/knowledge/live_state_acceptance_matrix.md` before the first visual
or UI slice is accepted. The generated matrix should be referenced by product
gate commands so missing states become explicit debt from the start.

## Done when

- [x] New-prototype startup docs or tooling create a project matrix fixture.
- [x] The generated fixture includes required state tags and acceptance proof
      prompts for HUD, primary CTA, modal, blocked/affordable, feedback, and
      any game-specific live-risk states.
- [x] Product gate examples in the startup path include `--state-matrix` and
      at least one `--require-state`.
- [x] A smoke test or documented command proves a new project can create and
      load the matrix.

## Open questions

- Should this live in `primary-gdd-pipeline`, a task template, or a dedicated
  `tools/project_startup/` helper?

## Log
- 2026-06-17 captured after Voxelheim accepted a narrow screenshot state that
  hid live UI overlap and edge regressions.
- 2026-06-17 implemented in `tools/game_context/new_prototype.mjs` and
  `tools/game_context/iteration_context.mjs`; `tools/game_context/test.mjs`
  proves kickoff creates the matrix and a strict product gate can load it.
