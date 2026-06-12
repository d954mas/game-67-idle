---
id: T0014
title: "Decide: fix gamedesing typo or freeze as convention"
status: done
epic: E003
priority: P3
tags: [cleanup]
created: 2026-06-11
updated: 2026-06-12
---

## What

Decide and encode the policy for the misspelled `gamedesing/` folder so future
agents do not waste time re-opening the same question or accidentally export
the typo into new projects.

## Done when

- [x] Current repo policy is explicit: existing `gamedesing/` is a legacy
  testbed path and is not renamed as part of this cleanup.
- [x] Portable-base export creates the corrected `gamedesign/knowledge/` path
  for new projects.
- [x] Reusable pipeline docs refer to `gamedesign/` for future projects while
  allowing this repo's legacy path.
- [x] Current repo and a fresh export pass pipeline/task validation.

## Open questions

## Log

- 2026-06-12: Started T0014. Decision: freeze existing `gamedesing/` in this
  testbed to avoid broad churn, but stop exporting the typo to future projects.
- 2026-06-12: Updated `AGENTS.md` to mark `gamedesing/` as a legacy testbed
  path, updated `AI_PIPELINE.md` and export templates to use `gamedesign/`, and
  changed portable export to copy `gamedesing/knowledge` into
  `gamedesign/knowledge`.
- 2026-06-12: Evidence passed: `node tools/taskboard/cli.mjs validate`;
  `node tools/skills_eval.mjs`; `node --test tools/taskboard/test.mjs`;
  `node tools/bootstrap/export_base.mjs --target tmp/export-gamedesign-path-test-20260612`;
  in the exported project, `node tools/taskboard/cli.mjs validate`,
  `node tools/skills_eval.mjs`, `Test-Path gamedesign/knowledge` -> `True`,
  and `Test-Path gamedesing/knowledge` -> `False`.
