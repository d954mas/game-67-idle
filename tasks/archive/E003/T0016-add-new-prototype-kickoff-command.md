---
id: T0016
title: Add new prototype kickoff command
status: done
epic: E003
priority: P1
tags: [pipeline, tooling]
created: 2026-06-15
updated: 2026-06-15
---

## What

Add a small native-pipeline kickoff command for a new game idea so agents do
not hand-create the first project wiki, first actionable task, status sections,
and startup-gate evidence.

## Done when

- [x] A `tools/game_context/new_prototype.mjs` command creates the project wiki
  skeleton, first task, status handoff, and startup-gate JSON/markdown for a
  supplied game id/title.
- [x] The command is idempotent enough to fail before overwriting existing
  project/task artifacts unless `--force` is explicitly passed.
- [x] Tests cover a clean kickoff and existing-project refusal.
- [x] Pipeline docs mention the command as the preferred Stage 0 entry point.

## Open questions

None; keep it lightweight and local, not a full generator.

## Log
- 2026-06-15: Added `tools/game_context/new_prototype.mjs` plus `--root`
  support in `iteration_context.mjs`, docs in `AI_PIPELINE.md`,
  `tasks/README.md`, and `primary-gdd-pipeline`. Validation:
  `node --test tools/game_context/test.mjs`, `node tools/taskboard/cli.mjs
  validate`, and `node tools/skills_eval.mjs` passed.
- 2026-06-15: Closed after kickoff CLI, startup gate tests, taskboard validation, and skills eval passed.
