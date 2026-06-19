---
id: T0009
title: Extract dormant asset/product-gate machinery (gate on active game)
status: backlog
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

- [ ] idle asset machinery is out of the hot seed's default validation when no game is active
- [ ] proven reusable tooling still runs / regresses in some form between games; export still works

## Open questions

- Exact dormant boundary: separate package vs flag-gated test suites. Decide before moving files.

## Log
