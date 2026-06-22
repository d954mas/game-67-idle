---
id: T0029
title: First native playable slice for Dragon Grove
status: done
epic: E004
priority: P1
tags: [prototype, dragon-grove, native-first]
created: 2026-06-21
updated: 2026-06-22
---

## What

Build the first native playable slice for `Dragon Grove`: one Y-up logical 5x5
merge grove with eggs/sprouts, one clear merge action, visible reward/restore
feedback, and a blocked state when no merge is available.

### Scope

- Keep the implementation native-first in `src/clean_seed_main.c`.
- Use original names/art direction, not Merge Dragons brand/content/art.
- Prove one small loop: merge three matching objects, update progress, show next
  action or blocked reason.
- Keep visual assets procedural/debug for this slice, with the debt recorded.

### Out Of Scope

- No full clone.
- No store economy, timers, monetization, map campaign, or broad collection.
- No final art/asset pipeline pass.

## Done when

- [x] `gamedesign/projects/dragon-grove/data/core_loop.json` exists and passes
      startup gate.
- [x] Native runtime opens to a Dragon Grove board, not the clean seed screen.
- [x] The board uses internal Y-up grid coordinates.
- [x] A merge-3 action changes board state and HUD feedback.
- [x] A blocked/no-merge state is visible and readable.
- [x] Smallest native build/probe evidence is recorded.
- [x] Task evidence records the subagents used for this implementation.

## Open questions

- None for the first tiny slice; final art/reference parity remains out of scope.

## Log

- 2026-06-21: Started as a small orchestration-pipeline test from the lead's
  request to make a Merge Dragons-like game. Scoped to original `Dragon Grove`
  merge-3 dragon grove prototype.
- 2026-06-21: Recorded explicit visual debug debt for this tiny slice:
  `nt_shape_renderer` is allowed only as temporary debug proof, not product art;
  product pass requires generated runtime art and engine text/font rendering.
- orchestration: used
  objective: build the first original Dragon Grove native merge slice
  allowed files: AGENTS.md, tasks/**, gamedesign/projects/dragon-grove/**, tools/game_context/new_prototype.mjs, tools/game_context/test.mjs, src/clean_seed_main.c, tools/dragon-grove/smoke.py
  expected output: Stage 0 docs, kickoff bug fix, one native merge-ready action, DevAPI smoke, screenshot evidence
  evidence command: node --test tools/game_context/test.mjs; node tools/taskboard/cli.mjs validate; node tools/game_context/iteration_context.mjs; cmake --build --preset native-debug --target game_seed; py -3.12 tools/dragon-grove/smoke.py; node tools/visual_invariant_guard.mjs
  stop condition: keep this to one board/action loop; no product visual pass claim until engine text and generated runtime art replace debug shapes
  independent reviewer: Herschel reviewed the kickoff guard bug; Feynman reviewed the runtime slice plan before implementation
- 2026-06-21: Evidence:
  - `node --test tools/game_context/test.mjs` PASS 7/7.
  - `node tools/taskboard/cli.mjs validate` PASS.
  - `node tools/game_context/iteration_context.mjs` PASS startup gate,
    `ready_for_first_slice`.
  - `cmake --build --preset native-debug --target game_seed` PASS.
  - `py -3.12 tools/dragon-grove/smoke.py` PASS: endpoints,
    `runtime=dragon_grove`, `merge_count=3`, `restored_tiles=3`, level-2
    reward, blocked state, `ui.tree` nodes, screenshot.
  - `node tools/visual_invariant_guard.mjs` PASS.
  - Screenshot: `build/captures/dragon-grove-smoke.png`.
