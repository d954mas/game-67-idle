---
id: T0374
title: Minimize agent context and profile scoped reads
status: done
project: P001
epic: E015
priority: P1
tags: [context, profiling]
created: 2026-07-10
updated: 2026-07-12
---

## What

Make Taskboard retrieval summary-first and measure scoped reads without logging
content or imposing a runtime hook on every agent action.

## Done when

- [x] Default context returns at most 5 task summaries; detailed content comes
      only from explicit `show`/scoped queries.
- [x] Profiler aggregates every registered public/private mounted store and
      records operation, path/query, bytes, duration, truncation, and result
      count without task contents.
- [x] Profiling is invoked at existing CLI/tool boundaries and does not launch a
      Node hook on the hot path of every model/tool event.
- [x] Before/after evidence reports context bytes and latency for common agent
      routing tasks.

## Open questions

## Log

- 2026-07-10: Split from `T0349` because minimal context is a product contract,
  not a side effect of lifecycle validation.
- 2026-07-10: Execute after T0373 ID-allocation changes and T0349 closure-gate changes to serialize edits to shared Taskboard store/CLI surfaces.
- 2026-07-12: Checkpoint before implementation. Scope is summary-first Taskboard retrieval plus privacy-safe profiling at existing CLI/tool boundaries; preserve unrelated dirty worktree changes.
- 2026-07-12: TDD RED: the two focused default-limit/profiler contract tests failed against the pre-change implementation as expected (0/2). GREEN: `node --test --test-name-pattern "defaults to five|read profiler|unsliced totals|help exits|rejects unrelated" ai_studio/taskboard/tests/taskboard.test.mjs` passed all 6 selected tests.
- 2026-07-12: Paused at lead request after implementation and author verification. Task remains doing and uncommitted. Reported checks: Taskboard 59/59, profiling 27/27, validation 0 problems, Architecture Map 0 issues, docs 11/11, diff check clean. Resume with two independent read-only diff reviews, fix/recheck, evidence+Quality closure, exact staging, and atomic commit.
- 2026-07-12: Verification: Taskboard 59/59; profiling 27/27; focused contract tests 6/6; Taskboard validation 0 problems; Architecture Map 354 mapped / 787 scanned with 0 issues; docs 11/11; profiler `--runs 7` emitted privacy-safe summary/context/show records.
- 2026-07-12: Performance evidence: default context fell from 25 rows / 13,849 UTF-8 bytes to 5 rows / 3,184 bytes (77.0% reduction). Fresh-process latency did not improve, so no latency claim is made; repeatable method and in-process medians are recorded in `ai_studio/core_harness/profiling/taskboard_reads_evidence.md`.
- 2026-07-12: Independent reviews: cycle 1 found 0 HIGH and 3 actionable LOW; UTF-8/truncation coverage, benchmark reproducibility, and RED/GREEN evidence were fixed. Cycle 2 corrected one evidence-count LOW. Cycle 3 converged with 0 HIGH and 0 actionable findings from both reviewers.
- 2026-07-12: Quality: QTECH_001=pass; evidence: Taskboard 59/59, profiling 27/27, focused contracts 6/6, validation/map/docs green, privacy-safe profile evidence, and two independent reviewers converged at 0 actionable
