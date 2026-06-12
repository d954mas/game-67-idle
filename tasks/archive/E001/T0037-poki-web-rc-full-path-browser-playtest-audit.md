---
id: T0037
title: Poki web RC full-path browser playtest audit
status: done
epic: E001
priority: P1
tags: [poki, web, qa, release, browser]
created: 2026-06-12
updated: 2026-06-12
---

## What

Extend web RC validation beyond the current first-two-tap canvas audit. The
release build now launches and the first reward path passes in browser, but
Poki readiness needs a browser-side full-path playtest that covers combat,
inventory/equip, camp, Hunter's Ford, sealed shrine, mobile portrait, and
desktop.

Out of scope: changing game balance or art; this task is validation harness and
evidence only.

## Done when

- [x] Web release audit drives the first playable path through at least first reward, combat win, Rusty Blade equip, camp/relic, Hunter's Ford payoff, and shrine attunement.
- [x] Desktop and mobile portrait browser screenshots are captured for key states and pass nonblank/visual-change checks.
- [x] Audit reports console errors/warnings and fails on load/runtime/input regressions.
- [x] `cmake --build --preset game-wasm-release` and the new full-path browser audit pass from a clean command.
- [x] Report path and screenshot directory are recorded in this task log.

## Open questions

None.

## Log

- 2026-06-12: Captured after Iteration 4. Current `tools/devapi/scenarios/web_visual_qa_audit.mjs` is useful but only verifies initial map, Old Road tap, and first Search reward because the web target has no DevAPI.
- 2026-06-12: Completed full-path browser RC audit in `tools/devapi/scenarios/web_visual_qa_audit.mjs`. The audit now fixes desktop slot coordinates, drives 22 interactions per viewport from Old Road through shrine attunement, captures 23 screenshots per viewport, checks visual health and step-to-step canvas changes, records console warnings/page errors, and fails on console/page errors. Validation passed after `cmake --build --preset game-wasm-release` (`ninja: no work to do`) with `node tools/devapi/scenarios/web_visual_qa_audit.mjs build/game_67_idle/wasm-release`. Evidence: `build/captures/web_visual_qa_audit/2026-06-12T07-30-22-344Z/report.json` and screenshots in `build/captures/web_visual_qa_audit/2026-06-12T07-30-22-344Z/`.
