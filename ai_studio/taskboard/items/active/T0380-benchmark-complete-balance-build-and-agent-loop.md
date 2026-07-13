---
id: T0380
title: Benchmark complete Balance build and agent loop
status: backlog
project: P001
epic: E016
priority: P1
tags: [balance, performance, benchmark]
created: 2026-07-10
updated: 2026-07-10
---

## What

After the evaluator, snapshot/export, and CLI exist, benchmark the production
end-to-end build and agent loop and use the results to optimize real bottlenecks.

## Done when

- [ ] Windows and Linux runs cover cold, warm, whole-snapshot no-op reuse,
      candidate one-module incremental, and 1K/100K/1M workloads with one fresh
      evaluator process per actual evaluation.
- [ ] Report startup, parse/load, evaluation, requirements, validation,
      normalization, serialization, C-array/blob export, compiler/linker,
      dedicated Items-pack rebuild, runtime binding, catalog lookup, total wall
      time, cache hit rate, raw blob/pack contribution/HTTP transport estimate,
      transient/steady memory, and value-only edit latency.
- [ ] Agent scenarios report command/tool count, stdout/context bytes, reads,
      latency, and diagnostic quality for edit/validate/inspect/build loops.
- [ ] The selected backend is compared against the `T0363` baseline; unexpected
      regressions are profiled before optimization.
- [ ] Results explicitly ratify or reverse the provisional backend choice; the
      final pinned backend/version is recorded with the full-pipeline evidence.
- [ ] Incremental evaluation is admitted only if instrumented purity and
      full-rebuild parity prove identical results; otherwise full evaluation
      remains the production contract.
- [ ] Stable budgets are proposed from measurements and require a lead decision
      before becoming blocking CI gates; timing stays advisory until then.

## Open questions

## Log

- 2026-07-10: Resolves the dependency cycle found in final transcript audit:
  backend selection precedes production implementation; full-loop proof follows.
