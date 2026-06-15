---
id: T0060
title: Speed up pipeline_validate full by skipping redundant in-export reruns
status: done
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

- [x] In-export re-run reduced: minimal export self-check ALWAYS runs (export + `skills_eval` + `taskboard validate` inside the export = proves the copy/allowlist is runnable); the full in-export test battery (~11-13 spawns) is now behind `--reexport-tests`, skipped by default.
- [x] Coverage not silently lost: the in-repo suites ran this same invocation; the skip prints "skipped the in-export test battery (suites already ran in-repo); pass --reexport-tests..." and the usage documents it.
- [x] `--dry-run` reflects the real plan deterministically; `pipeline_validate.test.mjs` updated (default-minimal test + a `--reexport-tests` battery test); 7/7 pass.
- [x] Real `--full` verified end-to-end: in-repo suites + cmake + export + minimal export check + "skipped battery" + passed.

## Open questions

- RESOLVED: chose a deterministic flag (`--reexport-tests`) over git-touched-`tools/bootstrap/` gating (git-gating is fragile / tied to commit state). Minimal export check always runs, so no coverage is silently lost.

## Log

- 2026-06-15: Captured from the second simplification/speed iteration (highest speed-per-effort, but --full is now rarely run since quick is the default, and the export re-run is the real "export works" signal -- design the gate carefully).

- 2026-06-15: Reduced --full in-export reruns. Minimal export self-check (export + skill eval + taskboard validate) always runs; full in-export test battery now behind --reexport-tests (default skips ~11-13 redundant spawns). Deterministic flag chosen over git-gating. pipeline_validate.test 7/7; real --full verified end-to-end.
