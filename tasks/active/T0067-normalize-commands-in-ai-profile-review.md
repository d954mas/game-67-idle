---
id: T0067
title: Normalize commands in AI profile review
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, analysis, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Improve AI profile review repeated-command analysis so Windows and POSIX path
spellings of the same command are counted together.

This keeps reflection findings from double-counting `tools/foo` and
`tools\foo` variants as separate repeated commands.

## Done when

- [x] `review.mjs` normalizes command strings before counting repeats.
- [x] The review output still preserves variant spellings when they matter for
      debugging.
- [x] Validation covers syntax, example profile review, live profile review,
      taskboard validation, and portable pipeline validation.

## Open questions

- Should future review output classify repeated commands by broad/scoped/narrow
  instead of only reporting counts?

## Log

- 2026-06-13: Added command normalization to `tools/ai_profile/review.mjs` so
  slash variants are counted as the same repeated command.
- 2026-06-13: Validation passed:
  `node tools/ai_profile/run.mjs ... -- node --check tools/ai_profile/review.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/review.mjs tools/ai_profile/session_profile_example.jsonl --output tmp/session_profiles/session_profile_example.review.md`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/review.mjs tmp/session_profiles/session_profile_2026-06-13.jsonl --output tmp/session_profiles/session_profile_2026-06-13.review.md`;
  `node tools/ai_profile/run.mjs ... -- node tools/taskboard/cli.mjs validate`;
  `node tools/ai_profile/run.mjs ... -- git diff --check`;
  `node tools/ai_profile/run.mjs ... -- node tools/pipeline_validate.mjs`.
