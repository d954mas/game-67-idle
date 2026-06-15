---
id: T0042
title: Add state codegen tests to validation planner
status: done
epic: E003
priority: P1
tags: [ai-profile, state, validation]
created: 2026-06-15
updated: 2026-06-15
---

## What

Make state-codegen validation planning cover both generated output and the
state codegen unit tests. Also use a `python` command that is runnable by the
validation runner shell in this workspace.

## Done when

- [x] `node tools/ai.mjs validate --file tools/state_codegen/generate_state.py --dry-run`
  includes state codegen and state codegen tests.
- [x] `node tools/ai.mjs validate --file tools/state_codegen/generate_state_test.py --dry-run`
  includes state codegen and state codegen tests.
- [x] State-codegen planner commands use runnable `python ...` commands.
- [x] Planner tests cover the state-codegen route.

## Open questions

- none; this is a narrow validation-planner fix.

## Log

- 2026-06-15: Started after finding state-codegen file routes skipped
  `tools.state_codegen.generate_state_test` and used `py -3.12`, which is less
  reliable under the validation runner shell.
- 2026-06-15: Added `state-codegen-tests` to the validation planner and changed
  the state codegen command to `python tools/state_codegen/generate_state.py`.
- 2026-06-15: Validation passed:
  `node tools/ai.mjs validate --file tools/state_codegen/generate_state.py --dry-run`;
  `node tools/ai.mjs validate --file tools/state_codegen/generate_state_test.py --dry-run`;
  `node --test tools/ai_profile/test.mjs`;
  `node tools/ai.mjs validate --file tools/ai_profile/plan_validation.mjs --file tools/ai_profile/test.mjs --file tasks/STATUS.md --file tasks/active/T0042-add-state-codegen-tests-to-validation-planner.md --risk medium`;
  `node tools/ai.mjs validate --file tools/state_codegen/generate_state.py --risk medium`;
  `node tools/ai.mjs status --require-current-scope-usable`;
  `node tools/taskboard/cli.mjs validate`; `git diff --check -- ...`.
