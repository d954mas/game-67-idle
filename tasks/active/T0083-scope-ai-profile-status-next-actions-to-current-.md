---
id: T0083
title: Scope AI profile status next actions to current iteration
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Update `tools/ai_profile/status.mjs` so it reports whole-profile health and
current-scope health separately. When persistent scope has an `updated_at`
timestamp, `next_action` should prioritize records from the current scope
window instead of repeatedly advising fixes for old records from previous
iterations.

## Done when

- [x] `status.mjs` JSON includes current-scope record count, missing work-item
      count, and missing context-input count based on `scope.updated_at`.
- [x] `next_action` uses current-scope missing context inputs when current
      scope is valid, while still reporting whole-profile totals.
- [x] AI profile tests cover stale historical missing context inputs that do
      not affect the current-scope next action.
- [x] Profiling docs and reflection skill mention current-scope status versus
      whole-profile history.
- [x] Validation passes for AI profile tests, taskboard, skill eval, diff
      check, and reusable pipeline.

## Open questions

- None.

## Log
- 2026-06-13: Started after `context_command.mjs` added measured command
  context, but live `status.mjs` still recommended fixing old missing context
  inputs from earlier profile history.
- 2026-06-13: Added `current_scope` health to `status.mjs`, derived from
  `scope.updated_at`, with current-scope record count, missing work-item count,
  and missing context-input count.
- 2026-06-13: Updated `next_action` to use current-scope missing context when
  scope is valid, while keeping whole-profile totals visible for retrospective
  history.
- 2026-06-13: Added AI profile tests for stale historical missing context
  inputs and for current-scope missing context still being flagged.
- 2026-06-13: Updated profiling docs, reflection skill, skill eval anchors,
  and iteration log to explain current-scope health versus whole-profile
  history.
- 2026-06-13: Evidence: `node --test tools/ai_profile/test.mjs` passed 10
  tests; `node --check tools/ai_profile/status.mjs`; `node --check
  tools/skills_eval.mjs`; live `node tools/ai_profile/status.mjs` showed
  `Current scope records: 1 (0 missing context inputs, 0 missing work items)`
  while whole-profile `Missing context inputs: 4` remained historical; `node
  tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node
  tools/taskboard/cli.mjs validate`; `node tools/pipeline_validate.mjs`
  passed, including exported AI profile tests.
- 2026-06-13: Moved to review after current-scope status health, docs/skill updates, ai profile tests, live status check, and reusable pipeline validation passed.
