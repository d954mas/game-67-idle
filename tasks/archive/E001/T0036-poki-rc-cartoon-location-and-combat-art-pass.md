---
id: T0036
title: Poki RC cartoon location and combat art pass
status: done
epic: E001
priority: P1
tags: [poki, visual, art, combat, locations]
created: 2026-06-12
updated: 2026-06-12
---

## What

Replace the remaining adult/painterly runtime location read with a cohesive
cartoon/toy-like Poki presentation. Iteration 4 made the first province map,
inventory, HUD, buttons, and rewards brighter, but ruins/combat/Hunter's Ford/
shrine still rely on realistic background art and minimal block motifs.

Out of scope: new combat rules, new progression content, engine/submodule
changes, and automatic asset-pack generation in normal builds.

## Done when

- [x] Runtime ruins, combat, Hunter's Ford, and sealed shrine screens have a bright cartoon/toy-like backdrop or paintover treatment that no longer reads as dark adult fantasy.
- [x] Combat has an immediately readable enemy/target silhouette and a clear primary Attack action on desktop and mobile portrait.
- [x] Hunter's Ford and sealed shrine each have distinct friendly landmark motifs visible in screenshots.
- [x] Desktop and mobile portrait screenshots show first reward, combat, Ford payoff, and shrine attunement without incoherent overlap.
- [x] `cmake --build --preset game-native-debug`, `py -3.12 tools/devapi/smoke_test.py <port>`, and focused visual capture pass.

## Open questions

None.

## Log

- 2026-06-12: Captured after Iteration 4 director review. T0035 closed runtime UI/reward polish, but Designer still flags realistic ruins/combat/Ford/shrine backgrounds as the main visual RC risk for Poki.
- 2026-06-12: Iteration 5 completed in `src/main.c`: code-drawn toy backdrops now cover ruins/reward, camp, inventory, Hunter's Ford, sealed shrine, and combat over the old painterly placeholders; combat has a visible target/HP bar and role-colored actions. Evidence: `build/captures/iter5_combat_buttons_desktop/t0035_visual_states_report.json`, `build/captures/iter5_combat_buttons_mobile/t0035_visual_states_report.json`, and web release audit `build/captures/web_visual_qa_audit/2026-06-12T07-16-50-305Z/report.json`. Validation passed: `cmake --build --preset game-native-debug`, `py -3.12 tools/devapi/scenarios/first_30_seconds_contract.py 9271 build/captures/iter5_combat_buttons_first30`, `py -3.12 tools/devapi/smoke_test.py 9272`, `cmake --build --preset game-wasm-release`, and `node tools/devapi/scenarios/web_visual_qa_audit.mjs build/game_67_idle/wasm-release`.
