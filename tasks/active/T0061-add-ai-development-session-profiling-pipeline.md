---
id: T0061
title: Add AI development session profiling pipeline
status: review
epic: ""
priority: P1
tags: [pipeline,profiling,telemetry,tools,skills]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a reusable AI-development session profiling pipeline so future long
sessions can measure time, tool use, context spend, rework, waste, evidence,
and blockers instead of relying only on qualitative retrospectives.

This is pipeline/tooling work. Raw session telemetry should stay in ignored
scratch paths by default; only reusable rules/tools and compact lessons belong
in git.

## Done when

- [x] Reusable profiling rules define what to log, where to store it, and how
      to separate committed infrastructure from scratch telemetry.
- [x] A machine-readable JSONL event format exists for session profiling.
- [x] A summarizer validates a profile and reports duration by phase/category,
      tool counts, context inputs, waste/rework, blockers, and evidence.
- [x] `chat-session-reflection` uses profile artifacts when available and
      clearly marks after-the-fact thread extraction as partial.
- [x] Portable export includes the profiling docs/tools for future projects.
- [x] Validation covers the example profile, skills, taskboard, and syntax.

## Open questions

- Should the next long run write a live `tmp/session_profiles/*.jsonl` profile
  automatically at every major phase, or should the agent only log explicit
  checkpoint events to reduce overhead?

## Log

- 2026-06-13: Added `AI_PIPELINE_SESSION_PROFILING.md`,
  `tools/ai_profile/session_profile_example.jsonl`, and
  `tools/ai_profile/summarize_session_profile.mjs`.
- 2026-06-13: Moved the recovered one-off telemetry extract out of git-visible
  root into `tmp/session_profiles/session_telemetry_extract_2026-06-13.md`.
- 2026-06-13: Updated `AI_PIPELINE.md` and `chat-session-reflection` so raw
  session telemetry stays in `tmp/session_profiles/` by default, while reusable
  profiling rules/tools are committed.
- 2026-06-13: Updated `tools/bootstrap/export_base.mjs` so future exported AI
  bases include session profiling docs/tools.
- 2026-06-13: Validation passed:
  `node --check tools/ai_profile/summarize_session_profile.mjs`;
  `node tools/ai_profile/summarize_session_profile.mjs tools/ai_profile/session_profile_example.jsonl`;
  `node tools/taskboard/cli.mjs validate`;
  `node tools/skills_sync.mjs`;
  `node tools/skills_eval.mjs`;
  `git diff --check`;
  trailing whitespace scan;
  `node tools/pipeline_validate.mjs`.
