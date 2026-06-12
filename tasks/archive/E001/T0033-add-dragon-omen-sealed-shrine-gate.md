---
id: T0033
title: Add Dragon Omen sealed shrine gate
status: done
epic: E001
priority: P1
tags: [state,map,dragon-omen,iteration-4]
created: 2026-06-12
updated: 2026-06-12
---

## What

Add a small playable Sealed Shrine node north of Hunter's Ford. The shrine should be a Dragon Omen progression gate, not a dragon pet/power unlock: after the player has inspected the Dragon-Marked Shard, cleared Hunter's Ford, and reached Dragon Omen 2, the map offers a shrine route. Attuning it opens a persistent milestone and gives a first faction/world hint toward the next region beat.

Out of scope: a full shrine dungeon, dragon companion, new combat enemy, faction reputation, or a complete fourth region loop.

## Done when

- [x] Province Map shows a Sealed Shrine route with clear locked/open labels.
- [x] Shrine travel is gated by Hunter's Ford cleared, relic inspected, and Dragon Omen >= 2.
- [x] Attuning the shrine sets persistent opened and faction-hint state without spending Dragon Omen or granting a dragon power.
- [x] Schema v3, migration, fixtures, generated state, and DevAPI state tests cover the new fields.
- [x] Smoke/full-loop automation clicks through the gate and captures visual evidence.
- [x] Design data/docs reflect the new gate and first ash-wisp/faction hint.
- [x] Gameplay and code/state reviews have no unresolved blocking findings.

## Open questions

None. This is the smallest Iteration 4 gate slice implied by the current roadmap and prior relic hook.

## Log

- 2026-06-12: Started after T0032. Scope limited to one Dragon Omen-gated non-combat shrine payoff after Hunter's Ford.
- 2026-06-12: Implemented Sealed Shrine map route, v3 state fields/migration, attunement action, ash-wisp/faction hint, design data/docs, DevAPI state/smoke/playtest coverage, and missed-cache recovery after review.
- 2026-06-12: Evidence: `py -3.12 tools/state_codegen/generate_state.py --self-test`; `py -3.12 -m py_compile tools/devapi/smoke_test.py tools/devapi/scenarios/state_roundtrip.py tools/devapi/agent_playtest.py tools/state_codegen/generate_state.py`; `cmake --build --preset game-native-debug`; `py -3.12 tools/devapi/scenarios/state_roundtrip.py 9185`; `py -3.12 tools/devapi/smoke_test.py 9186`; `py -3.12 tools/devapi/agent_playtest.py 9187 --full-loop --exe build/game_67_idle/native-debug/game_67_idle.exe --out-dir build/captures/t0033_sealed_shrine_gate_final`; `cmake --build --preset game-native-qa`; `node tools/taskboard/cli.mjs validate`.
- 2026-06-12: Screenshot evidence: `build/captures/t0033_sealed_shrine_gate_final/screenshots/agent_sealed_shrine_20260612_102227.png`.
- 2026-06-12: Subagent review: gameplay/design found the missed-cache gate blocker; fixed by allowing cleared Ford cache recovery. Gameplay/design and code/state re-checks found no remaining blockers.
