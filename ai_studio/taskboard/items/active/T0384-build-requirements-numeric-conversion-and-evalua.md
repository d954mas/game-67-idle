---
id: T0384
title: Build requirements normalization and deterministic diagnostics
status: backlog
project: P001
epic: E016
priority: P1
tags: [items, balance, requirements, numeric]
created: 2026-07-10
updated: 2026-07-14
---

## What

Add pure requirements, deterministic normalization, and stable diagnostics after
loader and Snapshot are stable. Sandbox limits stay in T0382 and runtime export
conversion/overflow stays in T0365.

## Done when

- [ ] Pure named requirements have stable severity, structured evidence, and an
      explicit waiver mechanism; v1 runs all requirements and does not trust
      manual dependency declarations.
- [ ] Fractional outputs declare normalization and integer outputs declare
      rounding within the evaluator's supported safe range.
- [ ] Deterministic Studio math has cross-platform golden properties; raw libm
      boundary differences cannot change exported rows silently.
- [ ] Unknown/missing/type/duplicate/reference/cycle/non-finite/storage/level
      failures retain stable source locations and codes.

## Open questions

- Game-specific warning/error thresholds remain game-owned.
- Exact arithmetic beyond IEEE-754 safe integers remains a separate decision.

## Log

- 2026-07-14: Absorbed the reusable severity/evidence/waiver seam from T0370;
  removed the rejected generic economy-policy project.

- 2026-07-10: Expanded after review distinguished double precision from
  cross-platform deterministic export and rejected stale manual dependencies.
