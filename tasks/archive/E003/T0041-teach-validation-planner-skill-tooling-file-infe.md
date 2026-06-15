---
id: T0041
title: Teach validation planner skill tooling file inference
status: done
epic: E003
priority: P1
tags: [ai-profile, skills, validation]
created: 2026-06-15
updated: 2026-06-15
---

## What

Teach file-based validation planning that reusable skill tooling changes under
`tools/skills_eval.mjs` and `tools/skills_sync.mjs` are skill/process changes.
Without this, edits to the skill eval/sync tools fall back to docs-only checks
and can skip the very checks that protect agent behavior.

## Done when

- [x] `node tools/ai.mjs validate --file tools/skills_eval.mjs --dry-run`
  selects `skills-sync` and `skills-eval`.
- [x] `node tools/ai.mjs validate --file tools/skills_sync.mjs --dry-run`
  selects `skills-sync` and `skills-eval`.
- [x] Planner tests cover skill tooling file inference.
- [x] Status/process docs mention file-based validation for skill tooling edits.

## Open questions

- none; this is a narrow validation-planner inference fix.

## Log

- 2026-06-15: Started after finding `tools/skills_eval.mjs` and
  `tools/skills_sync.mjs` inferred `docs` only in file-based validation.
- 2026-06-15: Added file inference for `tools/skills_eval.mjs` and
  `tools/skills_sync.mjs`, plus planner regression coverage.
- 2026-06-15: Validation passed:
  `node tools/ai.mjs validate --file tools/skills_eval.mjs --dry-run`;
  `node tools/ai.mjs validate --file tools/skills_sync.mjs --dry-run`;
  `node --test tools/ai_profile/test.mjs`;
  `node tools/ai.mjs validate --file tools/ai_profile/plan_validation.mjs --file tools/ai_profile/test.mjs --file tasks/STATUS.md --file tasks/active/T0041-teach-validation-planner-skill-tooling-file-infe.md --risk medium`;
  `node tools/ai.mjs validate --file tools/skills_eval.mjs --risk medium`;
  `node tools/ai.mjs status --require-current-scope-usable`;
  `node tools/taskboard/cli.mjs validate`; `git diff --check -- ...`.
