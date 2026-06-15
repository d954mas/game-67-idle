---
id: T0031
title: Wire visual-strict rubric into new prototype kickoff
status: done
epic: E003
priority: P1
tags: [visual, product-gate, startup]
created: 2026-06-15
updated: 2026-06-15
---

## What

`new_prototype.mjs` creates `reviews/first_slice_visual_gate.md`, but the
template predates the strict visual critique rubric. New prototype kickoff
should put the `--visual-strict` product gate command shape, six visual score
axes, and blocker/major issue rule directly into the first-slice artifact.

## Done when

- [x] New prototype first-slice visual gate template includes
      `--visual-strict`.
- [x] Template lists all six rubric axes and visual issue severities.
- [x] Generated status/readme/gdd text points the first slice at the strict
      visual rubric when visual quality matters.
- [x] Game-context tests assert the generated artifact contains the strict
      rubric.
- [x] Game-context/taskboard/profiler validation passes.

## Open questions

## Log

- 2026-06-15: Started after T0030 added `--visual-strict` but kickoff
  skeleton still created a generic product-read gate template.
- 2026-06-15: Wired `--visual-strict`, six score axes, and visual issue
  severities into `new_prototype.mjs` first-slice gate/status/readme/gdd
  output and added game-context assertions. Validation: `node --check
  tools/game_context/new_prototype.mjs`, `node --test tools/game_context/test.mjs`,
  `node tools/skills_eval.mjs`, profiled quick `node tools/pipeline_validate.mjs`,
  `git diff --check ...`, `node tools/taskboard/cli.mjs validate`, and
  `node tools/ai.mjs status --require-current-scope-usable` passed.
