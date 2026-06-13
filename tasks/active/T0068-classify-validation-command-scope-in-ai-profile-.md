---
id: T0068
title: Classify validation command scope in AI profile review
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, validation, analysis, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Improve AI profile review so repeated validation commands are classified by
scope: `preflight`, `scoped`, `broad/final`, or `unknown`.

The goal is to make retrospectives faster: repeated broad/final gates should be
called out separately from cheap preflight or intentionally repeated scoped
checks.

## Done when

- [x] `review.mjs` classifies repeated commands by scope.
- [x] It reports repeated command counts by scope.
- [x] It has a dedicated repeated broad/final section.
- [x] Suggested actions distinguish broad/final repeats from scoped/preflight
      repeats.
- [x] Profiling docs explain command scope in review output.
- [x] Validation covers syntax, example profile review, live profile review,
      taskboard validation, diff whitespace, and portable pipeline validation.

## Open questions

- Should command scope patterns later be shared with `plan_validation.mjs`
  through a common module?

## Log

- 2026-06-13: Added command scope classification to
  `tools/ai_profile/review.mjs` and split repeated broad/final commands into a
  dedicated review section.
- 2026-06-13: Validation passed:
  `node tools/ai_profile/run.mjs ... -- node --check tools/ai_profile/review.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/review.mjs tools/ai_profile/session_profile_example.jsonl --output tmp/session_profiles/session_profile_example.review.md`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/review.mjs tmp/session_profiles/session_profile_2026-06-13.jsonl --output tmp/session_profiles/session_profile_2026-06-13.review.md`;
  `node tools/ai_profile/run.mjs ... -- node tools/taskboard/cli.mjs validate`;
  `node tools/ai_profile/run.mjs ... -- git diff --check`;
  `node tools/ai_profile/run.mjs ... -- node tools/pipeline_validate.mjs`.
