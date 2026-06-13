---
id: T0072
title: Add work item metadata to AI session profiles
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, context, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add optional work-item and iteration metadata to AI session profile events, and
teach the profile review to segment long daily profiles by those fields.

## Done when

- [x] `run.mjs`, `event.mjs`, `context.mjs`, and `closeout.mjs` accept
      `--work-item` and `--iteration` without breaking existing profiles.
- [x] `review.mjs` reports work items, iterations, missing work-item coverage,
      and repeated broad/final commands by work item in markdown and JSON.
- [x] Profiling docs and reflection skill rules explain when to use
      `--work-item`/`--iteration`.
- [x] Validation passes for syntax, skill eval, taskboard validation, and the
      reusable pipeline gate.

## Open questions

- None.

## Log
- 2026-06-13: Started work-item/iteration profile metadata so long daily AI profiles can be segmented by task instead of treating all repeated validation as one pile.
- 2026-06-13: Implemented optional `work_item` and `iteration` fields in profile records, usage docs for `run.mjs`/`event.mjs`/`context.mjs`/`closeout.mjs`, and review markdown/JSON summaries for work items, iterations, missing coverage, and repeated broad/final commands by work item.
- 2026-06-13: Evidence: `node --check tools/ai_profile/profile_lib.mjs`; `node --check tools/ai_profile/run.mjs`; `node --check tools/ai_profile/event.mjs`; `node --check tools/ai_profile/context.mjs`; `node --check tools/ai_profile/closeout.mjs`; `node --check tools/ai_profile/review.mjs`; `node tools/ai_profile/context.mjs --phase context --intent "Measure profiling metadata docs" --work-item T0072 --iteration profile-metadata --path AI_PIPELINE_SESSION_PROFILING.md --reason "T0072 validation"`; `node tools/ai_profile/review.mjs tmp/session_profiles/session_profile_2026-06-13.jsonl --output tmp/session_profiles/session_profile_2026-06-13.review.md --json-output tmp/session_profiles/session_profile_2026-06-13.review.json`; JSON check confirmed `work_items`, `iterations`, `repeated_broad_final_by_work_item`, and `missing_work_item_metadata`.
- 2026-06-13: Validation passed: `node tools/skills_sync.mjs`; `node tools/taskboard/cli.mjs validate`; `node --test tools/taskboard/test.mjs`; `node tools/skills_eval.mjs`; `git diff --check`; `node tools/pipeline_validate.mjs`.
- 2026-06-13: Moved to review after implementation, review JSON proof, skill/taskboard checks, diff check, and final reusable pipeline validation passed.
