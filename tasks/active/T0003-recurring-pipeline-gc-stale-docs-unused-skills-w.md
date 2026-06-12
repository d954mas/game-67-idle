---
id: T0003
title: "Recurring pipeline GC: stale docs, unused skills, weak examples"
status: idea
epic: ""
priority: P3
tags: [pipeline, maintenance]
created: 2026-06-12
updated: 2026-06-12
---

## What

Recurring garbage-collection pass over the AI pipeline, per
`gamedesign/knowledge/agent_legibility.md`: agents replicate existing
patterns, so stale rules and weak examples compound unless cleaned up
deliberately. Run at milestone boundaries (start/end of a milestone), not on
a calendar schedule. After each pass, reset this task to `backlog` for the
next boundary instead of creating a new one.

Checklist for a pass:

- stale or contradictory statements in `AGENTS.md`, `AI_PIPELINE.md`,
  `tasks/README.md`, skill files (commands that no longer exist, renamed
  presets/paths);
- skills or references that were never triggered since the last pass:
  merge, slim, or drop;
- weak/obsolete examples agents might imitate (old fixtures, dead scenario
  scripts, leftover concept fields in `state/*.schema.json`);
- duplicate guidance across docs: keep one source, link the rest;
- `tools/skills_eval.mjs` anchors still meaningful after skill edits;
- `tmp/` and `build/captures/` debris worth deleting.

## Done when

- [ ] A GC pass has been run at a milestone boundary and findings are
  logged below
- [ ] Each finding is fixed inline or captured as its own task
- [ ] `node tools/pipeline_validate.mjs` passes after the cleanup

## Open questions

- First trigger: the boundary right after the next game concept's first
  milestone?

## Log
