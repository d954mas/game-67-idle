---
id: T0044
title: Make passive profiling truly passive and prune tmp
status: backlog
epic: E003
priority: P0
tags: [profiling, speed, tooling]
created: 2026-06-15
updated: 2026-06-15
---

## What

Profiling has become a second project. Measured: 172 profile sidecars in
`tmp/`; one source-sheet intake re-profiled 22x (`intake-compact-v5-profile-*`);
on 06-15 70% (113/161) of session records were `validation` and only 13 were
`implementation`. `status.mjs` (973 LOC), `review.mjs` (1212), and the
reflection chain (~1550 LOC) impose ceremony on normal work. AGENTS.md already
says profiling must stay passive and advisory - enforce it: keep only a
lightweight JSONL append for normal work, and gate status/review/reflection/
gap_checkpoint behind an explicit retrospective or AI-workflow request.

## Done when

- [ ] Normal game/tooling work needs no profiler ceremony (no forced `gap_checkpoint`, status, review, or reflection) - only an optional JSONL append.
- [ ] status/review/reflection tools run only when the task is explicitly about AI workflow / profiler / a requested retrospective.
- [ ] "Do not perf-profile the profiler or audit tools as default work" is recorded as an anti-pattern (cite the 22 intake reruns).
- [ ] `tmp/` profile sidecars + `tmp/asset-profiles` are pruned/ignored; no unbounded growth.
- [ ] Docs updated; narrow profile-facade tests covering the change + `node tools/taskboard/cli.mjs validate` pass.

## Open questions

## Log

- 2026-06-15: Created from full pipeline review. Evidence: 172 sidecars, 22 reruns of one intake, 70% of 06-15 records were validation ceremony.
