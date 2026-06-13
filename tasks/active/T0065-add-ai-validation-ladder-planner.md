---
id: T0065
title: Add AI validation ladder planner
status: review
epic: ""
priority: P1
tags: [pipeline,profiling,validation,tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a lightweight validation planner that helps agents choose the smallest
useful validation set before running commands.

The planner must stay generic for future projects. It should prevent repeated
broad validation by separating preflight, scoped, and final gates for common
change kinds such as docs, skills, taskboard, profiling, portable pipeline,
runtime, assets, state, release, tests, and explicit web work.

## Done when

- [x] A local command prints a validation ladder and does not run commands.
- [x] It accepts explicit change kinds and can infer change kinds from touched
      file paths.
- [x] It marks broad/final checks so agents batch them instead of rerunning
      them after every small edit.
- [x] AI pipeline and session profiling docs tell agents when to use it.
- [x] `chat-session-reflection` and skill eval include planner anchors.
- [x] Validation covers syntax, planner examples, profile review integration,
      skill sync/eval, taskboard validation, portable pipeline validation, and
      diff whitespace.

## Open questions

- Should the planner later read `git diff --name-only` directly, or is
  explicit `--file` input better because it keeps the tool deterministic?

## Log

- 2026-06-13: Added `tools/ai_profile/plan_validation.mjs`, integrated it with
  profile review suggestions, reusable pipeline docs, session profiling docs,
  `chat-session-reflection`, and skill eval.
- 2026-06-13: Validation passed:
  `node tools/ai_profile/run.mjs ... -- node --check tools/ai_profile/plan_validation.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/plan_validation.mjs --change profiling --change skills --risk medium`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/plan_validation.mjs --file tools/ai_profile/plan_validation.mjs --file .codex/skills/chat-session-reflection/SKILL.md --file AI_PIPELINE.md --risk medium --json`;
  `node tools/ai_profile/run.mjs ... -- node --check tools/ai_profile/review.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/review.mjs tools/ai_profile/session_profile_example.jsonl --output tmp/session_profiles/session_profile_example.review.md`;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_sync.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_eval.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/taskboard/cli.mjs validate`;
  `node tools/ai_profile/run.mjs ... -- git diff --check`;
  `node tools/ai_profile/run.mjs ... -- node tools/pipeline_validate.mjs`.
