---
id: T0373
title: Make Taskboard readable ID allocation collision safe
status: done
project: P001
epic: E015
priority: P1
tags: [taskboard, concurrency]
created: 2026-07-10
updated: 2026-07-10
---

## What

Keep readable `T####` IDs while making concurrent task creation deterministic
and collision safe across agent processes.

## Done when

- [x] Allocation uses a short filesystem lock, atomic counter update, exclusive
      target creation, bounded retry, and documented stale-lock recovery.
- [x] Concurrent process tests prove unique IDs and valid counter state after
      contention and injected failure.
- [x] Existing IDs and human-facing CLI output remain compatible.
- [x] Lock scope covers allocation only and does not serialize unrelated
      Taskboard reads or edits.

## Open questions

## Log

- 2026-07-10: Split from `T0349` to keep concurrency correctness independently
  testable.
- 2026-07-10: Checkpoint: current allocator reads .counters.json and writes it non-atomically in nextId; createProject/createEpic/createTask then write targets without an allocation lock or exclusive create. Existing monotonic single-process test passes, but no concurrent-process or failure-recovery proof exists. Starting TDD scope only in Taskboard store/tests/docs.
- 2026-07-11: RED evidence: three new allocator tests failed 3/3 on the old implementation; 24 concurrent creators produced only 3 unique IDs, failure recovery was absent, and a stale lock was not reclaimed.
- 2026-07-11: Implementation: store-wide candidate-directory lock publication with PID/token ownership, token-safe release, dead-PID stale quarantine, absolute wall-clock timeout, atomic same-directory counter replacement, exclusive target creation, bounded target retry, and documented manual recovery. Reads and edits remain lock-free.
- 2026-07-11: Verification: mixed 24-process P/E/T contention preserves unique readable IDs and all shared counter keys; injected target failure preserves E/P/T high-water marks; stale/live lock, successor-token ABA, read/edit isolation, archive monotonicity, and CLI compatibility tests pass. Full `node --test ai_studio/taskboard/tests/taskboard.test.mjs` 35/35; Taskboard validate pass; scoped diff check pass.
- 2026-07-11: Independent review cycle 1 found 1 HIGH / 7 actionable across ownership ABA, Windows retry, live-lock proof, counter preservation, and worker cleanup. Cycle 2 found 0 HIGH / 2 actionable for wall-clock timeout and failure cleanup. Cycle 3 converged independently at 0 HIGH / 0 actionable for architecture/correctness/ownership and tests/process/performance/context cost.
- 2026-07-11: Quality: QTECH_001=pass; evidence: RED reproduction, 35/35 GREEN suite, multi-process Windows contention proof, store validation, scoped diff check, and two clean independent final reviews.
- 2026-07-10: Closed after three review-fix-verify cycles converged at 0 HIGH / 0 actionable; 35/35 Taskboard tests, validation, and scoped diff check pass.
