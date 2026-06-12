---
id: T0018
title: Add activation+output evals for key skills (task-manager, game-runtime-automation)
status: done
epic: E003
priority: P2
tags: [ai-pipeline, qa]
created: 2026-06-11
updated: 2026-06-12
---

## What

Add a small portable regression check for key reusable skills so future pipeline
edits do not accidentally remove activation triggers, source-of-truth links, or
required evidence/output anchors.

## Done when

- [x] `task-manager` has static activation/output checks for task capture,
  refinement, source-of-truth usage, and evidence logging.
- [x] `game-runtime-automation` has static activation/output checks for DevAPI
  discovery, runtime driving, capture, logs, build-policy, and command metadata.
- [x] The eval exits nonzero when required anchors are missing.
- [x] The eval is included in portable-base export.
- [x] Current repo and a fresh export both pass the eval and task validation.

## Open questions

## Log

- 2026-06-12: Started T0018. Scope: portable static skill eval for
  `task-manager` and `game-runtime-automation`; no game/runtime product work.
- 2026-06-12: Added `tools/skills_eval.mjs`, wired it into portable export, and
  added skill/process evidence guidance to `tasks/README.md` and
  `AI_PIPELINE.md`. The initial eval failed when `task-manager` did not name
  `tasks/STATUS.md`, proving nonzero failure behavior; the skill was updated
  and synced.
- 2026-06-12: Evidence passed: `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`; `node --test tools/taskboard/test.mjs`;
  `node tools/bootstrap/export_base.mjs --target tmp/export-skills-eval-test-20260612`;
  in the exported project, `node tools/skills_eval.mjs` and
  `node tools/taskboard/cli.mjs validate`.
