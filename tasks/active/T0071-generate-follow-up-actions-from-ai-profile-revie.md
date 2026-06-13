---
id: T0071
title: Generate follow-up actions from AI profile review JSON
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a safe follow-up generator that reads `review.mjs --json-output` artifacts
and produces draft action items for the next pipeline/reflection pass.

The generator should not create task files by default. It should produce
reviewable markdown/JSON suggestions so agents can promote only still-relevant
items after checking current tasks.

## Done when

- [x] A local command reads review JSON and prints draft follow-up actions.
- [x] It can write markdown and JSON follow-up artifacts under `tmp/`.
- [x] Suggestions cover repeated broad/final validation, missing context input
      details, failed/blocked records, waste/rework, repeated scoped/preflight
      commands, and clean-profile baseline cases.
- [x] Profiling docs and reflection skill describe the follow-up draft flow.
- [x] Skill eval includes the follow-up tool anchor.
- [x] Validation covers syntax, example/live follow-up generation, JSON shape,
      skill sync/eval, taskboard validation, diff whitespace, and portable
      pipeline validation.

## Open questions

- Should a future `--create-task` mode exist, or should promotion to taskboard
  always stay a separate agent decision?

## Log

- 2026-06-13: Added `tools/ai_profile/followups.mjs` as a draft generator from
  review JSON. It writes reviewable follow-up markdown/JSON and does not create
  task files.
- 2026-06-13: Validation passed:
  `node tools/ai_profile/run.mjs ... -- node --check tools/ai_profile/followups.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/followups.mjs tmp/session_profiles/session_profile_example.review.json --output tmp/session_profiles/session_profile_example.followups.md --json-output tmp/session_profiles/session_profile_example.followups.json`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/followups.mjs tmp/session_profiles/session_profile_2026-06-13.review.json --output tmp/session_profiles/session_profile_2026-06-13.followups.md --json-output tmp/session_profiles/session_profile_2026-06-13.followups.json`;
  JSON shape checks for both generated follow-up JSON files;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_sync.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_eval.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/taskboard/cli.mjs validate`;
  `node tools/ai_profile/run.mjs ... -- git diff --check`;
  `node tools/ai_profile/run.mjs ... -- node tools/pipeline_validate.mjs`.
