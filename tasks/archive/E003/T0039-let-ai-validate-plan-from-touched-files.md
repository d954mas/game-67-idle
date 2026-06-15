---
id: T0039
title: Let ai validate plan from touched files
status: done
epic: E003
priority: P1
tags: [ai-profile, validation, pipeline]
created: 2026-06-15
updated: 2026-06-15
---

## What

Allow `node tools/ai.mjs validate` to plan validation directly from touched
files. `plan_validation.mjs` already supports `--file`, but the facade requires
`--change` or `--plan`, forcing agents to manually translate changed paths into
change kinds before running a profiled validation batch.

## Done when

- [x] `node tools/ai.mjs validate --file <path> --dry-run` is accepted without
  `--change`.
- [x] `tools/ai.mjs` file inference selects the AI facade test path.
- [x] Facade usage/docs mention `--file <path>`.
- [x] Tests cover file-only validation planning through `tools/ai.mjs`.
- [x] Status/process docs point agents to file-based validation planning.

## Open questions

- none; this is a facade ergonomics fix.

## Log

- 2026-06-15: Started after finding agents can ask `plan_validation.mjs` to
  infer checks from files, but cannot use the main profiled facade with
  `--file` alone.
- 2026-06-15: Updated `node tools/ai.mjs validate` to accept `--file` without
  requiring `--change`, and added facade coverage for file-only validation.
- 2026-06-15: Taught validation planning that `tools/ai.mjs` /
  `tools/ai.test.mjs` are profiling/facade changes and should select
  `node --test tools/ai.test.mjs`.
- 2026-06-15: Validation passed:
  `node tools/ai.mjs validate --file tools/game_context/new_prototype.mjs --dry-run`;
  `node --test tools/ai.test.mjs`; `node --test tools/ai_profile/test.mjs`;
  `node tools/ai_profile/plan_validation.mjs --file tools/ai.mjs --json`;
  `node tools/ai.mjs validate --file tools/ai.mjs --file tools/ai.test.mjs --file tools/ai_profile/plan_validation.mjs --file AI_PIPELINE.md --file tasks/STATUS.md --risk medium`;
  `node tools/ai.mjs status --require-current-scope-usable`;
  `node tools/taskboard/cli.mjs validate`; `git diff --check -- ...`.
