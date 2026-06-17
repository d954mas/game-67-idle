---
id: T0011
title: Clean Voxelheim source-of-truth drift in AGENTS and status
status: done
epic: E001
priority: P1
tags: [pipeline, source-of-truth, voxelheim, cleanup]
created: 2026-06-17
updated: 2026-06-17
---

## What

Clean the duplicated and contradictory Voxelheim status/rules after the rescue
iteration. `AGENTS.md` currently contains both the new Frost Keep Rebuilder
concept and older Game Seed / old Voxelheim statements in the same sections,
which lets future agents pick the wrong source of truth.

## Done when

- [x] `AGENTS.md` has one current Voxelheim concept statement and one current
      runtime surface statement.
- [x] Obsolete "Game Seed current surface" and old generic Voxelheim direction
      text is removed or clearly moved to historical context.
- [x] `tasks/STATUS.md` points to the current post-iteration state without
      implying the UI hotfix is a final product pass.
- [x] `node tools/taskboard/cli.mjs validate` passes.

## Open questions

- Should the finished Voxelheim prototype be archived as a closed iteration, or
  kept active for another polish/fun-review cycle?

## Log
- 2026-06-17 created from process retrospective: the review found contradictory
  current-state rules in `AGENTS.md` and status drift around pass/fail evidence.
- 2026-06-17 cleaned `AGENTS.md` to one active process state and one stopped
  Voxelheim runtime surface; `tasks/STATUS.md` already points to process review
  and avoids final-product claims. Evidence: `node tools/taskboard/cli.mjs
  validate` passed.
