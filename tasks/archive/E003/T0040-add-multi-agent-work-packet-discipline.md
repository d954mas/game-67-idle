---
id: T0040
title: Add multi-agent work-packet discipline
status: done
epic: E003
priority: P1
tags: [ai-pipeline, multi-agent, workflow]
created: 2026-06-12
updated: 2026-06-12
---

## What

Add a reusable rule for when and how agents should split work into bounded
sub-agent work packets. This should prevent late, vague, duplicated, or
conflicting delegation during long-running sessions.

## Done when

- [x] `AI_PIPELINE.md` defines multi-agent/work-packet discipline.
- [x] The rule explains when to delegate and when not to.
- [x] The rule requires narrow ownership, expected artifact/evidence, and
  integration review.
- [x] `tasks/STATUS.md` mentions the new discipline without duplicating it.
- [x] Taskboard validation passes.

## Open questions

None.

## Log

- 2026-06-12: Added `Multi-agent work packets` to `AI_PIPELINE.md`, added a
  short `STATUS.md` index note, and validated with
  `node tools/taskboard/cli.mjs validate`.
- 2026-06-12: Closed multi-agent work-packet discipline; AI_PIPELINE.md and STATUS.md updated; taskboard validate passed.
