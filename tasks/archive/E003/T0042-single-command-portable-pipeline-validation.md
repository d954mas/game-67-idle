---
id: T0042
title: Single-command portable pipeline validation
status: done
epic: E003
priority: P2
tags: [ai-pipeline, tooling]
created: 2026-06-12
updated: 2026-06-12
---

## What

Add one portable validation entrypoint for reusable pipeline changes. The script
should run the same checks currently listed in `tasks/STATUS.md`: taskboard
list, skill eval, taskboard validation/tests, fresh export, and exported-project
validation/tests.

Out of scope: game/runtime builds and visual QA. This gate is for the reusable
AI pipeline base only.

## Done when

- [x] `node tools/pipeline_validate.mjs` runs current-repo pipeline checks.
- [x] The script exports to a fresh ignored `tmp/` folder and validates the
  exported project from inside that folder.
- [x] The script fails fast with readable command labels.
- [x] `tasks/STATUS.md` points future agents to the single command.
- [x] Current repo passes the new command.

## Open questions

## Log

- 2026-06-12: Started T0042. Scope: one command for portable AI pipeline gate;
  no game/runtime validation.
- 2026-06-12: Added `tools/pipeline_validate.mjs`, included it in portable
  export, and documented it in `AI_PIPELINE.md`, `tasks/README.md`, and
  `tasks/STATUS.md`.
- 2026-06-12: Evidence passed: `node --check tools/pipeline_validate.mjs`;
  `node tools/taskboard/cli.mjs validate`; `node tools/skills_eval.mjs`;
  `node tools/pipeline_validate.mjs`. The full command validated the current
  repo, exported to `tmp/pipeline-validate-2026-06-12T10-39-43-839Z`, then ran
  skill eval, taskboard validation, and taskboard tests inside the export.
