---
id: T0024
title: Stronger Ember Road reference and UX reset
status: review
epic: E003
priority: P0
tags: [prototype, ember-road, visual, ux, references, lead-rejection, y-up]
created: 2026-06-20
updated: 2026-06-20
---

## What

### Iteration Goal

Respond to renewed lead rejection: the current Ember Road visual and UX still
do not match the intended game. Strengthen the reference study, add closer
similar references, and freeze content expansion behind a clearer visual/UX
translation gate.

### Scope

- Add a stronger reference refresh that compares the current T0023 town forge
  screenshot against fantasy browser-RPG screen grammar.
- Promote more similar references with specific roles: dense browser RPG,
  quest guidance, scene-first quest locations, compact dungeon/equipment loop,
  reward/result clarity, and caution refs.
- Make the next proof target explicit before any Depth 2, economy, or runtime
  art expansion.
- Reassert Y-up as a hard game/world/UI invariant.

### Out Of Scope

- No Depth 2 combat, procedural mine floors, extra economy, final UI kit, or
  copied reference layout in this slice.
- No Y-down layout semantics. Renderer/input/screenshot/DevAPI conversion only.

## Done when

- [x] A strengthened reference refresh exists and names more similar refs.
- [x] Current T0023 screenshot mismatch is recorded as rejected/review debt.
- [x] Borrow / avoid / copy-risk are explicit.
- [x] The next proof target is one visual/UX pass, not new gameplay content.
- [x] Y-up invariant is stated in the digest and task.
- [x] A draft town forge v2 art job and prompt packet exist before generation.
- [x] A town forge v2 direction fake shot is generated, saved, and reviewed.
- [ ] Lead accepts the strengthened direction or provides the next named target.

## Open questions

- Is `Legend: Legacy of the Dragons` still the primary taste anchor, or should
  the next visual target lean harder into a closer adjacent ref such as Dragon
  Eternity, DragonFable, or a specific screenshot/video from the lead?

## Log

- 2026-06-21: Created after renewed lead rejection: "visual is not like the
  game I want", "even UX is wrong", and "Y must be up always". Added stronger
  reference refresh:
  `gamedesign/projects/ember-road/references/stronger_visual_ux_reference_refresh_2026-06-21.md`.
- 2026-06-21: Added draft art job and fake-shot prompt packet for the next
  proof target:
  `gamedesign/projects/ember-road/art_requests/ember-road-town-forge-v2.json`
  and
  `gamedesign/projects/ember-road/art/prompts/ember-road-town-forge-v2-fakeshot-prompt.md`.
- 2026-06-20: product gate REVIEW (direction-target); review: gamedesign/projects/ember-road/reviews/T0024_town_forge_v2_direction_target_review.md; screenshot: gamedesign/projects/ember-road/art/ember-road-town-forge-v2.png; next: Generate or derive separate town forge background, lantern/resource icons, character/forge FX, and frame/source families; then integrate the smallest native forge visual pass and capture state_town_lantern_upgrade_v2.png.
- 2026-06-21: Saved generation record:
  `gamedesign/projects/ember-road/art/generation_records/ember-road-town-forge-v2.json`.
  The fake shot is direction evidence only; final runtime still requires
  separate source families and native screenshot proof.
- 2026-06-21: Generated first runtime-relevant source family:
  `gamedesign/projects/ember-road/art/ember-road-town-forge-v2-forge-lantern-resource-icon-sheet-v001.png`
  with generation record
  `gamedesign/projects/ember-road/art/generation_records/ember-road-town-forge-v2-forge-lantern-resource-icon-sheet-v001.json`.
- 2026-06-21: Generated town forge background/source layer sheet:
  `gamedesign/projects/ember-road/art/ember-road-town-forge-v2-town-forge-background-layer-sheet-v001.png`.
- 2026-06-21: Generated town forge character/FX source sheet:
  `gamedesign/projects/ember-road/art/ember-road-town-forge-v2-town-forge-character-sprite-sheet-v001.png`.
- 2026-06-21: Generated town forge UI frame/source sheet:
  `gamedesign/projects/ember-road/art/ember-road-town-forge-v2-fantasy-forge-ui-frame-sheet-v001.png`.
- 2026-06-21: Source-family coverage now passes for the v2 art job:
  `gamedesign/projects/ember-road/reviews/ember-road-town-forge-v2-source_family_coverage_audit.md`.
  Crop/runtime integration is not ready yet: icon sheet intake failed on
  chroma/key contamination even after normalized green, blue, and magenta
  candidate tests. Next step is safer source-sheet intake: true alpha,
  dual-plate, or a stricter regenerated sheet before crop manifests.
- 2026-06-21: Strengthened the reference refresh with a stricter similarity
  filter, screen translation rules, Town Forge V2 direction gap, and explicit
  Y-up non-negotiable: larger logical `y` means higher on the game screen;
  Y-down values are boundary-only and never stored layout truth.
- 2026-06-21: Promoted additional closer loop references: Broken Ranks for
  hand-finished fantasy location/combat identity, plus Gladiatus, BattleKnight,
  and Tanoth for legacy browser-RPG mission/expedition/dungeon -> loot/equip
  progression grammar.
- 2026-06-20: product gate REVIEW (town-forge-native-v2); review: gamedesign/projects/ember-road/reviews/T0024_town_forge_native_scene_anchor_review.md; screenshot: build/captures/ember-road/state_town_lantern_upgrade.png; next: Fix clean town-forge source-sheet intake, cut runtime assets, then replace this temporary scene workbench with source-derived forge/lantern assets and rerun strict visual gate.
- 2026-06-21: Implemented the smallest native Y-up forge scene-anchor pass:
  `src/clean_seed_main.c` now places a clickable `ember.scene.forge_workbench`
  in the Old Gate scene for the lantern upgrade and exposes forged state as
  `town_lantern_forged_open`. Captures refreshed:
  `build/captures/ember-road/state_town_lantern_upgrade.png` and
  `build/captures/ember-road/state_town_lantern_forged.png`. This remains
  REVIEW, not PASS, because the forge anchor is a temporary composition from
  older runtime assets, not clean town-forge source art.
- 2026-06-20: product gate REVIEW (town-forge-native-v2); review: gamedesign/projects/ember-road/reviews/T0024_town_forge_native_scene_anchor_review.md; screenshot: build/captures/ember-road/state_town_lantern_upgrade.png; next: Regenerate or repair the town-forge source sheets with a safer key color, cut the full forge/workbench and standalone lantern assets, then rerun strict product gate.
- 2026-06-21: Cut a partial source-derived runtime set from safe town-forge v2
  components only: `forge_action_panel_v2`, `forge_lantern_ready_badge_v2`,
  `forge_signpost_v2`, `forge_floor_patch_v2`, and
  `forge_result_strip_slice9_v2`. Whole-sheet contaminated forge/workbench and
  standalone lantern components remain excluded. Updated manifests:
  `gamedesign/projects/ember-road/data/ember-road-town-forge-v2-crop_manifest.json`
  and
  `gamedesign/projects/ember-road/data/ember-road-town-forge-v2-asset_manifest.json`.
- 2026-06-21: Wired the partial source-derived assets into the native Y-up
  forge scene and pack builder. Rebuilt, smoke-tested, and recaptured
  `state_town_lantern_upgrade.png` / `state_town_lantern_forged.png`.
  Product gate remains REVIEW, not PASS: the screen is no longer panel-only,
  but final acceptance still needs clean full forge/workbench and standalone
  lantern source assets.
- 2026-06-21: Repaired the core forge/workbench and standalone lantern source
  crops via explicit key-matte chroma-hole handling, moved normalized sources
  into `gamedesign/projects/ember-road/art/processed/`, and added
  `forge_workshop_v2`, `forge_worktable_v2`, and
  `mine_lantern_standalone_v2` to the native pack/scene. Source derivation now
  passes:
  `gamedesign/projects/ember-road/reviews/ember-road-town-forge-v2-source_derivation_audit.md`.
  Product gate remains REVIEW because the rail/text hierarchy still needs
  polish or lead acceptance.
- 2026-06-20: product gate REVIEW (town-forge-native-v2); review: gamedesign/projects/ember-road/reviews/T0024_town_forge_native_scene_anchor_review.md; screenshot: build/captures/ember-road/state_town_lantern_upgrade.png; next: Polish the right rail and label hierarchy around the source-derived forge scene, then rerun strict product gate or request lead acceptance of this direction.
- 2026-06-20: product gate REVIEW (town-forge-native-v2); review: gamedesign/projects/ember-road/reviews/T0024_town_forge_native_scene_anchor_review.md; screenshot: build/captures/ember-road/state_town_lantern_upgrade.png; next: Ask for lead acceptance on the refreshed native forge screenshots or continue with another visual-only correction if rejected.
- 2026-06-21: Polished the native forge rail/text hierarchy in
  `src/clean_seed_main.c`: reduced in-scene labels to one forge/result callout,
  replaced the dense rail progress block with the source-derived
  `forge_result_strip_slice9_v2`, used the source-derived Mine Lantern/badge in
  the rail portrait/result slot, and tightened long primary button labels.
  Rebuilt, smoke-tested, refreshed captures, and reran strict product gate:
  `gamedesign/projects/ember-road/reviews/T0024_town_forge_native_scene_anchor_review.md`.
  Scores are now 4/5 across strict visual axes, but verdict remains REVIEW
  because lead acceptance is still pending before Depth 2/content expansion.
- 2026-06-21: Added and ran a native Y-up layout audit:
  `tools/ember-road/audit_y_up_layout.py` writes
  `gamedesign/projects/ember-road/reviews/T0024_town_forge_y_up_layout_audit.md`.
  It passes source-boundary checks, converts `ui.tree` screen rectangles back
  into logical Y-up rectangles, verifies the forge is above the route strip and
  left of the rail action, and confirms `ui.click` on
  `ember.scene.forge_workbench` forges the Mine Lantern and unlocks Depth 2.
- 2026-06-21: Added lead review evidence without expanding gameplay:
  `tools/ember-road/build_lead_acceptance_montage.py` builds
  `gamedesign/projects/ember-road/reviews/T0024_town_forge_lead_acceptance_moment.png`,
  and
  `gamedesign/projects/ember-road/reviews/T0024_town_forge_lead_acceptance_packet.md`
  asks for acceptance or a named replacement target. T0024 remains REVIEW
  until the lead accepts the direction.
- 2026-06-21: Made Y-up a standard validation gate for active Ember Road work:
  `node tools/ai.mjs validate` now runs
  `tools/ember-road/audit_y_up_layout.py --port 9124` after the visual
  invariant guard. The current pass proves the forge, route strip, rail action,
  DevAPI rectangles, and `ui.click` still round-trip through logical Y-up
  coordinates.
- 2026-06-21: Added the source-derived `forge_action_panel_v2` as an in-scene
  forge action plaque in `src/clean_seed_main.c`, refreshed live-state
  captures, regenerated
  `gamedesign/projects/ember-road/reviews/T0024_town_forge_lead_acceptance_moment.png`,
  reran the strict REVIEW product gate, and extended the Y-up audit source
  evidence to include `FORGE_ACTION_PANEL_V2`. Verdict remains REVIEW pending
  lead acceptance.
- 2026-06-21: Tightened the forge screen player-read labels without expanding
  content: the town forge state now renders `Old Gate Town Forge` /
  `Mine Lantern Upgrade` (or `Mine Lantern Equipped`) instead of the generic
  `Old Gate Town Square` / `Old Gate Quest Hub`. Captures, smoke, Y-up audit,
  montage, and strict REVIEW gate were refreshed.
- 2026-06-21: Lead stopped the current game iteration for review. No feature
  expansion was resumed; the latest native town-forge proof remains REVIEW
  pending acceptance or a replacement target.
