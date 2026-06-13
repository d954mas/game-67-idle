---
id: T0064
title: Add AI profile reflection review analyzer
status: review
epic: ""
priority: P1
tags: [pipeline,profiling,reflection,analysis,tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a lightweight profile review analyzer that turns collected JSONL telemetry
into reflection-ready findings without requiring manual inspection of every
record.

The analyzer must stay local, project-agnostic, and cheap to run. It should
support future projects by pointing agents at waste/rework, repeated commands,
failed/blocked records, context hotspots, and missing closeout data.

## Done when

- [x] `review.mjs` reads a profile JSONL and validates record shape.
- [x] It reports priority findings for waste/rework, failures, blockers,
      high-context records, repeated commands, missing context-input details,
      and missing closeout.
- [x] It writes an ignored `.review.md` artifact when `--output` is provided.
- [x] Profiling docs and `chat-session-reflection` require profile review
      before deeper retrospectives when profile data exists.
- [x] Skill eval checks for the review workflow.
- [x] Validation covers syntax, example profile review, live profile review,
      taskboard, skill eval, and portable export.

## Open questions

- Should `review.mjs` eventually emit machine-readable follow-up suggestions
  for task creation, or should suggestions stay markdown-only?

## Log

- 2026-06-13: Added `tools/ai_profile/review.mjs` and integrated it into
  session profiling docs, `chat-session-reflection`, skill eval, and status.
- 2026-06-13: Validation passed:
  `node tools/ai_profile/run.mjs ... -- node --check tools/ai_profile/review.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/review.mjs tools/ai_profile/session_profile_example.jsonl --output tmp/session_profiles/session_profile_example.review.md`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/review.mjs tmp/session_profiles/session_profile_2026-06-13.jsonl --output tmp/session_profiles/session_profile_2026-06-13.review.md`;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_sync.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_eval.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/taskboard/cli.mjs validate`;
  `node tools/ai_profile/run.mjs ... -- node tools/pipeline_validate.mjs`.
