---
id: T0020
title: Codify visual-first gate and session contract
status: done
epic: E003
priority: P0
tags: [visual-gate, product-gate, skills, workflow]
created: 2026-06-15
updated: 2026-06-15
---

## What

Turn the fishing review lessons into the default workflow for future visual
prototype work: fake shot and current native screenshot first, mismatch list
before code, one vertical art slice before feature expansion, and product gate
as a stop condition when visual quality fails.

This replaces the failed pattern from Splash Rods where gameplay, state, model
pipeline, generated UI, and visual rescue expanded at the same time while the
latest product gate was still red.

## Done when

- [x] `AI_PIPELINE.md` and relevant skills require a 5-line session contract for
      visual prototype work: goal, non-goal, proof, stop condition, likely files.
- [x] Visual work instructions require screenshot vs fake-shot comparison before
      coding and after meaningful render changes.
- [x] Generated UI instructions require non-empty crop/runtime manifest and
      pixel audit before runtime integration claims.
- [x] Product gate failure explicitly blocks feature/content expansion unless
      the lead accepts the debt.
- [x] Validation proves the updated workflow docs/skills are internally
      consistent.

## Open questions

- Answered: both. `tools/game_context/iteration_context.mjs` now emits a
  machine-readable `visual_first_contract`, and the rules are also codified in
  `AI_PIPELINE.md` and the relevant skills.

## Log

- 2026-06-15: Created from fishing review findings: first runtime view was
  programmer art, product gate arrived too late, UI audit was run after runtime
  integration, and visual claims were not tied tightly enough to screenshot
  proof.
- 2026-06-15: Started profiler scope with
  `node tools/ai.mjs start T0020 visual-first-gate`.
- 2026-06-15: Added `visual_first_contract` to
  `tools/game_context/iteration_context.mjs`, updated new prototype skeletons
  to require 5-line visual contracts and screenshot-vs-target mismatch lists,
  and codified the same rules in `AI_PIPELINE.md`,
  `game-visual-art-direction`, `game-feature-iteration`,
  `generated-game-ui-assets`, and `primary-gdd-pipeline`.
- 2026-06-15: Validation passed:
  `node --test tools/game_context/test.mjs`,
  `node tools/skills_eval.mjs`,
  `node tools/taskboard/cli.mjs validate`, and
  `node tools/pipeline_validate.mjs`.
