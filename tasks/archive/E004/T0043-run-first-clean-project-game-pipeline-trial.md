---
id: T0043
title: Run first clean-project game pipeline trial
status: done
epic: E004
priority: P1
tags: [ai-pipeline, trial, gdd]
created: 2026-06-12
updated: 2026-06-12
---

## What

Export a new clean project from the portable pipeline base and initialize it as
a neutral foundation for the user's next game concept. The user will provide
the concept later; this task prepares the repo structure, rules, and status so
the next session can start from a clean slate without old testbed context.

## Done when

- [x] Fresh project is exported outside the dirty repo under `C:\tmp\`.
- [x] Exported project passes `node tools/pipeline_validate.mjs`.
- [x] Exported project has clean starter `AGENTS.md` and `tasks/STATUS.md`
  instructing agents to wait for the user's concept.
- [x] Exported project has no active, archived, or idea tasks by default.
- [x] Exported project has a neutral `gamedesign/` area ready for the future
  concept, without invented game-specific content.
- [x] Preparation friction and next pipeline improvements, if any, are logged
  here.

## Open questions

What game concept should be entered into the prepared project next?

## Log

- 2026-06-12: Started T0043. Scope: fresh exported project and neutral
  foundation for the user's future game concept; old game/runtime files are out
  of scope.
- 2026-06-12: Corrected scope after user clarified that they will invent the
  project. No game concept should be invented by the agent in this task.
- 2026-06-12: Tightened scope after user clarified clean means removing
  unnecessary files, old task history, and old game context. The exported base
  should start with no tasks and wait for the user's idea.
- 2026-06-12: Cleanliness issue found and fixed: exporter briefly had fallback
  from `gamedesign/knowledge` to `gamedesing/knowledge`. User rejected fallback
  and legacy debt, so reusable knowledge was moved to canonical
  `gamedesign/knowledge`, exporter now copies only `gamedesign/knowledge`, and
  `AGENTS.md` no longer treats the old fantasy testbed as current direction.
- 2026-06-12: Evidence passed: current repo `node tools/pipeline_validate.mjs`;
  exported clean base to `C:\tmp\clean-game-base-20260612`; inside the clean
  base, `Test-Path gamedesign/knowledge` -> `True`, `Test-Path gamedesing` ->
  `False`, `node tools/taskboard/cli.mjs list --all` -> no tasks, grep for
  `gamedesing|fromAny|legacy` in starter rules/exporter/status returned no
  matches, and elevated `node tools/pipeline_validate.mjs` passed including
  nested export with `copied: gamedesign/knowledge`.
