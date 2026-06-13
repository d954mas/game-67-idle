---
id: T0066
title: Add compact AI context digest
status: review
epic: ""
priority: P1
tags: [pipeline, context, taskboard, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a compact current-context digest so agents can resume or start long work
without reading a huge `tasks/STATUS.md` into context by default.

The helper must stay generic for future projects: it should read the normal task
store/status files, print bounded current goal/gate/evidence/blocker/next
sections, show actionable task counts, and tell agents to inspect only specific
linked task/evidence files next.

## Done when

- [x] `node tools/taskboard/cli.mjs context` prints a bounded current-context
      digest.
- [x] The digest warns when `tasks/STATUS.md` is large and caps status text by
      `--status-max-chars`.
- [x] It lists actionable task counts and a limited task list controlled by
      `--tasks-limit`.
- [x] Long-session rules prefer the digest before reading full `STATUS.md`.
- [x] `task-manager`, `chat-session-reflection`, and skill eval include the
      context-diet rule.
- [x] Tests cover the context command.

## Open questions

- Should a future version emit JSON for agents that want machine-readable
  context packets?

## Log

- 2026-06-13: Added `node tools/taskboard/cli.mjs context`, updated minimal
  context rules and skills to prefer the digest before full `STATUS.md` reads.
- 2026-06-13: Validation passed:
  `node tools/ai_profile/run.mjs ... -- node --check tools/taskboard/cli.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/taskboard/cli.mjs context --status-max-chars 4000 --tasks-limit 8`;
  `node tools/ai_profile/run.mjs ... -- node --test tools/taskboard/test.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_sync.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_eval.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/taskboard/cli.mjs validate`;
  `node tools/ai_profile/run.mjs ... -- git diff --check`;
  `node tools/ai_profile/run.mjs ... -- node tools/pipeline_validate.mjs`.
