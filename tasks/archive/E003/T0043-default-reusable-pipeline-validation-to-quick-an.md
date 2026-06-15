---
id: T0043
title: Default reusable pipeline validation to quick and gate full
status: done
epic: E003
priority: P0
tags: [validation, speed, tooling]
created: 2026-06-15
updated: 2026-06-15
---

## What

`pipeline_validate.mjs` / `ai.mjs validate` run `--full` in practice on almost
every change. Measured: 126 `--full` runs in ~34h (06-14/06-15), 0 quick runs;
each full = ~32 spawned steps (portable export copy + every test suite run
twice + python unittests + `cmake`), leaving ~227MB of export copies in `tmp/`.
The quick/full split already exists (T0029) but the cheap path was never the
default. Flip the default to quick; require full only for export/runtime/release
changes or via an explicit flag. Add `tmp/` auto-prune.

This is the single biggest wall-clock win (problem B).

## Done when

- [x] `node tools/pipeline_validate.mjs` with no flag runs the quick path; `--full` is opt-in. (Already true; verified `mode: quick`.)
- [x] `tools/README.md`, the evidence-gate guidance in `tasks/README.md`, and the STATUS validation policy say quick is the default; full is reserved for portable-base/export/runtime/release gates. (Verified existing; strengthened tools/README with a "full is heavy" note + prune flags.)
- [x] `tmp/pipeline-validate-*` is auto-pruned to the last 3 (or written outside the repo); no unbounded growth. (Added prune; proven 126 -> 3, tmp 362M -> 143M.)
- [x] Relevant `node --test tools/*` suites + `node tools/taskboard/cli.mjs validate` pass. (pipeline_validate tests 6/6; quick run passed including taskboard validate.)
- [~] `node tools/ai.mjs validate` quick-default: this is the planned-validation (`validation_run.mjs`) path, not the 126-dir driver. Delegated to T0047 (retire/simplify the validation planner). The actual full-run driver (`pipeline_validate.mjs`) is now quick-default + pruned.

## Open questions

## Log

- 2026-06-15: Created from full pipeline review. Evidence: 126 full-run dirs / 362MB `tmp/`, ~32 steps each.
- 2026-06-15: Found `pipeline_validate.mjs` already defaults to quick (line 21/34); docs already said quick-default. Real gaps were unbounded `tmp/` growth and habitual `--full`. Added `pruneOldExports` (default keep 3) run on every non-dry-run invocation, plus `--keep-exports <n>` / `--no-prune` flags, and a "full is heavy / reserved" note in usage + tools/README. Real quick run pruned 126 -> 3 dirs (tmp 362M -> 143M) and passed. Tests: `node --test tools/pipeline_validate.test.mjs` 6/6. `ai validate` planner scope moved to T0047.
