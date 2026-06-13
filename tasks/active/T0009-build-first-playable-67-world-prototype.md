---
id: T0009
title: Build first playable 67 World prototype
status: review
epic: ""
priority: P1
tags: [gameplay, state, devapi, prototype]
created: 2026-06-12
updated: 2026-06-12
---

## What

Implement the first runnable 67 World prototype from the accepted GDD: spawn
Tiny 67, merge matching variants, discover collection progress, earn giggle
coins, expose stable DevAPI actions, and capture native desktop evidence.

## Done when

- [x] State schema stores first-slice 67 World progression.
- [x] Native screen shows a playable 67 board, collection tray, and controls.
- [x] DevAPI exposes semantic spawn/merge/upgrade actions.
- [x] A scenario proves spawn -> spawn -> merge -> screenshot.
- [x] Native debug build and targeted DevAPI scenario pass.

## Open questions

None for the current native asset-backed prototype. Next pass should focus on
visual polish, FTUE clarity, and user acceptance.

## Log

- 2026-06-12: Started implementation in parallel with character lineup and
  Cow Evolution reference research.
- 2026-06-12: Implemented state fields, `game_state_actions`, 67 World runtime
  screen, semantic DevAPI actions, and `first_67_loop.py`.
- 2026-06-12: Evidence passed:
  `cmake --build --preset native-debug`,
  `py -3.12 tools/devapi/smoke_test.py 9134`, and
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9133 build/captures/scenarios/first_67_loop.png`.
- 2026-06-12: User rejected the button-like/debug feel and correctly called out
  that web playtest work violates the PC-first project rule. Continued scope is
  native PC only: make the PC build feel like a Cow Evolution-style playable
  merge board.
- 2026-06-12: Prepared generated runtime art path for native integration:
  sliced 35 reusable character/UI PNGs into `assets/runtime/67-world/`, added
  `build_67_world_packs`, and built
  `build/game_seed/67-world-packs/world67_art.ntpack`.
- 2026-06-12: Integrated `world67_art.ntpack` into the native PC runtime for
  board tiles/frame, cards, first seven 67 characters, spawn button/crate,
  upgrade button, HUD icons, and progress art. Evidence passed:
  `cmake --build --preset native-debug`,
  `node tools/taskboard/cli.mjs validate`,
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9148 build/captures/scenarios/first_67_loop_assets_v3.png`,
  and `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/first_67_loop_assets_v3.png`.
- 2026-06-12: Added first FTUE state progression: `world_67.ftue_step`,
  `world_67.ftue_prompt`, `world_67.next_goal`, and `tutorial.done` completes
  after the first Tiny 67 -> Berry 67 merge. Evidence passed:
  `cmake --build --preset native-debug`,
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9149 build/captures/scenarios/first_67_loop_ftue_v1.png`,
  and `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/first_67_loop_ftue_v1.png`.
- 2026-06-12: Improved collection tray readability in native PC art mode:
  unlocked cards show full characters, the next locked variant shows a dim
  preview plus lock, and later locked variants show lock icons instead of
  fully revealed characters. Evidence passed:
  `cmake --build --preset native-debug`,
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9151 build/captures/scenarios/first_67_loop_collection_locks_v2.png`,
  and `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/first_67_loop_collection_locks_v2.png`.
- 2026-06-12: Added a runtime-composed `TAP BOX` / `MERGE FIRST` CTA above the
  spawn button in native PC art mode so the main child-facing action reads as a
  button, not only a decorative crate panel. Evidence passed:
  `cmake --build --preset native-debug`,
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9152 build/captures/scenarios/first_67_loop_spawn_cta_v1.png`,
  and `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/first_67_loop_spawn_cta_v1.png`.
- 2026-06-12: Added merge reward feedback in native PC art mode: successful
  merges remember the created variant and pulse the matching collection card
  with generated highlight/star assets. Evidence passed:
  `cmake --build --preset native-debug`,
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9153 build/captures/scenarios/first_67_loop_merge_reward_v1.png`,
  and `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/first_67_loop_merge_reward_v1.png`.
- 2026-06-12: Added board merge guidance in native PC art mode: the runtime
  exposes `world_67.merge_hint_slot_a/b`, draws a generated arrow/star guide
  over the first mergeable pair, and clears the hint after the merge. Evidence
  passed: `cmake --build --preset native-debug`,
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9154 build/captures/scenarios/first_67_loop_merge_guidance_v1.png`,
  and `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/first_67_loop_merge_guidance_v1.png`.
- 2026-06-12: Improved upgrade affordance and HUD readability in native PC art
  mode. The scenario now verifies upgrade states (`locked`, `saving`, `ready`,
  `bought`), the upgrade button renders its state inside the button instead of
  overlapping the board frame, and FTUE/goal text stays readable in the top
  HUD. Evidence passed: `cmake --build --preset native-debug`,
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9156 build/captures/scenarios/first_67_loop_hud_readability_v1.png`,
  and `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/first_67_loop_hud_readability_v1.png`.
