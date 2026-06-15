---
id: T0043
title: Default reusable pipeline validation to quick and gate full
status: doing
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

- [ ] `node tools/pipeline_validate.mjs` with no flag runs the quick path; `--full` is opt-in.
- [ ] `node tools/ai.mjs validate` defaults to quick unless touched files include export/runtime/release paths.
- [ ] `tools/README.md`, `AGENTS.md`, and the evidence-gate guidance in `tasks/README.md` say quick is the default; full is reserved for portable-base/export/runtime/release gates.
- [ ] `tmp/pipeline-validate-*` is auto-pruned to the last 3 (or written outside the repo); no unbounded growth.
- [ ] Relevant `node --test tools/*` suites + `node tools/taskboard/cli.mjs validate` pass.

## Open questions

## Log

- 2026-06-15: Created from full pipeline review. Evidence: 126 full runs / 0 quick, 32 steps each, 227MB of `tmp/` export copies.
