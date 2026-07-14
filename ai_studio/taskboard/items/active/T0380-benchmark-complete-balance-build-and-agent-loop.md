---
id: T0380
title: Benchmark complete Balance build and agent loop
status: backlog
project: P001
epic: E016
priority: P2
tags: [balance, performance, benchmark]
created: 2026-07-10
updated: 2026-07-14
---

## What

After the evaluator, snapshot/export, and CLI exist, benchmark the production
end-to-end build and agent loop and use the results to optimize real bottlenecks.

## Done when

- [ ] Representative cold, warm, no-op, one-edit, build, and runtime-bind flows
      expose their main wall-time and memory costs.
- [ ] Agent scenarios report command/tool count, stdout/context bytes, reads,
      latency, and diagnostic quality for edit/validate/inspect/build loops.
- [ ] Results explicitly ratify or reverse the provisional backend choice; the
      final pinned backend/version is recorded with the full-pipeline evidence.
- [ ] Real bottlenecks are profiled before optimization; stress sizes and extra
      cross-platform matrices run only when they can change a release decision.
- [ ] Proposed budgets remain advisory until explicitly accepted as gates.

## Open questions

## Log

- 2026-07-14: Absorbed final backend comparison from T0363 and removed the
  mandatory every-size/every-metric benchmark matrix.

- 2026-07-10: Resolves the dependency cycle found in final transcript audit:
  backend selection precedes production implementation; full-loop proof follows.
