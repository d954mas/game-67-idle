---
id: T0063
title: Add AI profile closeout summary command
status: review
epic: ""
priority: P1
tags: [pipeline,profiling,telemetry,reflection,tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a one-command closeout helper for profiled AI development sessions so
future agents can end a work cycle with a profile summary artifact without
manual copying or extra ceremony.

This is universal pipeline tooling. It must write raw/summary artifacts under
ignored `tmp/session_profiles/` by default.

## Done when

- [x] `closeout.mjs` appends a final closeout event to the session JSONL.
- [x] `closeout.mjs` writes a `.summary.md` artifact under
      `tmp/session_profiles/` by default.
- [x] The command prints both profile and summary paths for final reporting.
- [x] Profiling docs and reflection skill prefer closeout for normal session
      endings.
- [x] Skill eval checks for the closeout guidance.
- [x] Validation proves syntax, closeout generation, taskboard, skill eval, and
      portable export.

## Open questions

- Should closeout eventually propose follow-up tasks automatically from
  `waste` and `rework` records, or should that stay as human/agent judgment?

## Log

- 2026-06-13: Added `tools/ai_profile/closeout.mjs` and updated profiling docs,
  reflection skill, skill eval, and status/task tracking.
- 2026-06-13: Validation passed:
  `node tools/ai_profile/run.mjs ... -- node --check tools/ai_profile/closeout.mjs`;
  `node tools/ai_profile/closeout.mjs --profile tmp/session_profiles/session_profile_2026-06-13.jsonl --output tmp/session_profiles/session_profile_2026-06-13.closeout.md`;
  `node tools/ai_profile/summarize_session_profile.mjs tmp/session_profiles/session_profile_2026-06-13.jsonl --output tmp/session_profiles/session_profile_2026-06-13.summary.md`.
- 2026-06-13: Final universal validation passed:
  `node tools/ai_profile/run.mjs ... -- node tools/skills_sync.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_eval.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/taskboard/cli.mjs validate`;
  `node tools/ai_profile/run.mjs ... -- node tools/pipeline_validate.mjs`.
