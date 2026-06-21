---
id: T0016
title: "Third Ember Road slice: Old Mine entry choice"
status: dropped
epic: E003
priority: P1
tags: [prototype, ember-road, native-first, choice]
created: 2026-06-20
updated: 2026-06-21
---

## What

Add the next narrow Ember Road slice after level 2: an Old Mine entry surface
or simple choice/modal that appears only after the first quest is completed.
The goal is to prove the next route and choice UX, not to build a full dungeon.

Out of scope: full Old Mine content, new enemy family, inventory system,
crafting, market, party, or broad economy. Keep all game/world/UI logic Y-up;
boundary conversions only.

## Done when

- [x] Old Mine route is clickable/enabled only after level 2 quest completion.
- [x] A modal or choice surface exists and is captured as `modal_or_choice_open`
      in the live-state matrix.
- [x] The first choice communicates enter/scout/back or equivalent intent, with
      a locked/future-only consequence if dungeon content is not implemented.
- [x] Existing Old Gate -> North Road -> battle -> loot/equip -> claim flow
      still passes smoke and progression capture.
- [x] Strict product gate records PASS or exact REVIEW debt before any broader
      content expansion.
- [x] Native build, DevAPI smoke, capture states, taskboard validate, visual
      invariant guard, and `node tools/ai.mjs validate` pass.

## Open questions

- Answered for this slice: travel to `old_mine` and show a choice surface over
  the mine entrance scene.
- Should the first mine hook preview a new enemy, a resource node, or a locked
  depth floor?

## Log

- 2026-06-21: Lead closed the Ember Road prototype. This task is dropped and
  archived as historical evidence only; do not continue it unless explicitly
  reopened.
- 2026-06-20: Started after T0015 progression panel PASS. Scope is route entry
  and choice/modal UX only.
- 2026-06-20: product gate PASS (desktop); review: gamedesign/projects/ember-road/reviews/T0016_old_mine_choice_product_gate.md; screenshot: build/captures/ember-road/state_modal_or_choice_open.png; next: continue to the next narrow slice
- 2026-06-20: close-slice PASS gate (desktop); gate: gamedesign/projects/ember-road/reviews/T0016_old_mine_choice_product_gate.json; screenshot: build/captures/ember-road/state_modal_or_choice_open.png; evidence: cmake --build --preset native-debug --target game_seed passed | py -3.12 tools/ember-road/capture_states.py covered modal_or_choice_open at build/captures/ember-road/state_modal_or_choice_open.png | py -3.12 tools/devapi/smoke.py passed 27/27 including Old Mine choice UI tree and return flow | node tools/ai.mjs gate wrote PASS for T0016 Old Mine choice surface | node tools/taskboard/cli.mjs validate passed | node tools/visual_invariant_guard.mjs passed; Y-up invariant clean | node tools/ai.mjs validate passed quick reusable pipeline validation; next: Next narrow slice: add a dedicated Old Mine visual/reference digest or first mine encounter only after reviewing the current choice UX against stronger references.
