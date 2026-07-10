---
id: T0374
title: Minimize agent context and profile scoped reads
status: backlog
project: P001
epic: E015
priority: P1
tags: [context, profiling]
created: 2026-07-10
updated: 2026-07-10
---

## What

Make Taskboard retrieval summary-first and measure scoped reads without logging
content or imposing a runtime hook on every agent action.

## Done when

- [ ] Default context returns at most 5 task summaries; detailed content comes
      only from explicit `show`/scoped queries.
- [ ] Profiler aggregates every registered public/private mounted store and
      records operation, path/query, bytes, duration, truncation, and result
      count without task contents.
- [ ] Profiling is invoked at existing CLI/tool boundaries and does not launch a
      Node hook on the hot path of every model/tool event.
- [ ] Before/after evidence reports context bytes and latency for common agent
      routing tasks.

## Open questions

## Log

- 2026-07-10: Split from `T0349` because minimal context is a product contract,
  not a side effect of lifecycle validation.
