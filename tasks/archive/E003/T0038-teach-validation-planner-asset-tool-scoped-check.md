---
id: T0038
title: Teach validation planner asset-tool scoped checks
status: done
epic: E003
priority: P1
tags: [ai-profile, assets, validation]
created: 2026-06-15
updated: 2026-06-15
---

## What

Teach the validation planner to distinguish reusable asset tooling from runtime
asset/content changes. Edits under `tools/assets/` should select the asset
pipeline test suites directly instead of only returning native/runtime
placeholder checks.

## Done when

- [x] `plan_validation.mjs --change asset-tools` recommends the reusable
  asset tool JS/Python tests.
- [x] `plan_validation.mjs --file tools/assets/validate_art_job.mjs` infers
  `asset-tools` and does not require native runtime placeholder checks.
- [x] Tests cover explicit and file-inferred asset-tool validation planning.
- [x] Process/status docs mention the scoped planner route.

## Open questions

- none; this is a validation-planner routing fix.

## Log

- 2026-06-15: Started after finding `tools/assets/validate_art_job.mjs`
  was inferred as generic `assets`, which selected native/runtime placeholders
  instead of the existing asset pipeline tests.
- 2026-06-15: Added `asset-tools` change kind and scoped JS/Python asset
  pipeline checks to `tools/ai_profile/plan_validation.mjs`; `tools/assets/...`
  file inference no longer selects generic runtime asset placeholders.
- 2026-06-15: First profiled validation found `py -3.12 -m unittest ...`
  failed under the shell runner. Replaced planner Python commands with
  `python -m unittest ...` and taught profile status/review to recover failed
  validation checks by later passing `validation_check_id` even when the
  command text changes.
- 2026-06-15: Validation passed:
  `node tools/ai_profile/plan_validation.mjs --change asset-tools --risk medium --json`;
  `node tools/ai_profile/plan_validation.mjs --file tools/assets/validate_art_job.mjs --json`;
  `node --test tools/ai_profile/test.mjs`;
  `node tools/ai.mjs validate --change profiling --change docs --change asset-tools --risk medium`;
  `node tools/ai.mjs status --require-current-scope-usable`;
  `node tools/taskboard/cli.mjs validate`; `git diff --check -- ...`.
