---
id: T0040
title: Teach validation planner pipeline validator file inference
status: done
epic: E003
priority: P1
tags: [ai-profile, pipeline, validation]
created: 2026-06-15
updated: 2026-06-15
---

## What

Teach file-based validation planning that `tools/pipeline_validate.mjs` and its
tests are reusable pipeline changes. After T0039, agents can validate from
touched files, but the pipeline validator itself still falls back to docs-only
checks and can skip `node tools/pipeline_validate.mjs`.

## Done when

- [x] `node tools/ai.mjs validate --file tools/pipeline_validate.mjs --dry-run`
  selects `pipeline-quick`.
- [x] Planner tests cover pipeline validator file inference.
- [x] Status/process docs mention file-based validation for pipeline validator
  edits.

## Open questions

- none; this is a narrow validation-planner inference fix.

## Log

- 2026-06-15: Started after finding
  `node tools/ai.mjs validate --file tools/pipeline_validate.mjs --dry-run`
  inferred `docs` only.
- 2026-06-15: Added file inference for `tools/pipeline_validate.mjs` and
  `tools/pipeline_validate.test.mjs`, plus planner regression coverage.
- 2026-06-15: Validation passed:
  `node tools/ai.mjs validate --file tools/pipeline_validate.mjs --dry-run`;
  `node tools/ai_profile/plan_validation.mjs --file tools/pipeline_validate.mjs --json`;
  `node --test tools/ai_profile/test.mjs`;
  `node tools/ai.mjs validate --file tools/ai_profile/plan_validation.mjs --file tools/ai_profile/test.mjs --file tasks/STATUS.md --file tasks/active/T0040-teach-validation-planner-pipeline-validator-file.md --risk medium`;
  `node tools/ai.mjs validate --file tools/pipeline_validate.mjs --risk medium`;
  `node tools/ai.mjs status --require-current-scope-usable`;
  `node tools/taskboard/cli.mjs validate`; `git diff --check -- ...`.
