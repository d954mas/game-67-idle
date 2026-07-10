---
id: T0373
title: Make Taskboard readable ID allocation collision safe
status: backlog
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

- [ ] Allocation uses a short filesystem lock, atomic counter update, exclusive
      target creation, bounded retry, and documented stale-lock recovery.
- [ ] Concurrent process tests prove unique IDs and valid counter state after
      contention and injected failure.
- [ ] Existing IDs and human-facing CLI output remain compatible.
- [ ] Lock scope covers allocation only and does not serialize unrelated
      Taskboard reads or edits.

## Open questions

## Log

- 2026-07-10: Split from `T0349` to keep concurrency correctness independently
  testable.
