---
id: T0172
title: Refactor game state management workflow
status: done
epic: E001
priority: P2
tags: [game-project, state, skill, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What
Move the legacy `game-state-management` skill and `tools/state_codegen` into AI
Studio ownership. Keep the real generator and tests, fix the broken default
schema path for the AI Studio root, and replace the old skill references with a
thin `nt-game-state-management` router.

## Done when

- [x] Reviewed legacy state skill and generator/test/template.
- [x] Created `ai_studio/game_project/state_management/` with generator, tests,
      template, and concise docs.
- [x] Fixed generator defaults for game roots and AI Studio template root.
- [x] Replaced legacy skill surface with `nt-game-state-management`.
- [x] Updated `ai_studio/tree.json`, routes, and generated agent surfaces.
- [x] Validated generator, map, docs, skill surface, and taskboard.
- [x] Committed and pushed the slice.

## Open questions

- Resolved: generated state output stays generated on demand for this slice.
  The generator, template, schema, tests, and docs are the source of truth.

## Log

- 2026-07-01: Started migration slice after map validation showed
  `game-state-management` and `tools/state_codegen` as unmapped legacy.
- 2026-07-01: Review found the generator is real but currently broken in AI
  Studio root: it expects `state/game_state.schema.json`, while this repository
  stores the reusable seed schema in `template/state/game_state.schema.json`.
- 2026-07-01: Moved the generator/test/template into
  `ai_studio/game_project/state_management/`, replaced the old skill with
  `nt-game-state-management`, generated `.claude` surface, and added the module
  to `ai_studio/tree.json`.
- 2026-07-01: Validation passed: generator unittest, explicit tmp smoke output,
  skill sync check, architecture map validation, doc reference check, skill
  frontmatter check, and taskboard validation.
- 2026-06-30: Moved game state management into AI Studio, fixed generator defaults, added map coverage, and validated generator/docs/surfaces/taskboard.
