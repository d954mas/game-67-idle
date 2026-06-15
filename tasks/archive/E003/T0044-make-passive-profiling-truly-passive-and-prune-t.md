---
id: T0044
title: Make passive profiling truly passive and prune tmp
status: done
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

- [x] Normal game/tooling work needs no profiler ceremony - `reflect`'s gap checkpoint is now opt-in (`--gap-checkpoint`), and the slice-hygiene profiler guard is advisory (warning, not blocking).
- [x] status/review/reflection are documented as optional/on-demand for AI-workflow/profiler/retrospective tasks (SESSION_PROFILING.md + tasks/README.md Intent-To-Scope + slice-hygiene evidence bullet).
- [x] "Do not perf-profile the profiler or audit tools as default work" recorded as an anti-pattern, citing the 22 intake reruns / 70% validation day (SESSION_PROFILING.md "Do Not Profile The Profiler").
- [x] `tmp/` profile sidecars + `tmp/asset-profiles` pruned; `tmp/` is gitignored. Behavioral guard against regrowth = the anti-pattern rule + the pipeline_validate prune (T0043).
- [x] Docs updated; tests pass: ai.test 14/14, ai_profile/test 84/84, product_gate/test 20/20, skills_eval 9/9, taskboard validate ok.

## Open questions

## Log

- 2026-06-15: Created from full pipeline review. Evidence: 172 sidecars, 22 reruns of one intake, 70% of 06-15 records were validation ceremony.
- 2026-06-15: Made profiling non-blocking. (1) `ai.mjs reflect` gap checkpoint inverted to opt-in `--gap-checkpoint` (`--no-gap-checkpoint` still accepted, no-op); updated ai.test. (2) `slice_hygiene.mjs` profiler guard demoted from blocking problem to advisory warning for missing/stale/inconclusive guards - **supersedes T0028's strict requirement** per the lead's passive-profiling direction; updated product_gate/test (missing -> status 0 advisory; stale -> status 0 advisory). (3) Docs: SESSION_PROFILING.md ("no forced gate", "Do Not Profile The Profiler" anti-pattern), tasks/README.md Intent-To-Scope (profiler optional) + slice-hygiene evidence bullet (guard optional). (4) Cleaned tmp profiling artifacts: 362M -> 121M (asset-profiles, session_profiles, profile-*, intake reruns, profiled exports removed). NOTE for lead: this relaxes T0028; pre-commit slice hygiene no longer blocks on profiler evidence.
