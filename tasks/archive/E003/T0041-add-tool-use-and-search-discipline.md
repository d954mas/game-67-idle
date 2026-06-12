---
id: T0041
title: Add tool-use and search discipline
status: done
epic: E003
priority: P1
tags: [ai-pipeline, tools, search, validation]
created: 2026-06-12
updated: 2026-06-12
---

## What

Add reusable rules for how agents should search, inspect files, and choose
validation commands without loading stale history or running broad expensive
checks too early.

## Done when

- [x] `tasks/README.md` defines task/search hygiene for active vs archive
  context.
- [x] `AI_PIPELINE.md` defines general tool-use and validation discipline.
- [x] `tasks/STATUS.md` mentions the new discipline without duplicating it.
- [x] Taskboard validation passes.

## Open questions

None.

## Log

- 2026-06-12: Added `Search Hygiene` to `tasks/README.md`, added `Tool and
  validation discipline` to `AI_PIPELINE.md`, updated `STATUS.md`, and
  validated with `node tools/taskboard/cli.mjs validate`.
- 2026-06-12: Closed tool-use/search discipline; tasks/README.md, AI_PIPELINE.md, and STATUS.md updated; taskboard validate passed.
