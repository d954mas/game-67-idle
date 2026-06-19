---
id: T0009
title: Extract dormant asset/product-gate machinery (gate on active game)
status: done
epic: E001
priority: P3
tags: [pipeline, assets]
created: 2026-06-19
updated: 2026-06-19
---

## What

Heavy asset/cutout/product-gate machinery + ~13 audits is carried, exported, and
tested for a repo with no active game. Extract the proven-but-idle parts into a
dormant tier the seed pulls in only when STATUS has an active concept; gate those
test suites on an active concept (mirror the existing STATUS<->runtime guard). Do
NOT delete — it is proven reusable (ran end-to-end on a real slice). Largest item;
sequence after the small ones.

## Done when

- [x] idle asset machinery is out of the hot seed's default validation when no game is active
- [x] proven reusable tooling still runs / regresses in some form between games; export still works

## Open questions

- RESOLVED: flag-gate, not a separate package. The planner found the heavy asset
  battery is already `--full`-only + file-existence-gated, and the export
  self-check must keep running it to catch reusable-tooling regressions. So the
  only quick-mode idle item was the product-gate suite.

## Log

- 2026-06-19: gate the quick-mode product-gate suite on hasActiveConcept(STATUS)
  (skip in a clean seed); add `--with-assets` (and `--full` / active concept)
  to force it on. The `--full` heavy asset battery + export self-check are
  unchanged, so between-games reusable-tooling regressions are still caught.
  NT_FORCE_CONCEPT env override for deterministic tests. pipeline_validate tests
  14/14, ai.test 11/11. Did NOT edit STATUS.md (lead is editing it for the concept).
