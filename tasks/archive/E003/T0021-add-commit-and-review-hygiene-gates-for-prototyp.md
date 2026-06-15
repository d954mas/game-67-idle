---
id: T0021
title: Add commit and review hygiene gates for prototype slices
status: done
epic: E003
priority: P1
tags: [git, review, validation, workflow]
created: 2026-06-15
updated: 2026-06-15
---

## What

Prevent future prototype snapshots from becoming hard-to-review 80-file mixed
commits. The fishing commit mixed GDD, art, runtime, generated assets, tools,
audits, and stale/fail evidence. Future slices should either be split by phase
or explicitly marked as an end-of-experiment snapshot.

## Done when

- [x] Workflow docs define a pre-commit checklist for prototype slices:
      build/probe, product gate, taskboard validate, diff stat, screenshot
      evidence, and known red gates.
- [x] The checklist warns when a normal slice exceeds the agreed broad-change
      threshold, currently 30 files, unless the lead requested a snapshot.
- [x] Push/remote policy is checked before promising push.
- [x] Stale fail audits are either refreshed, archived as historical evidence,
      or explicitly called out in final/review notes.

## Open questions

- Answered: both. `tools/product_gate/slice_hygiene.mjs` warns by default and
  enforces the threshold in `--strict`; `--snapshot` is the explicit exception
  for lead-approved end-of-experiment snapshots.

## Log

- 2026-06-15: Created from fishing review finding: final commit was too broad
  and push constraints were discovered after the user asked for push.
- 2026-06-15: Started profiler scope with
  `node tools/ai.mjs start T0021 prototype-review-hygiene`.
- 2026-06-15: Added `tools/product_gate/slice_hygiene.mjs` and tests. The tool
  checks changed-file count, 30-file threshold, explicit snapshots,
  build/probe/product-gate/screenshot evidence, known red gates, changed fail
  audit artifacts, and push target readiness before a push promise.
- 2026-06-15: Codified the checklist in `AI_PIPELINE.md`, `tasks/README.md`,
  `tools/README.md`, `game-feature-iteration`, and the iteration cycle
  playbook.
- 2026-06-15: Validation passed:
  `node --test tools/product_gate/test.mjs`,
  `node tools/skills_eval.mjs`,
  `node tools/taskboard/cli.mjs validate`, and
  `node tools/pipeline_validate.mjs`.
- 2026-06-15: Ran current dirty-worktree hygiene snapshot:
  `tmp/slice-hygiene/T0021-current-snapshot.md`. Verdict was `WARN` because
  the current cleanup batch is 62 files, explicitly treated as a pipeline
  snapshot; push target was configured (`master` -> `origin/master`), and the
  closed fishing product-gate fail was called out as historical evidence.
