---
id: T0017
title: "Fourth Ember Road slice: dedicated Old Mine entrance visual"
status: dropped
epic: E003
priority: P1
tags: [prototype, ember-road, native-first, visual, old-mine]
created: 2026-06-20
updated: 2026-06-21
---

## What

Replace the T0016 visual compromise where `old_mine` reused the North Road
backdrop. Add a dedicated Old Mine entrance bitmap/background and prove the
choice UX against a fresh native screenshot.

Out of scope: full dungeon content, new enemies, combat balance, inventory
expansion, crafting, economy, or new town systems. Keep game/world/UI logic
Y-up; convert Y-down only at renderer/input/screenshot/DevAPI boundaries.

## Done when

- [x] Old Mine uses a dedicated mine entrance backdrop in runtime, not the
      North Road background.
- [x] The mine choice surface still communicates Scout locked/future and Back
      active without hiding the location fantasy.
- [x] `modal_or_choice_open` capture shows the dedicated mine entrance asset.
- [x] Strict product gate records PASS or exact REVIEW debt for the visual
      target before any dungeon-content expansion.
- [x] Native build, DevAPI smoke, capture states, taskboard validate, visual
      invariant guard, and `node tools/ai.mjs validate` pass.

## Open questions

- Should the next slice add a first mine encounter or only improve the mine
  map/choice UX further?

## Log

- 2026-06-21: Lead closed the Ember Road prototype. This task is dropped and
  archived as historical evidence only; do not continue it unless explicitly
  reopened.
- 2026-06-20: Started after T0016 PASS. Main mismatch: Old Mine route works but
  still visually reuses the road backdrop.
- 2026-06-20: Generated and integrated
  `gamedesign/projects/ember-road/art/ember-road-old-mine-entrance-backdrop-v001.png`
  as `old_mine_backdrop` in the runtime atlas; capture
  `build/captures/ember-road/state_modal_or_choice_open.png` shows the dedicated
  cave entrance with the choice panel moved into the quest rail.
- 2026-06-20: product gate PASS (desktop); review:
  `gamedesign/projects/ember-road/reviews/T0017_old_mine_entrance_visual_product_gate.md`;
  screenshot: `build/captures/ember-road/state_modal_or_choice_open.png`; next:
  continue to the next narrow slice.
- 2026-06-20: product gate PASS (desktop); review: gamedesign/projects/ember-road/reviews/T0017_old_mine_entrance_visual_product_gate.md; screenshot: build/captures/ember-road/state_modal_or_choice_open.png; next: continue to the next narrow slice
- 2026-06-20: close-slice PASS gate (desktop); gate: gamedesign/projects/ember-road/reviews/T0017_old_mine_entrance_visual_product_gate.json; screenshot: build/captures/ember-road/state_modal_or_choice_open.png; evidence: cmake --build --preset native-debug --target game_seed passed; atlas rebuilt with old_mine_backdrop | py -3.12 tools/ember-road/capture_states.py covered modal_or_choice_open at build/captures/ember-road/state_modal_or_choice_open.png with dedicated Old Mine backdrop | py -3.12 tools/devapi/smoke.py passed 27/27 including Old Mine choice UI tree and return flow | node tools/ai.mjs gate wrote PASS for T0017 Old Mine entrance visual | node tools/taskboard/cli.mjs validate passed | node tools/visual_invariant_guard.mjs passed; Y-up invariant clean | node tools/ai.mjs validate passed quick reusable pipeline validation; next: Next narrow slice: add the first Old Mine encounter/resource/depth preview without regressing the dedicated entrance visual or Y-up layout.
