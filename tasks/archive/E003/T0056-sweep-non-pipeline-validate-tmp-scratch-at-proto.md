---
id: T0056
title: Sweep non-pipeline-validate tmp scratch at prototype close
status: done
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

- [x] Added `tools/tmp_sweep.mjs`: `--list` (default, reports + deletes nothing), `--all-scratch` (deletes non-validate scratch + old validate dirs), `--keep-validate <n>` (default 3), `--dry-run`, `--root <dir>`. Documented a prototype-close step in tasks/README Context Budget.
- [x] Explicit/opt-in (default is `--list`; deletion needs `--all-scratch`) and safe (only operates inside `tmp/`, which is gitignored; durable evidence lives under gamedesign/projects/).
- [x] `node --test tools/tmp_sweep.test.mjs` 4/4; `node tools/taskboard/cli.mjs validate` ok.

## Open questions

- RESOLVED: documented step + scripted `--all-scratch` flag (lean from the task), default `--list` so it never deletes in-progress work.

## Log

- 2026-06-15: Captured from the post-implementation review (tmp still grows outside the pipeline-validate prune).
- 2026-06-15: Added tools/tmp_sweep.mjs (+ test 4/4) and a tasks/README prototype-close line. `--list` shows ~13M reclaimable across 94 entries on the current tmp; keeps the newest 3 pipeline-validate dirs.
