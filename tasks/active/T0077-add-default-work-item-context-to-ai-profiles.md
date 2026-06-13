---
id: T0077
title: Add default work item context to AI profiles
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, context, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Allow AI profile events to inherit work-item and iteration metadata from
environment variables so agents do not need to repeat `--work-item` and
`--iteration` on every profile wrapper command.

## Done when

- [x] `buildRecord` uses `AI_PROFILE_WORK_ITEM` and `AI_PROFILE_ITERATION` as
      fallbacks when explicit CLI flags are absent.
- [x] Explicit `--work-item` and `--iteration` still override environment
      defaults.
- [x] CLI usage, profiling docs, status guidance, and reflection skill rules
      document the default-context variables.
- [x] Validation passes for syntax, env-default live event, override live
      event, status/review evidence, skill eval, taskboard, diff check, and
      reusable pipeline.

## Open questions

- None.

## Log
- 2026-06-13: Started env-default work item metadata so future profile events can inherit task context without repeating `--work-item` on every command.
- 2026-06-13: Implemented `AI_PROFILE_WORK_ITEM` and `AI_PROFILE_ITERATION` fallbacks in `tools/ai_profile/profile_lib.mjs`; explicit CLI flags still override env defaults.
- 2026-06-13: Updated `run.mjs`, `event.mjs`, `context.mjs`, `closeout.mjs`, `status.mjs`, profiling docs, reflection skill, and skill eval anchors to document env-default metadata.
- 2026-06-13: Env-default evidence: `$env:AI_PROFILE_WORK_ITEM='T0077_ENV'; $env:AI_PROFILE_ITERATION='env-default-test'; node tools/ai_profile/event.mjs --profile tmp/session_profiles/env_defaults_test.jsonl ...` wrote `work_item: T0077_ENV` and `iteration: env-default-test`.
- 2026-06-13: Override evidence: with the same env defaults, `node tools/ai_profile/event.mjs --profile tmp/session_profiles/env_override_test.jsonl ... --work-item T0077_CLI --iteration cli-override` wrote `work_item: T0077_CLI` and `iteration: cli-override`.
- 2026-06-13: Status evidence: `node tools/ai_profile/status.mjs --json-output tmp/session_profiles/session_profile_2026-06-13.status.json` now recommends setting `AI_PROFILE_WORK_ITEM` / `AI_PROFILE_ITERATION` when work-item coverage is low.
- 2026-06-13: Final validation passed: `node --check tools/ai_profile/profile_lib.mjs`; `node --check tools/ai_profile/run.mjs`; `node --check tools/ai_profile/event.mjs`; `node --check tools/ai_profile/context.mjs`; `node --check tools/ai_profile/closeout.mjs`; `node --check tools/ai_profile/status.mjs`; `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node tools/taskboard/cli.mjs validate`; `git diff --check`; `node tools/pipeline_validate.mjs`.
- 2026-06-13: Started env-default work item metadata so future profile events can inherit task context without repeating --work-item on every command.
- 2026-06-13: Moved to review after env-default metadata implementation, env/override evidence, status guidance, skill/taskboard checks, diff check, and final reusable pipeline validation passed.
