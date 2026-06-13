---
id: T0104
title: Add profiled AI validation runner
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, validation, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a low-overhead validation runner for AI-development sessions. It should
reuse validation plans, execute concrete checks in tier order, record each
command in the AI profile, and avoid running broad/final checks after earlier
failures unless the agent explicitly asks to continue.

## Done when

- [x] `tools/ai_profile/validation_run.mjs` can build a validation plan from
      change/risk inputs or consume an existing plan JSON.
- [x] The runner executes non-placeholder checks, skips placeholder commands,
      and writes a machine-readable summary.
- [x] Each executed command is appended to the AI profile with validation tier,
      check id, duration, result, command, and exit code.
- [x] By default, later checks are skipped after a failed command; explicit
      `--continue-on-fail` is available.
- [x] Regression tests cover profiled execution, failure short-circuiting, and
      dry-run summaries.
- [x] Profiling docs, reflection skill rules, and skill eval anchors mention
      `validation_run.mjs`.
- [x] Full validation is complete and the work is ready to commit.

## Open questions

None.

## Log

- 2026-06-13: Started `T0104` scope with `tools/ai_profile/start.mjs`.
- 2026-06-13: Added `validation_run.mjs`, tests, docs, skill rule, and
  iteration-log entry.
- 2026-06-13: Early validation passed: `node --check
  tools/ai_profile/validation_run.mjs` and `node --test
  tools/ai_profile/test.mjs`.
- 2026-06-13: Real validation batch passed through
  `node tools/ai_profile/validation_run.mjs --change profiling --change
  pipeline --change skills --risk medium --json-output
  tmp/session_profiles/validation_run_T0104.json`; it executed 8 checks,
  skipped the placeholder JS syntax check, and ran `pipeline_validate.mjs`
  once as the final broad gate.
