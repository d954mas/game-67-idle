---
id: T0027
title: Add loot compare and Rusty Blade equip beat
status: done
epic: E001
priority: P1
tags: [gameplay, loot, inventory, combat]
created: 2026-06-12
updated: 2026-06-12
---

## What

Add the first loot comparison/equip beat promised by the fantasy RPG GDD: after clearing the ruin, the player can see why `Rusty Blade` matters, equip it, and get a visible combat stat change without introducing a full inventory system.

Out of scope: multi-slot inventory, item lists, shops, durability, armor, and procedural loot.

## Done when

- [x] loot result or inventory UI shows `Old Knife 5 -> Rusty Blade 7`
- [x] player can equip `Rusty Blade` after it is looted
- [x] combat damage uses the equipped weapon value
- [x] DevAPI/smoke coverage proves the equip state and damage value
- [x] native screenshot evidence shows the compare/equip beat or resulting inventory state

## Open questions

## Log

- 2026-06-12: Started from GDD gap review. Core loop exists, but the first loot result currently grants `Rusty Blade` without a readable compare/equip decision.
- 2026-06-12: Completed. `Take All` now opens a lightweight inventory compare screen, `Equip` sets `rusty_blade_equipped`, state exposes `weapon_damage`/`equipped_weapon`, and combat attack damage uses the equipped weapon value.
- 2026-06-12: Review fix: inventory screenshot exposed an existing `DRAGON OMEN` label overlap with the omen badge; shifted the label left and recaptured evidence.
- 2026-06-12: Code review fix: added state validation so `rusty_blade_equipped=true` requires `has_rusty_blade=true`; updated state roundtrip coverage accordingly.
- 2026-06-12: Evidence: `cmake --build --preset game-native-debug`, `py -3.12 tools/devapi/smoke_test.py 9144`, `py -3.12 tools/devapi/scenarios/state_roundtrip.py 9146`, `py -3.12 tools/devapi/agent_playtest.py 9145 --full-loop --exe build/game_67_idle/native-debug/game_67_idle.exe --out-dir build/captures/t0027_inventory_equip_review`, `cmake --build --preset game-native-qa`, `node tools/taskboard/cli.mjs validate`; screenshot `build/captures/t0027_inventory_equip_review/screenshots/agent_inventory_20260612_024354.png`.
