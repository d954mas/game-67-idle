---
id: T0030
title: Add strict visual critique rubric to product gate
status: done
epic: E003
priority: P1
tags: [visual, product-gate, pipeline]
created: 2026-06-15
updated: 2026-06-15
---

## What

`tools/product_gate/review.mjs` currently records player-read answers, but it
does not force a harsh visual/UI critique. A pass can be written without
structured evidence for readability, UI controls, art quality, action direction,
or audience fit.

Add a strict visual critique rubric to the product gate so visually important
prototype slices cannot hand-wave the exact failure modes that hurt the fishing
iteration.

## Done when

- [x] `--visual-strict` requires scores for the full visual rubric.
- [x] A strict visual pass fails if any rubric score is below the pass
      threshold or if a blocker/major visual issue is recorded.
- [x] A strict visual fail records structured visual issues in JSON and
      Markdown.
- [x] Product-gate tests cover missing scores, low-score pass rejection, and
      fail report output.
- [x] Visual skill/docs point beautiful/casual/UI prototype gates at
      `--visual-strict`.
- [x] Product-gate/taskboard/profiler validation passes.

## Open questions

## Log

- 2026-06-15: Started after finding that the product gate checked player-read
  answers but not a structured harsh visual critique.
- 2026-06-15: Added `--visual-strict`, six `--visual-score` axes, and
  structured `--visual-issue` output to `tools/product_gate/review.mjs`;
  updated visual skill/process docs. Validation: `node --check
  tools/product_gate/review.mjs`, `node --test tools/product_gate/test.mjs`,
  `node tools/skills_eval.mjs`, profiled quick `node tools/pipeline_validate.mjs`,
  `git diff --check ...`, `node tools/taskboard/cli.mjs validate`, and
  `node tools/ai.mjs status --require-current-scope-usable` passed.
