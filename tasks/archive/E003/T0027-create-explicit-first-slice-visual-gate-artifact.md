---
id: T0027
title: Create explicit first-slice visual gate artifact during prototype kickoff
status: done
epic: E003
priority: P1
tags: [game-context, visual-gate, pipeline]
created: 2026-06-15
updated: 2026-06-15
---

## What

`tools/game_context/new_prototype.mjs` currently makes a new prototype look
startup-ready using general status/GDD text, but it does not create a durable
artifact for the first fake-shot/product-read/native screenshot gate. Add a
project review template that agents must fill before broad runtime work.

## Done when

- [x] New prototype kickoff creates
      `gamedesign/projects/<game-id>/reviews/first_slice_visual_gate.md`.
- [x] The artifact has explicit fields/checklist for fake shot or visual
      target, current native screenshot or capture plan, mismatch list,
      product-read gate command/path, stop condition, and expansion decision.
- [x] `STATUS.md`, the project README/GDD, and generated startup context point
      agents to that artifact.
- [x] Tests cover the new artifact and the startup gate remains ready after
      kickoff.
- [x] Game-context/taskboard/profiler validation passes.

## Open questions

## Log

- 2026-06-15: Created because the new-prototype kickoff path names visual gates
  in prose but does not create the durable first-slice visual proof artifact
  needed to prevent code-first expansion.
- 2026-06-15: Done. `tools/game_context/new_prototype.mjs` now creates
  `reviews/first_slice_visual_gate.md`, links it from README/GDD/STATUS, and
  `iteration_context` includes it as an active-project design source.
- 2026-06-15: Evidence: `node --test tools/game_context/test.mjs`, `node
  tools/taskboard/cli.mjs validate`, `node tools/skills_eval.mjs`, `node
  tools/ai.mjs status --require-current-scope-usable`, and profiled `node
  tools/pipeline_validate.mjs`.
