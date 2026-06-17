---
id: T0009
title: "Profiler coverage guard: exclude idle gaps >1h / auto-roll scope per work session; status should report active-time not raw wall coverage"
status: doing
epic: E001
priority: P2
tags: [pipeline, profiler, telemetry]
created: 2026-06-17
updated: 2026-06-17
---

## What

Improve profiler coverage reporting so long idle gaps do not make normal work
look poorly covered. Coverage should report active work time separately from raw
wall-clock duration and should roll or close scopes across long inactive gaps.

## Done when

- [ ] Profiler status distinguishes active-time coverage from raw wall-clock
      coverage.
- [ ] Idle gaps longer than 1 hour are excluded or reported separately.
- [ ] Tests or a documented fixture cover a long inactive gap.

## Open questions

## Log
- 2026-06-17: 06-16 profile blends >=4 workstreams (engine WASM/CI PR#213, Voxelheim design, Critter cleanup, image-gen); 18/33 'fails' real but mostly engine cmake-wasm/clang-tidy/gh-run-watch, 15 false (rg/Get-Content no-match). Need per-workstream scope tags + drop search-no-match from fail + report active-time.
- 2026-06-17: DONE part: per-session logs implemented in hook_record_fast.c (sessions/<date>__<harness>__<sid8>.jsonl) + session_id/harness/cwd stamp; works for claude (payload session_id) AND codex (CODEX_SESSION_FILE/latest rollout uuid); false-fail fix (search exit1=no-match -> pass); status reads active session by default + --all/--session. 30/30 tests. REMAINING: idle-gap exclusion in coverage guard + .mjs fallback parity.
- 2026-06-17: DONE: status now reports ACTIVE time and excludes idle gaps >1h from coverage. Real 06-17 file: was 'coverage 0.0% + guard BLOCKING'; now 'Active work 2.03h of 2.49h effective (81.6%); 8.46h idle excluded' + guard usable. Root cause was double: records have no duration_ms (merged=0) AND idle counted in wall span. 30/30 tests.
