---
id: T0079
title: Add AI profile regression tests
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, tests, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add automated regression tests for `tools/ai_profile/` so profiling workflow
behavior is guarded without relying only on live manual command checks.

## Done when

- [x] `node --test tools/ai_profile/test.mjs` covers scope precedence,
      status JSON, closeout bundle output, recovered failure classification,
      and follow-up draft generation.
- [x] `tools/pipeline_validate.mjs` runs the AI profile tests in the source
      repo and in exported portable pipeline bases.
- [x] Task and profiling docs mention the AI profile test command as the
      scoped validation for profiler/tooling changes.
- [x] Validation passes for AI profile tests, taskboard, skill eval, diff
      check, and reusable pipeline.

## Open questions

- None.

## Log
- 2026-06-13: Started AI profile regression tests so scope precedence, status health, closeout bundle, and review/followup behavior are guarded automatically.
- 2026-06-13: Added `tools/ai_profile/test.mjs` covering persistent scope precedence, status JSON, closeout bundle output, recovered failure review JSON, and recovered-failure follow-up drafts.
- 2026-06-13: Wired `node --test tools/ai_profile/test.mjs` into `tools/pipeline_validate.mjs` for both source repo and exported portable pipeline validation.
- 2026-06-13: Updated `AI_PIPELINE_SESSION_PROFILING.md`, `tasks/README.md`, `tools/skills_eval.mjs`, and iteration log to treat `node --test tools/ai_profile/test.mjs` as scoped profiler validation.
- 2026-06-13: Evidence: `node --test tools/ai_profile/test.mjs` passed 4 tests; `node --check tools/pipeline_validate.mjs`; `node --check tools/ai_profile/test.mjs`.
- 2026-06-13: Final evidence: `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node --test tools/taskboard/test.mjs`; `node tools/taskboard/cli.mjs validate`; `git diff --check`; `node tools/pipeline_validate.mjs` passed, including AI profile tests in both the source repo and exported portable pipeline base.
- 2026-06-13: Moved to review after AI profile regression tests, source/export pipeline wiring, skill/taskboard checks, diff check, and final reusable pipeline validation passed.
