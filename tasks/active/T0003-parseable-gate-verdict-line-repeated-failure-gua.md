---
id: T0003
title: Parseable gate verdict line + repeated_failure_guard keys on it
status: backlog
epic: E001
priority: P2
tags: [pipeline, gates]
created: 2026-06-19
updated: 2026-06-19
---

## What

Define a fixed one-line gate verdict format `[GATE-ID]: PASS|CONCERNS|FAIL` for
the four gates, and have `repeated_failure_guard.mjs` key its "same major reason
twice" detection on gate-id + FAIL token instead of parsing prose. Also count
total (not only consecutive) occurrences per signature to catch interleaved
loops. Borrowed: Claude Code Game Studios `[GATE]:` verdict line.

## Done when

- [ ] quality-validation.md documents the verdict-line format
- [ ] repeated_failure_guard clusters failures by parsed gate-id + FAIL, total occurrences
- [ ] tools/product_gate/test.mjs covers the parse + interleaved-loop detection

## Open questions

## Log
