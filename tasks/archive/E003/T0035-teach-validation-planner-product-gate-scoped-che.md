---
id: T0035
title: Teach validation planner product-gate scoped checks
status: done
epic: E003
priority: P1
tags: [validation, product-gate, pipeline]
created: 2026-06-15
updated: 2026-06-15
---

## What

`tools/ai_profile/plan_validation.mjs` does not have a product-gate change
kind. Changes under `tools/product_gate/` therefore lack a narrow planned
check and tend to fall back to broad quick/full pipeline validation.

## Done when

- [x] Planner recognizes `product-gate` / `product_gate` / `gate` changes.
- [x] `--file tools/product_gate/review.mjs` infers the product-gate change
      kind.
- [x] Planner recommends `node --test tools/product_gate/test.mjs` as the
      scoped check.
- [x] Tests cover explicit and file-inferred product-gate planning.
- [x] Planner/taskboard/profiler validation passes.

## Open questions

## Log

- 2026-06-15: Started after adding visual critic/product gate tools and finding
  the validation planner still had no narrow product-gate check.
- 2026-06-15: Added `product-gate` change kind, aliases, file inference for
  `tools/product_gate/`, and scoped `node --test tools/product_gate/test.mjs`
  planning. Validation: `node --test tools/ai_profile/test.mjs`,
  `node tools/ai_profile/plan_validation.mjs --change product-gate --risk medium --json`,
  `node tools/ai_profile/plan_validation.mjs --file tools/product_gate/review.mjs --json`,
  `node --test tools/product_gate/test.mjs`, `git diff --check ...`,
  `node tools/taskboard/cli.mjs validate`, and
  `node tools/ai.mjs status --require-current-scope-usable` passed.
