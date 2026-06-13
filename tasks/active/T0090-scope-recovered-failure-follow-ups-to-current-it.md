---
id: T0090
title: Scope recovered failure follow-ups to current iteration
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Extend current-scope profile review/follow-up filtering to recovered failed
records, so old failures that already recovered are captured as retrospective
history instead of staying as current draft tasks.

## Done when

- [x] `review.mjs --json-output` includes current-scope recovered and
      unresolved failed records.
- [x] `followups.mjs` suppresses historical-only recovered failure suggestions
      when the current scope has no recovered failures.
- [x] Current-scope recovered failures still produce a follow-up suggestion.
- [x] Regression tests cover both suppression and preservation.
- [x] Profiling docs and reflection skill mention suppressed historical
      recovered failures as retrospective notes, not current tasks.

## Open questions

## Log

- 2026-06-13: After `T0089`, live followups dropped from five suggestions to
  one, but the remaining recovered-failure suggestion was also historical-only.
- 2026-06-13: Added current-scope recovered/unresolved failure summaries to
  `review.mjs`; updated `followups.mjs` to suppress historical-only
  `recovered_failed_records` when the current scope has none.
- 2026-06-13: Live smoke: refreshed `session_profile_2026-06-13` followups now
  reports `Suppressed historical-only findings: repeated_broad_final_commands,
  missing_context_inputs, missing_work_item_metadata, low_profile_coverage,
  recovered_failed_records` and only emits the P3 clean-profile baseline draft.
- 2026-06-13: Validation passed: `node --check tools/ai_profile/review.mjs`;
  `node --check tools/ai_profile/followups.mjs`; `node --check
  tools/skills_eval.mjs`; `node --test tools/ai_profile/test.mjs` passed 25
  tests; `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node
  tools/taskboard/cli.mjs validate`; `git diff --check`; `node
  tools/pipeline_validate.mjs`.
- 2026-06-13: Completed current-scope filtering for recovered failure follow-ups; validation: node --test tools/ai_profile/test.mjs, skills_sync/eval, taskboard validate, git diff --check, node tools/pipeline_validate.mjs.
