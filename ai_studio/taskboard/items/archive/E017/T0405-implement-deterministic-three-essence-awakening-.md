---
id: T0405
title: Implement deterministic three-Essence awakening model and save state
status: done
project: P004
epic: E017
priority: P0
tags: [gameplay, state, tdd]
created: 2026-07-11
updated: 2026-07-13
---

## What

Replace theme/score/RNG state with deterministic focus-pair recipes, discovery,
rewards and persistence for Moon, Bloom and Flame.

## Done when

- [ ] Main focus sets primary Essence; accent focus sets secondary Essence.
- [ ] All six unordered recipes resolve deterministically and order-independently.
- [ ] AWAKEN is blocked only when a focus slot is missing and shows a visual slot hint.
- [ ] State sequence covers Dress, Intro, Charge, Flash, Reveal, Victory and Recipe Card.
- [ ] Discoveries, unlocks, best cards and last outfit survive save/reload.
- [ ] Unit tests prove all pairs, no failure path, no random rank and corrupt-save recovery.

## Open questions

- Exact state schema for storing best authored card metadata without storing image blobs.

## Log
- 2026-07-11: Started TDD domain slice for three Essences, six unordered recipes and deterministic always-win awakening phases.
- 2026-07-13: Closure: waived; reason: prototype closed by lead before acceptance; evidence: lead decision 2026-07-13; committed pause 4697cd445 and .planning/.continue-here.md record incomplete gates
- 2026-07-13: Quality: not-applicable; reason: closure records cancellation and does not claim implementation or acceptance
