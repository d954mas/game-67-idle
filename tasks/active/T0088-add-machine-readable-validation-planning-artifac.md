---
id: T0088
title: Add machine-readable validation planning artifact
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, validation, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Make validation planning machine-readable enough for future agents/tools to
consume without parsing markdown, reducing repeated broad/final validation
waste.

## Done when

- [x] `tools/ai_profile/plan_validation.mjs` can write a JSON artifact with
      tiered checks, broad/final counts, deferred broad counts, and next action.
- [x] Profiling docs and reflection skill tell agents to use the JSON artifact
      when another tool/agent or later reflection needs the validation decision.
- [x] Regression tests cover medium-risk final checks and low-risk deferred
      broad checks.
- [x] Validation proves the tool, skill eval, taskboard, and portable pipeline.

## Open questions

## Log

- 2026-06-13: Promoted profile follow-up about repeated broad/final validation
  into a focused tooling task. Existing `plan_validation.mjs --json` was useful
  for stdout but not enough as a durable handoff artifact.
- 2026-06-13: Added `--json-output` to `tools/ai_profile/plan_validation.mjs`
  plus `schema_version`, `checks_by_tier`, `final_checks`,
  `broad_final_checks`, `broad_final_count`, `deferred_broad_count`,
  `next_action`, and a broad-gate repetition rule.
- 2026-06-13: Smoke evidence: `node tools/ai_profile/plan_validation.mjs
  --change profiling --change pipeline --risk medium --json-output
  tmp/session_profiles/validation_plan_T0088.json` wrote a JSON artifact with
  `broad_final_count: 1` and `next_action` to run broad/final once at the end
  of the batch.
- 2026-06-13: Updated `AI_PIPELINE_SESSION_PROFILING.md`,
  `chat-session-reflection`, `tools/skills_eval.mjs`, and iteration log so
  future agents use `--json-output` when validation decisions are handed to
  another tool/agent or cited later.
- 2026-06-13: Validation passed: `node --check
  tools/ai_profile/plan_validation.mjs`; `node --check tools/skills_eval.mjs`;
  `node --test tools/ai_profile/test.mjs` passed 21 tests; `node
  tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node
  tools/taskboard/cli.mjs validate`; `git diff --check`; `node
  tools/pipeline_validate.mjs`.
- 2026-06-13: Completed machine-readable validation planning artifact; validation: node --test tools/ai_profile/test.mjs, skills_sync/eval, taskboard validate, git diff --check, node tools/pipeline_validate.mjs.
