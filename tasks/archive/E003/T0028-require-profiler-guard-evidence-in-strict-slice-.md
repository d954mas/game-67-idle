---
id: T0028
title: Require profiler guard evidence in strict slice hygiene
status: done
epic: E003
priority: P1
tags: [profiling, product-gate, pipeline]
created: 2026-06-15
updated: 2026-06-15
---

## What

`tools/product_gate/slice_hygiene.mjs --strict` checks build/probe/product
gate/screenshot evidence, but not profiler guard evidence. Add an explicit
profiler guard evidence slot so prototype handoff/commit hygiene can require
`node tools/ai.mjs status --require-current-scope-usable` or an equivalent
artifact.

## Done when

- [x] `slice_hygiene --strict` fails when profiler guard evidence is omitted.
- [x] Passing profiler guard evidence is represented in JSON/Markdown reports.
- [x] Failed/stale profiler guard evidence fails strict hygiene unless called
      out as a known red gate.
- [x] Product-gate tests cover missing/pass/fail profiler evidence.
- [x] Product-gate/taskboard/profiler validation passes.

## Open questions

## Log

- 2026-06-15: Created after finding that strict slice hygiene could pass with
  build/probe/product/screenshot evidence while profiler current-scope evidence
  was absent.
- 2026-06-15: Added `--profile-guard` strict hygiene evidence and docs/skill
  guidance. Validation: `node --test tools/product_gate/test.mjs`,
  `node tools/taskboard/cli.mjs validate`, `node tools/skills_eval.mjs`,
  `node tools/ai.mjs status --require-current-scope-usable` (current scope
  usable), and profiled `node tools/pipeline_validate.mjs` via
  `node tools/ai.mjs run` passed.
