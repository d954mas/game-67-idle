---
id: T0089
title: Scope AI profile review follow-ups to current iteration
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Make profile review/follow-up artifacts distinguish whole-profile historical
findings from current-scope issues, so old missing metadata or validation waste
does not keep generating urgent follow-up drafts after the current iteration is
clean.

## Done when

- [x] `review.mjs --json-output` includes a `current_scope` summary when a
      persistent profile scope exists.
- [x] `followups.mjs` suppresses historical-only missing context/work-item,
      low-coverage, and repeated broad/final suggestions when current scope is
      clean.
- [x] Markdown output names suppressed historical-only findings.
- [x] Regression tests cover historical-noise suppression and current-scope
      issue preservation.
- [x] Profiling docs and reflection skill explain how to use current-scope
      review/follow-up output.

## Open questions

## Log

- 2026-06-13: Current `status.mjs` reported no urgent profiling action for
  `T0088`, but refreshed `review/followups` still promoted historical
  whole-profile issues as P1 actions. This creates stale reflection noise for
  the next iteration.
- 2026-06-13: Added `current_scope` summary to `review.mjs --json-output` and
  review markdown: scope name, since timestamp, records, missing context/work
  item counts, repeated broad/final commands, and scoped wall-clock coverage.
- 2026-06-13: Updated `followups.mjs` so current-scope-clean profiles suppress
  historical-only repeated broad/final, missing context, missing work-item, and
  low-coverage suggestions. Markdown and JSON now expose
  `suppressed_historical_findings`.
- 2026-06-13: Live smoke: rerunning review/followups for
  `session_profile_2026-06-13` with scope `T0089/current-scope-review-followups`
  reduced draft suggestions from five to one and listed suppressed historical
  findings: `repeated_broad_final_commands`, `missing_context_inputs`,
  `missing_work_item_metadata`, and `low_profile_coverage`.
- 2026-06-13: Validation passed: `node --check tools/ai_profile/review.mjs`;
  `node --check tools/ai_profile/followups.mjs`; `node --check
  tools/skills_eval.mjs`; `node --test tools/ai_profile/test.mjs` passed 23
  tests; `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node
  tools/taskboard/cli.mjs validate`; `git diff --check`; `node
  tools/pipeline_validate.mjs`.
- 2026-06-13: Completed current-scope review/follow-up filtering; validation: node --test tools/ai_profile/test.mjs, skills_sync/eval, taskboard validate, git diff --check, node tools/pipeline_validate.mjs.
