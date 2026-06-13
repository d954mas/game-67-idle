---
id: T0070
title: Add machine-readable AI profile review output
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, analysis, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add machine-readable output to AI profile review so future tools/agents can
consume findings without parsing markdown.

Markdown remains the human-readable reflection prep artifact. JSON should carry
the same core findings, repeated-command scope, missing context input details,
and suggested actions for automated follow-up.

## Done when

- [x] `review.mjs` accepts `--json-output <path>`.
- [x] JSON output includes schema version, profile metadata, findings,
      waste/rework, failures/blockers, context hotspots, missing context input
      details, repeated commands, repeated commands by scope, repeated
      broad/final commands, time by phase, and suggested actions.
- [x] Markdown output remains compatible with the existing `--output` flow.
- [x] Profiling docs and reflection skill mention the JSON artifact for
      automation.
- [x] Validation covers syntax, example/live markdown+JSON review, skill eval,
      taskboard validation, diff whitespace, and portable pipeline validation.

## Open questions

- Should a future follow-up convert review JSON directly into draft task files,
  or should task creation remain a human/agent decision?

## Log

- 2026-06-13: Added `--json-output` to `tools/ai_profile/review.mjs` while
  keeping markdown output as the default human review format.
- 2026-06-13: Validation passed:
  `node tools/ai_profile/run.mjs ... -- node --check tools/ai_profile/review.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/review.mjs tools/ai_profile/session_profile_example.jsonl --output tmp/session_profiles/session_profile_example.review.md --json-output tmp/session_profiles/session_profile_example.review.json`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/review.mjs tmp/session_profiles/session_profile_2026-06-13.jsonl --output tmp/session_profiles/session_profile_2026-06-13.review.md --json-output tmp/session_profiles/session_profile_2026-06-13.review.json`;
  JSON shape checks for both generated review JSON files;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_sync.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_eval.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/taskboard/cli.mjs validate`;
  `node tools/ai_profile/run.mjs ... -- git diff --check`;
  `node tools/ai_profile/run.mjs ... -- node tools/pipeline_validate.mjs`.
