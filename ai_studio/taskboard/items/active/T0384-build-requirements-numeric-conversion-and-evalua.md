---
id: T0384
title: Build requirements deterministic numeric conversion and evaluator limits
status: backlog
project: P001
epic: E016
priority: P1
tags: [items, balance, requirements, numeric]
created: 2026-07-10
updated: 2026-07-10
---

## What

Add pure requirements, deterministic math/normalization, checked runtime numeric
conversion, and evaluator resource limits after loader and snapshot are stable.

## Done when

- [ ] Pure named requirements have static severity and structured evidence; v1
      runs all requirements and does not trust manual dependency declarations.
- [ ] Fractional outputs declare normalization; integer exports declare rounding,
      remain exact within the supported safe range, and convert with checked
      C-domain/overflow diagnostics.
- [ ] Deterministic Studio math has cross-platform golden properties; raw libm
      boundary differences cannot change exported rows silently.
- [ ] Unknown/missing/type/duplicate/reference/cycle/non-finite/storage/level
      failures retain stable source locations and codes.
- [ ] CPU, memory, instruction, recursion, output-row/byte, and wall-time budgets
      stop runaway scripts with isolated repeatable fixtures.

## Open questions

- Domain warning/error ranges and waivers remain T0370.
- Exact arithmetic beyond IEEE-754 safe integers remains a separate decision.

## Log

- 2026-07-10: Expanded after review distinguished double precision from
  cross-platform deterministic export and rejected stale manual dependencies.
