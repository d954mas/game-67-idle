---
id: T0031
title: Add Dragon Relic inspection camp beat
status: done
epic: E001
priority: P1
tags: [gameplay, camp, dragon, progression]
created: 2026-06-12
updated: 2026-06-12
---

## What

Add the missing early `Inspect Dragon Relic` camp interaction so Dragon Omen becomes a visible mystery beat instead of only a number.

Scope: a camp button unlocked by `has_dragon_marked_shard`, one persistent `dragon_relic_inspected` milestone, a readable relic resonance message, and a small next-hook signal toward a future sealed shrine.

Out of scope: dragon pet, dragon collection, combat power unlock, full shrine node, new art pack, or faction system.

## Done when

- [x] camp shows an `Inspect`/relic action after the Dragon-Marked Shard is found
- [x] inspecting the relic sets persistent progress and gives a readable mystery payoff
- [x] repeated inspect is stable and does not farm resources
- [x] visible camp/map text makes Dragon Omen feel like a mystery hook, not just a number
- [x] design data/docs are updated for the implemented camp beat
- [x] DevAPI smoke/playtest/state coverage includes the relic beat
- [x] native screenshot evidence shows the relic inspection state
- [x] gameplay/code review issues are recorded and fixed before closing

## Open questions

## Log

- 2026-06-12: Started after T0030. Addresses the documented risk that Dragon Omen is currently just a number.
- 2026-06-12: Implemented `dragon_relic_inspected`, camp `Inspect/Review` action, sealed shrine camp/map hook, v2 migration, and autosave reload coverage. Review fixes: hid inspect before shard, removed Omen reward from inspect, kept repeat UI stable, updated balance/progression/implementation docs, added keyed save temp-file replace.
- 2026-06-12: Evidence: `cmake --build --preset game-native-debug`; `py -3.12 tools/devapi/scenarios/state_roundtrip.py 9174`; `py -3.12 tools/devapi/smoke_test.py 9175`; `py -3.12 tools/devapi/agent_playtest.py 9176 --full-loop --exe build/game_67_idle/native-debug/game_67_idle.exe --out-dir build/captures/t0031_dragon_relic_inspection_final`; `cmake --build --preset game-native-qa`; `node tools/taskboard/cli.mjs validate`.
- 2026-06-12: Screenshot evidence: `build/captures/t0031_dragon_relic_inspection_final/screenshots/agent_relic_20260612_093337.png`.
