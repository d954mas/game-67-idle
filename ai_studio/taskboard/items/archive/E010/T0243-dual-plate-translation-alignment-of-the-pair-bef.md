---
id: T0243
title: "dual-plate: translation alignment of the pair before gate/extract (rescue 'align' verdicts)"
status: done
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
- 2026-07-03: Landed 29390012: align_pair (+-8px, gate's own metric, coarse-to-fine ~1.3-2.8s) wired into canvas tool + CLI. Honest finding: on all 5 real wings pairs best shift = (0,0) - the 0.08 align-verdict inconsistency is NOT integer translation (sub-pixel/scale/redraw noise). Mechanism verified on synthetic known-shift pairs; rescues genuinely drifted pairs going forward.
