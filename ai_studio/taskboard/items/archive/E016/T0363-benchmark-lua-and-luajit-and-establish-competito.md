---
id: T0363
title: Provisionally select Lua or LuaJIT with a neutral benchmark harness
status: done
project: P001
epic: E016
priority: P0
tags: [lua, luajit, benchmark, research]
created: 2026-07-10
updated: 2026-07-14
quality: {"notApplicable":{"reason":"planning and routing cleanup only"}}
---

## What

Build a small backend-neutral representative Items Lua harness and use it for a
provisional evaluator choice before production implementation. Full-pipeline performance and the
competitor workflow study are owned separately by `T0380` and `T0379`.

## Done when

- [ ] Pin a current supported PUC Lua and a specific LuaJIT revision as the two
      candidates; neither is the default before results exist.
- [ ] Windows and Linux representative runs cover startup, module load/parse,
      deterministic full-process formula evaluation/normalization, and
      1K/100K/1M values using the currency/fixed-sword/levelled-sword contract.
- [ ] Report evaluator wall time, startup share, peak memory, output size, and
      normalized determinism without depending on unfinished production CLI/UI.
- [ ] LuaJIT is measured with the relevant JIT modes; both candidates produce
      the same normalized snapshot/runtime values or deviations are explained.
- [ ] One fresh isolated evaluator process is used per representative run;
      backend/version is part of the fingerprint and raw libm exponentiation is
      excluded from the deterministic export fixture.
- [ ] A checked-in report records the evidence-backed provisional backend/version
      choice and the assumptions `T0380` must revalidate before final ratification.

## Open questions

- Exact product performance budgets are a separate decision after baseline
  data; this selection harness must not invent them retroactively.

## Log

- 2026-07-10: Speed of generation is a product requirement. Selection by
  reputation or microbenchmark alone is not acceptable.
- 2026-07-14: Closure: waived; reason: superseded during full Taskboard grooming without claiming new implementation; evidence: minimal backend selection merged into T0382 and final measurement into T0380
- 2026-07-14: Quality: not-applicable; reason: planning and routing cleanup only
