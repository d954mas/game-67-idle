---
id: T0035
title: Poki runtime UI icons, buttons, and reward juice polish
status: done
epic: E001
priority: P1
tags: [poki,ui,visual,icons,rewards]
created: 2026-06-12
updated: 2026-06-12
---

## What

Finish the runtime UI polish needed for a bright cartoon/toy-like Poki read.
Iteration 2 added shape-only HUD placeholders and button accent rails, but the
release-candidate visual target still needs asset-backed icons, explicit button
classes, and reward moments that feel like game payoffs instead of log text.

Out of scope: full background repaint, new combat systems, base-builder camp
features, or engine/submodule changes.

## Done when

- [x] Runtime HUD has readable icon silhouettes for Health, Resolve, Supplies, Gold, Herbs, and Dragon Omen.
- [x] Main action buttons use explicit visual classes for primary route, safe/camp, danger/combat, reward/loot, mystical/shrine, disabled, and pressed states.
- [x] First reward, Rusty Blade equip, Hunter's Ford cache/payoff, and shrine attunement show a bright reward banner, stat-change chip, or equivalent visual reward beat.
- [x] Desktop 16:9 screenshots cover fresh map, first reward, inventory, combat, camp/relic, Hunter's Ford, and sealed shrine.
- [x] Mobile portrait screenshot confirms text fit, tap target clarity, and no incoherent overlap.

## Open questions

None.

## Log

- 2026-06-12: Captured from designer Iteration 2. No-asset placeholders and accent rails landed in `src/main.c`; remaining work needs a deliberate runtime UI icon/reward pass.
- 2026-06-12: Iteration 3 started. Scope: explicit runtime button classes plus reward banner/chips for first reward, Rusty Blade/equip, Hunter's Ford payoff, and shrine attunement, then native/web screenshot validation.
- 2026-06-12: Iterations 3-4 completed runtime UI/reward polish in `src/main.c`: HUD icon silhouettes, explicit button classes including pressed state, reward banners/chips/confetti, brighter toy-map first screen, and warmer web/native background treatment. Evidence: `build/captures/iter4_t0035_visual_states_desktop/t0035_visual_states_report.json`, `build/captures/iter4_t0035_visual_states_mobile/t0035_visual_states_report.json`, and web release audit `build/captures/web_visual_qa_audit/2026-06-12T07-05-07-798Z/report.json`. Validation passed: `cmake --build --preset game-native-debug`, `py -3.12 tools/devapi/scenarios/first_30_seconds_contract.py 9251 build/captures/iter4_first30_contract`, `py -3.12 tools/devapi/smoke_test.py 9252`, `cmake --build --preset game-wasm-release`, and `node tools/devapi/scenarios/web_visual_qa_audit.mjs build/game_67_idle/wasm-release`.
