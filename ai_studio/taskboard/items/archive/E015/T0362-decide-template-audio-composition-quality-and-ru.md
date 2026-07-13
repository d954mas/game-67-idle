---
id: T0362
title: Decide runtime/template performance seams
status: done
project: P001
epic: E015
priority: P1
tags: [template, performance]
created: 2026-07-10
updated: 2026-07-10
---

## What

Close the remaining audit topic that had not been discussed far enough to
authorize implementation: runtime/template performance seams.

## Done when

- [x] The performance topic has current code/benchmark evidence and links to
      overlapping existing work.
- [x] The lead accepts a concrete scoped task or records an evidence-backed
      no-op.

## Open questions

None. Follow-up implementation is tracked by the extracted tasks.

## Log

- 2026-07-10: Audio was decided and extracted to T0393.
- 2026-07-10: Quality closeout was decided and extracted to T0394.
- 2026-07-10: Existing performance ownership was separated: T0357 owns
  toolchain/CMake build phases, T0353 owns Studio CI and advisory timing, T0393
  owns audio measurements, and T0323 owns web load/first-frame smoke.
- 2026-07-10: Inspection found that Runtime Automation's `iterate.py` promised
  build-if-stale but accepted any existing executable, so an agent could collect
  evidence from a stale binary.
- 2026-07-10: Lead chose option 2. T0395 now owns a trustworthy, measured native
  source-edit -> fresh binary -> DevAPI-ready -> semantic-proof loop. A generic
  performance framework and premature budgets were rejected.
- 2026-07-10: Quality: not-applicable; reason: planning-only decision card with
  no product or runtime implementation.
