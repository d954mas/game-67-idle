---
id: T0060
title: Speed up pipeline_validate full by skipping redundant in-export reruns
status: backlog
epic: E003
priority: P2
tags: [validation, speed, tooling]
created: 2026-06-15
updated: 2026-06-15
---

## What

`pipeline_validate.mjs --full` runs the deep suites in-repo (lines ~145-205),
exports the repo, then RE-RUNS the identical suites inside the export
(~208-264). `export_base.mjs` copies test files verbatim, so the in-export
results are byte-identical unless the diff touches `tools/bootstrap/` or its
copy allowlist; the only unique signal is "does the export still self-validate."
This is ~11-13 redundant process spawns -- roughly 40-50% of `--full` wall-clock
in the no-build case.

## Done when

- [ ] The in-export re-run is reduced. Prefer the SAFE design: always keep a
      minimal export self-check (`taskboard validate` + `skills_eval` inside the
      export proves the export is structurally sound) but skip the full duplicate
      test battery unless the diff touches `tools/bootstrap/` (use the
      `git status --porcelain` pattern already in slice_hygiene.mjs), with an
      `--always-reexport` escape hatch.
- [ ] Coverage is not silently lost: document that the in-repo suites already ran
      this invocation; the export check proves the copy/allowlist, not the tests.
- [ ] `--dry-run` still prints the export plan (guard the skip with `!dryRun` so
      `pipeline_validate.test.mjs` stays green); add a test for the new flag/skip.
- [ ] `node --test tools/pipeline_validate.test.mjs` passes; a real `--full` is
      verified end-to-end.

## Open questions

- Gate on git-touched `tools/bootstrap/` vs always run the minimal export check + skip only the duplicated test battery? Lean: the latter (safer; not tied to commit state).

## Log

- 2026-06-15: Captured from the second simplification/speed iteration (highest speed-per-effort, but --full is now rarely run since quick is the default, and the export re-run is the real "export works" signal -- design the gate carefully).
