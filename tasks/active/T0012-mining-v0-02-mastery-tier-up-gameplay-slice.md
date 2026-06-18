---
id: T0012
title: Mining v0.02 mastery tier-up gameplay slice
status: idea
epic: E001
priority: P2
tags: [mine-cards, gameplay, mining, mastery, gated]
created: 2026-06-17
updated: 2026-06-18
---

## What

Prepare the first gameplay expansion after T0001 acceptance: make Node Mastery
produce one visible tier-up moment that gives a small Mining benefit and a new
reason to keep repeating a node.

This is gated behind T0001 lead acceptance. Until then it stays `idea` and must
not change `src/`, runtime state, balance JSON, or the playable screen.

Current design inputs:

- Core loop: `gamedesign/projects/mine-cards/core_loop.md`
- Parameters: `gamedesign/projects/mine-cards/parameters.md`
- Machine-readable parameters: `gamedesign/projects/mine-cards/data/parameters.json`
- Systems registry: `gamedesign/projects/mine-cards/data/systems_registry.json`
- Slice packet:
  `gamedesign/projects/mine-cards/design/mining_v002_mastery_slice_packet.md`
- Machine-readable implementation contract:
  `gamedesign/projects/mine-cards/data/mining_v002_mastery_contract.json`

Scope:

- one visible Node Mastery tier-up for `surface_stone` and/or `copper_vein`;
- one small effect already defined by v0.01 parameters: tier 1 gives `-3%`
  interval on that node;
- UI feedback for mastery progress, tier-up, and before/after interval;
- live-state/product gate updates for mastery near-tier, tier-up, and post-tier
  active mining.

Out of scope:

- new skills;
- Smithing, equipment stats, combat, food, cards, offline progress, or
  automation;
- adding new resources or changing first-upgrade economy;
- modifying T0001 before lead acceptance;
- editing `external/neotolis-engine`.

## Done when

- [ ] T0001 is accepted or the lead explicitly chooses mechanics before visual
      baseline acceptance.
- [x] The mastery slice packet is promoted from prep to implementation scope
      with any changed numbers recorded in design data.
- [ ] Runtime state stores node mastery XP/tier per node and preserves it
      through save/load or the current state path.
- [ ] Mining ticks increment mastery XP and resolve tier changes in the same
      reward order as `core_loop.md`.
- [ ] UI shows current tier/progress, tier-up feedback, and interval before/after.
- [ ] Product/live-state gate covers near-tier, tier-up, and post-tier mining.
- [ ] Native screenshot or short capture proves the tier-up moment without
      regressing T0001 readability.

## Open questions

- Default proof node is `surface_stone` only. `copper_vein` remains optional if
  it falls out naturally without extra UI complexity.
- Tier-up feedback is non-blocking and can appear as stage or node-row callout,
  plus reward log. It must not cover progress or next goal.
- First mastery benefit applies from the next mining cycle after tier-up, not
  retroactively to the completed cycle.
- Before implementation starts, choose whether mastery state is schema-backed
  persistent state or game-local runtime state for the narrow proof.

## Log

- 2026-06-18: Created as a gated gameplay prep task while T0001 remains in
  review. Added `design/mining_v002_mastery_slice_packet.md` as the first
  post-acceptance mechanics candidate because it deepens the existing Mining
  loop without adding a new system.
- 2026-06-18: Added machine-readable implementation contract and validator:
  `gamedesign/projects/mine-cards/data/mining_v002_mastery_contract.json` and
  `tools/design/validate_mastery_slice_contract.py`. The contract locks the
  first proof to `surface_stone`, 10 mastery XP, tier 1 `-3%` interval, and
  `3.00s -> 2.91s` after tier-up. T0012 remains a gated `idea` and must not
  change runtime/state before T0001 acceptance or explicit lead override.
