---
id: T0248
title: "Canvas: alpha-dual-generate - full white-then-black chain from ANY art (flat-light check routes, not refuses)"
status: doing
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: Root cause: T0238 packet collapsed gen_dual_plate.sh's white-plate step (lead: 'я же генерирую из любого арта белую и черные версии' - correct). Fix in flight: flat-light check becomes a ROUTER not a refusal - flat bg = element is light plate (1 codex call); else generate white from the art first, then black from white (the legacy chain), plates marked generated:true in meta. Fast-worker running.
