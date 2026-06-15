---
id: T0047
title: Retire over-built validation planner machinery
status: done
epic: E003
priority: P1
tags: [validation, speed, tooling, subtraction]
created: 2026-06-15
updated: 2026-06-15
---

## What

Eight consecutive tasks (T0035-T0042) built a "validation planner" that infers
scoped checks from touched files, plus per-tier re-runs - `validation_run.mjs`
was the single most-recorded tool on 06-15 (86 entries). This is machinery built
instead of using a simple fixed default. Audit what the planner actually saves
versus its cost, then collapse it to the minimum (or remove it) so selecting
checks is cheap and obvious, not a subsystem. Example of the subtraction the
lead asked for.

## Done when

- [x] Documented audit: the planner's file-to-check inference duplicated the fixed suite set `pipeline_validate.mjs` already runs as the quick default; placeholders (`native-scenario`, `asset-pack`, `release-smoke`) were skipped no-ops; only caller was `ai.mjs validate`; `validation_run.mjs` mostly generated its own profiler churn (86 records/06-15). No hard coupling.
- [x] Planner removed (full remove chosen): no preflight/scoped/final cascade. `ai.mjs validate` is now a thin alias to `node tools/pipeline_validate.mjs` (passes `--full`/`--dry-run`).
- [x] Dead tools removed: `plan_validation.mjs`, `validation_run.mjs`, `check_touched_js.mjs` deleted (~714 LOC + 469 test lines) without breaking quick/full validate (T0043).
- [x] Tests pass: ai_profile/test 84->68, ai.test 14/14, full quick `pipeline_validate.mjs` green, skills_eval ok, taskboard validate ok.

## Open questions

- RESOLVED: full removal (not minimal keep). The inference table was duplicate routing around the same fixed suites; nothing hard-depended on it.

## Log

- 2026-06-15: Created from full pipeline review. T0035-T0042 added validation-planner machinery; usage data shows heavy re-run churn, not game progress.
- 2026-06-15: Audited then fully removed the planner. Deleted plan_validation.mjs + validation_run.mjs + check_touched_js.mjs. `ai.mjs validate` -> thin alias to pipeline_validate.mjs (quick; --full for the heavy gate). Removed 16 planner/runner tests from ai_profile/test.mjs (kept "status and review recover…", which is self-contained). Trimmed AI_PIPELINE.md validation-ladder block to the single pipeline_validate story. Fixed a stale `plan_validation` reference in STATUS.md Validation Policy. Supersedes the validation-planner parts of T0035-T0042. Verified: quick gate fully green.
