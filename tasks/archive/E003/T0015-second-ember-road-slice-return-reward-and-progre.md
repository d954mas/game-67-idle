---
id: T0015
title: "Second Ember Road slice: return reward and progression panel"
status: dropped
epic: E003
priority: P1
tags: [prototype, ember-road, native-first, progression]
created: 2026-06-20
updated: 2026-06-21
---

## What

Add the next narrow `Ember Road` slice after the first visual gate pass:
make the post-battle return/claim/reward progression read like an RPG loop, not
just a state change. The slice should expose a minimal progression panel/state
for level, XP, gold, equipped ring, and next locked/unlocked route consequence.

Out of scope: no second town, no new enemy family, no crafting/party/PvP, and
no broad economy. Keep world/UI/game layout Y-up; convert Y-down input/capture
only at boundaries.

## Done when

- [ ] Native flow clearly shows victory -> equip/claim -> return/reward
      progression without relying on chat/log explanation.
- [ ] A minimal progression panel or panel-open state is visible and captured
      after claim/equip.
- [ ] `game.action.claim_reward` and UI click path still reach level 2/gold and
      keep equip-before/claim-before order stable.
- [ ] Live-state matrix covers the new progression panel state instead of
      marking it as first-slice debt.
- [ ] Strict product gate for the updated slice is PASS or REVIEW with exact
      remaining debt before any new content expansion.
- [ ] `cmake --build --preset native-debug --target game_seed`,
      `py -3.12 tools/devapi/smoke.py`, `py -3.12 tools/ember-road/capture_states.py`,
      `node tools/taskboard/cli.mjs validate`, and `node tools/ai.mjs validate`
      pass after changes.

## Open questions

- Should the progression panel be always visible after claim, or opened by a
  compact character/status button?
- Should Old Mine unlock immediately at level 2 in this slice, or remain a
  preview until the next slice?

## Log

- 2026-06-21: Lead closed the Ember Road prototype. This task is dropped and
  archived as historical evidence only; do not continue it unless explicitly
  reopened.
- 2026-06-20: Started after T0014 first visual slice reached PASS. Scope is
  progression/readability for the existing loop only; no new broad content.
- 2026-06-20: product gate PASS (desktop); review: gamedesign/projects/ember-road/reviews/T0015_progression_panel_product_gate.md; screenshot: build/captures/ember-road/state_progression_panel_open.png; next: continue to the next narrow slice
- 2026-06-20: close-slice PASS gate (desktop); gate: gamedesign/projects/ember-road/reviews/T0015_progression_panel_product_gate.json; screenshot: build/captures/ember-road/state_progression_panel_open.png; evidence: cmake --build --preset native-debug --target game_seed passed | py -3.12 tools/ember-road/capture_states.py covers progression_panel_open at build/captures/ember-road/state_progression_panel_open.png | py -3.12 tools/devapi/smoke.py passes 20/20 including equip/claim ordering | node tools/ai.mjs validate passed quick reusable pipeline validation; next: Next narrow slice: Old Mine entry encounter or modal/choice, with fresh live-state coverage and Y-up layout.
