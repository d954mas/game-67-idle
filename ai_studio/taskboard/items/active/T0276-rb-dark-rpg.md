---
id: T0276
title: "rb-dark-rpg: equipment screen and starter gear equip flow"
status: review
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, onboarding, equipment, state, uiux]
created: 2026-07-04
updated: 2026-07-04
---

## What

Implement the next onboarding step after the guard dialogue: the guard grants
starter gear into the backpack, the player opens Equipment from bottom nav, sees
the character/equipment slots/backpack, and equips the granted sword, armour, and
greaves through domain actions.

## Done when

- [x] Guard dialogue rewards create owned gear instances but do not auto-equip
      them.
- [x] A domain equip action validates ownership/item slot and equips by
      instance id without raw UI state mutation.
- [x] Starter sword, armour, and greaves equip actions can advance the authored
      quest steps/flags.
- [x] Equipment nav opens a runtime UI surface with 12 slots: weapon, offhand,
      head, armour, hands, waist, legs, feet, neck, ring_left, ring_right, relic.
- [x] The UI shows backpack gear and lets the player select/equip an item using
      engine-rendered text.
- [x] Layout remains usable in landscape and portrait without occupying the
      reserved top-left Poki area.
- [x] Relevant native tests and responsive quality checks pass.

## Open questions

- Final art for the character doll and item icons can be upgraded later; this
  slice may use existing icons/placeholders if provenance is already tracked.

## Log

- 2026-07-04: Started implementation. Plan review delegated to deep-reasoner;
  lead is beginning with the state/domain TDD slice.
- 2026-07-04: Implemented and moved to review. Guard rewards now go to
  backpack instead of auto-equipping. Added `game_actions_equip_gear`, generated
  equip-step content lookup, equipment screen UI, bottom-nav open path, and
  DevAPI equipment-screen scenario. Evidence: `game_dialogue_test`,
  `scene_interactions_test`, `quality_responsive`, `git diff --check`, and
  equipment responsive matrix in `tmp/quality/qclr_002_equipment_screen/`.
  Runtime smoke: tapping `Надеть` through UI equips `gear_old_sword_001` and
  advances `q001_gate_pass` to `equip_padded_jacket`.
