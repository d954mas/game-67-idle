---
id: T0404
title: Stabilize worn-peel art factory and visual QA gates
status: done
project: P004
epic: E017
priority: P0
tags: [assets, generation, qa]
created: 2026-07-11
updated: 2026-07-13
---

## What

Turn worn-on-body generation plus `body_peel.py` into a repeatable acceptance
pipeline for one locked doll and electric magical editorial anime style.

## Done when

- [ ] In a 10-player comparison against two alternatives, >=7 rate/prefer the master as beautiful and aspirational and <=2 call it stiff, creepy or inappropriately adult.
- [ ] Style bible fixes pose, camera, line weight, shading, materials and Moon/Bloom/Flame grammar.
- [ ] Accepted mini-capsule contains one main-focus and one accent-focus item for each of Moon, Bloom and Flame.
- [ ] Each candidate produces source, peeled layer, checkerboard, body composite and mixed-outfit proof.
- [ ] Automated/manual gates reject face/body drift, skin residue, halos, text, signatures and layer collisions.
- [ ] At least 5 of a 6-item trial capsule pass compressed runtime QA.
- [ ] Legacy isolated/placed garments are excluded from the shipping path.

## Open questions

- Whether landmark drift checking belongs in this game tool or a reusable asset tool after MVP.

## Log
- 2026-07-11: Started locked-body mini-capsule gate. Source-first search returned no compatible registered Moon/Bloom/Flame outfit or runway art; baseline visual audit retained only body_base and rejected legacy fashion art for release.
- 2026-07-13: Closure: waived; reason: prototype closed by lead before acceptance; evidence: lead decision 2026-07-13; committed pause 4697cd445 and .planning/.continue-here.md record incomplete gates
- 2026-07-13: Quality: not-applicable; reason: closure records cancellation and does not claim implementation or acceptance
