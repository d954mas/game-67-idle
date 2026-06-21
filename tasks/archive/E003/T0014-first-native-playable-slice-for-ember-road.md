---
id: T0014
title: First native playable slice for Ember Road
status: dropped
epic: E003
priority: P1
tags: [prototype, ember-road, native-first, lead-rejection]
created: 2026-06-20
updated: 2026-06-21
---

## What

Build the first native playable slice for `Ember Road`: town square -> accept
quest -> travel to North Road -> automated Road Wolf battle -> loot/XP/gold ->
equip or keep item -> return to town and claim reward.

## Done when

- [x] `gamedesign/projects/ember-road/gdd.md` names the first playable loop and player-readable goal.
- [x] `gamedesign/projects/ember-road/data/core_loop.json` describes the
      player verbs, rules, feedback, risk, goals, replay reason, and reference
      grounding without assuming hands-off progression, away-time rewards, or
      reset-meta loops.
- [x] `gamedesign/projects/ember-road/data/combat.json` defines the first
      automated battle with exact stats, round rules, win/loss/recovery, and
      expected result.
- [x] `gamedesign/projects/ember-road/data/ui_flow.json` defines the first
      town/map/battle/loot/equipment screens and transitions.
- [x] `gamedesign/projects/ember-road/visual/live_state_acceptance_matrix.json`
      is reviewed for this game's HUD, primary CTA, feedback, modal,
      blocked/affordable, and transient stress states.
- [x] A beautiful fantasy RPG fake shot or visual target exists before runtime
      polish starts.
- [x] A 5-line visual session contract exists: goal, non-goal, proof, stop
      condition, likely files.
- [x] `gamedesign/projects/ember-road/references/legend_legacy_dragons_digest.md`
      is completed or explicitly scoped out before implementing UI/art/balance
      from the named reference.
- [x] Current native screenshot or capture plan is compared against the fake
      shot/target in a mismatch list before visual code expands.
- [x] Native PC build/run command is identified and captured in the task log.
- [x] First native screenshot/product-read proof is captured before expanding content.

## Open questions

- Should the hero be a named protagonist, created avatar, or class archetype?
- Should the first tone be noble/high-fantasy, darker gothic, or colorful
  fairy-tale?
- Should autobattle allow pre-battle skill/loadout choices in the first slice,
  or stay fully automatic after Fight is pressed?

## Log

- 2026-06-21: Lead closed the Ember Road prototype. This task is dropped and
  archived as historical evidence only; do not continue it unless explicitly
  reopened.
- 2026-06-20: Captured lead concept: fantasy hero RPG, map/towns, quests,
  items, visual polish/animations, farming/grinding/leveling, automated battles,
  and `Legend: Legacy of the Dragons` as taste reference. Created project wiki,
  core loop, combat, UI flow, asset manifest, and reference digest gate.
- 2026-06-20: Native build passes: `cmake --build --preset native-debug`.
  Clean build now generates `ember_road_base.h`, builds
  `ember_road_base.ntpack` from slug text shaders plus
  `LilitaOne-RussianChineseKo.ttf`, and copies the pack to
  `build/game_seed/native-debug/assets/ember_road_base.ntpack`.
  DevAPI smoke passes 20/20 for accept quest -> travel -> autobattle -> equip
  ring -> claim reward -> level 2/gold, including reward-order stability.
  Screenshot evidence: `build/captures/iterate.png`; UI zoom:
  `build/captures/iterate_uizoom.png`.
- 2026-06-20: Removed handmade shape/bitmap font path; runtime UI text now uses
  generated font assets plus engine `nt_text_renderer`. Added visual invariant
  guard coverage that hard-fails handmade glyph/font helpers even with debug
  debt markers.
- 2026-06-20: Added `tools/ember-road/asset_pack_contract_guard.mjs` and wired
  it into pipeline validation. The guard enforces that clean native builds
  generate/copy `ember_road_base.ntpack`, include `ember_road/font_ui`, pass
  the CMake pack path into runtime, load the generated header ids, and render
  product text through the engine text renderer.
- 2026-06-20: product gate FAIL (desktop); review: gamedesign\projects\ember-road\reviews\T0014_first_slice_product_gate.md; screenshot: build/captures/iterate.png; next: Generate/import project-local fantasy UI source sheet and location/portrait/reward art, pack them through the asset pipeline, replace debug rectangles, then capture first/reward/locked/transient states before content expansion.
- 2026-06-20: Lead rejected the current direction more broadly: visual is not
  like the desired game, and even the UX is wrong. Added
  `gamedesign/projects/ember-road/references/fantasy_browser_rpg_ux_reference_set.md`
  and updated the visual gate to treat `build/captures/iterate.png` as a
  mismatch artifact, not a layout base. Next step is an accepted composed
  fantasy browser-RPG fake shot/direction board before more UI-kit generation
  or runtime visual polish.
- 2026-06-20: product gate FAIL (desktop); review: gamedesign/projects/ember-road/reviews/T0014_first_slice_product_gate_ux_reset.md; screenshot: build/captures/iterate_first_screen.png; next: Generate or import accepted project-local fantasy location, NPC/hero/wolf, route, reward, and UI frame assets; pack them through the asset pipeline; then recapture first/reward/locked/transient states.
- 2026-06-20: Strengthened the post-rejection reference study with
  `gamedesign/projects/ember-road/references/fantasy_browser_rpg_central_deconstruction.md`.
  Added similar references beyond the named Legend anchor and recorded a
  source-ladder gap: strong enough for fake-shot/art-packet work, not final
  runtime art or pacing claims. Generated and accepted a direction fake shot:
  `gamedesign/projects/ember-road/art/ember-road-old-gate-fakeshot-v001.png`.
  Art job `gamedesign/projects/ember-road/art_requests/ember-road-old-gate-fakeshot-v001.json`
  is draft-valid; source-family coverage intentionally fails until Old Gate
  background, hero/NPC/enemy, route/reward icons, and browser-RPG frame pieces
  are generated as separate runtime assets. Y-up remains mandatory for all
  game/UI layout; conversions are boundary-only.
- 2026-06-20: Generated and accepted project-local source families for the Old
  Gate visual pass: background layers, hero/Gate Warden/Road Wolf sprites,
  quest/reward/route icons, and fantasy browser-RPG UI frame parts. Added
  provenance records under `gamedesign/projects/ember-road/art/generation_records/`.
  Source-family coverage now passes:
  `gamedesign/projects/ember-road/reviews/ember-road-old-gate-fakeshot-v001-source_family_coverage_audit.md`.
  Next blocker is crop/slice metadata, runtime packing, and native rendering;
  the current product gate remains FAIL until the native screen stops using
  debug shapes.
- 2026-06-20: Added `tools/ember-road/slice_old_gate_assets.py`, sliced the
  accepted source sheets into 57 project-local runtime PNGs under
  `assets/runtime/ember-road-old-gate-fakeshot-v001/`, and updated
  `gamedesign/projects/ember-road/data/ember-road-old-gate-fakeshot-v001-crop_manifest.json`
  plus `gamedesign/projects/ember-road/data/ember-road-old-gate-fakeshot-v001-asset_manifest.json`.
  Draft art job validation passes; strict final-art validation remains
  intentionally blocked by missing slice9 policy/final native proof. Some green
  fringe remains on alpha edges and must be cleaned before release-quality
  final art.
- 2026-06-20: Integrated the Old Gate runtime PNGs into
  `ember_road_base.ntpack` as `ember_road_old_gate_atlas` with sprite shaders
  and generated atlas region ids. Native runtime now initializes `nt_atlas` and
  `nt_sprite_renderer`, renders the Old Gate/North Road backdrop, hero, Gate
  Warden, Road Wolf, route plaques, quest rail, reward preview, HUD frame, and
  CTA buttons from packed atlas assets while preserving Y-up `UiBox` layout and
  boundary-only conversions. Updated the slicer with slice9 metadata and
  edge-only chroma cleanup; slice9 policy audit now passes. Evidence:
  `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py` 20/20,
  `py -3.12 tools/devapi/iterate.py`,
  `build/captures/iterate.png`,
  `build/captures/iterate_uizoom.png`,
  `node tools/ai.mjs validate`,
  `node tools/visual_invariant_guard.mjs`,
  `node tools/taskboard/cli.mjs validate`.
- 2026-06-20: product gate REVIEW (desktop); review: gamedesign/projects/ember-road/reviews/T0014_first_slice_product_gate_atlas_review.md; screenshot: build/captures/iterate.png; next: Capture required live states, review against the Old Gate fake shot, then fix any lead/product-read issues before content expansion.
- 2026-06-20: Added `tools/ember-road/capture_states.py` and captured live
  first-screen, HUD, primary-action, feedback, reward, locked, and transient
  states through native DevAPI. Evidence:
  `gamedesign/projects/ember-road/visual/live_state_capture_matrix.json`,
  `gamedesign/projects/ember-road/reviews/live_state_capture_report.json`,
  `build/captures/ember-road/state_first_screen.png`,
  `build/captures/ember-road/state_reward_active.png`.
- 2026-06-20: product gate REVIEW (desktop); review: gamedesign/projects/ember-road/reviews/T0014_first_slice_product_gate_live_states_review.md; screenshot: build/captures/ember-road/state_first_screen.png; next: Fix reward panel overlap and route/readability polish against the captured live states before adding any new locations, enemies, or systems.
- 2026-06-20: product gate REVIEW (desktop); review: gamedesign/projects/ember-road/reviews/T0014_first_slice_product_gate_live_states_review.md; screenshot: build/captures/ember-road/state_first_screen.png; next: Polish crop/fringe/scale and quest-panel typography against the captured live states, then rerun capture states and strict product gate before adding locations, enemies, or systems.
- 2026-06-20: product gate PASS (desktop); review: gamedesign/projects/ember-road/reviews/T0014_first_slice_product_gate_live_states_review.md; screenshot: build/captures/ember-road/state_first_screen.png; next: continue to the next narrow slice
- 2026-06-20: close-slice PASS gate (desktop); gate: gamedesign/projects/ember-road/reviews/T0014_first_slice_product_gate_live_states_review.json; screenshot: build/captures/ember-road/state_first_screen.png; evidence: cmake --build --preset native-debug --target game_seed passed | py -3.12 tools/ember-road/capture_states.py captured first/HUD/primary/feedback/reward/locked/transient states | py -3.12 tools/devapi/smoke.py passes 20/20 first-loop checks | node tools/ai.mjs gate wrote PASS for build/captures/ember-road/state_first_screen.png; resolved rejection: Lead rejected prior visual and UX as not like the desired game; proof is the packed-atlas fantasy RPG first screen plus live-state PASS gate in gamedesign/projects/ember-road/reviews/T0014_first_slice_product_gate_live_states_review.json.; next: Start the next narrow Ember Road slice: return/claim flow plus a minimal progression panel, keeping Y-up and live-state captures.
