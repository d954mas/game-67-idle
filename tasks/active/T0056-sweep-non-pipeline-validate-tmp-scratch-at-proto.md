---
id: T0056
title: Sweep non-pipeline-validate tmp scratch at prototype close
status: backlog
epic: E003
priority: P3
tags: [tooling, speed, housekeeping]
created: 2026-06-15
updated: 2026-06-15
---

## What

T0043 added auto-prune for `tmp/pipeline-validate-*` only. Other scratch (closed
prototype renders, generation pipelines, atlas review dirs) still accumulates
unbounded; the post-review sweep reclaimed ~98M of it manually. Add a small,
explicit housekeeping path so it does not require a manual `rm` each time.

## Done when

- [ ] A documented or scripted way to sweep stale non-pipeline-validate `tmp/` scratch (e.g. a `tools/...` housekeeping command, or a "prototype close" checklist step in tasks/README/task-manager).
- [ ] It is explicit/opt-in (does not delete active scratch mid-work) and safe (tmp is gitignored; durable evidence lives under gamedesign/projects/).
- [ ] `node tools/taskboard/cli.mjs validate` ok (+ test if a script is added).

## Open questions

- Scripted prune (age/size based) vs a documented prototype-close checklist step? Lean: a documented step plus an optional `--all-scratch` flag, to avoid deleting in-progress work.

## Log

- 2026-06-15: Captured from the post-implementation review (tmp still grows outside the pipeline-validate prune).
