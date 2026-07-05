---
id: T0278
title: "rb-dark-rpg: world map movement and location story layer"
status: review
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, story, world-map, locations, quests, state]
created: 2026-07-04
updated: 2026-07-04
---

## What

Build the story/world traversal layer for `rb-dark-rpg`, separate from combat:
Legend-like region map data, clickable location movement, and per-location
content surfaces with NPCs, quest objects, enemies, and exits.

First vertical slice: establish the data/state/action contract so UI can open a
map and move the hero by location id without hardcoding scene-specific state.

## Done when

- [x] `locations.json` is treated as authored world content: location ids,
      display names, objects, exits, requirements, and map metadata are exposed
      through generated runtime content.
- [x] Player state stores current location and visited locations with stable
      schema ids.
- [x] A domain action moves the hero to an authored location only when the
      location exists and requirements pass.
- [x] Moving to a location records the visit and can advance authored
      `visit_location` quest steps.
- [x] The first world slice supports `hub_last_post`,
      `hub_gate_outskirts`, and `old_mill` as data, not UI hardcode.
- [x] Unit coverage proves valid move, invalid location rejection, locked
      location rejection, idempotent visits, and quest visit advancement.
- [x] Bottom nav `Карта` opens a runtime map overlay driven by generated
      locations, not hardcoded UI ids.
- [x] Bottom nav `Место` opens a runtime current-location overlay with authored
      objects and exits.
- [x] Map/place overlays hide the tutorial/nav layer while open and keep combat
      start as a separate follow-up slice.
- [x] Native build succeeds.

## Open questions

- Full illustrated regional map art, path lines, node art, and actual
  per-location scene backgrounds are follow-up slices after this runtime overlay.
- Combat start/result remains owned by the combat agent; this task may expose
  combat objects as location content but must not implement battle.

## Log

- 2026-07-05: Resolved runtime blocker. UI runtime now feeds all DevAPI synthetic pointer slots into Clay, bottom nav art is visual-only under semantic slot widgets, and scene interactions are gated while modal UI overlays are open so Place clicks cannot leak into the guard hotspot below. Reworked q002 scenario to stay inside the current Place overlay instead of reopening hidden nav. Evidence passed: `cmake --build games\rb-dark-rpg\build\native-debug --target game`, responsive `prepare_q002_elder_contract_flow` at `960x540` + `390x844` with summary `tmp/rb_dark_rpg_q002_elder_contract_flow/summary.json`, `py -3.12 -m unittest games.rb-dark-rpg.devapi.scenarios_test`, `world_story_test`, `game_dialogue_test`, `game_combat_test`, and `git diff --check` (CRLF warnings only).
- 2026-07-05: Started after direction change: combat stays with another agent;
  this task owns story/world map/movement foundation.
- 2026-07-05: First vertical foundation complete and moved to review. Added
  generated runtime location registry from `locations.json`, persisted
  `world.current_location_id` and `world.visited_location_ids`, transactional
  `game_actions_move_location`, q002 `visit_old_mill` authored step, and
  `world_story_test`. Deep-reasoner reviewed the boundary: keep combat separate,
  use `locations.json` exits as v1 paths, and defer full map UI. Evidence:
  `generate_state_test.py`, `world_story_test`, `game_dialogue_test`,
  `scene_interactions_test`, `game_combat_test`, `game` build, and
  `git diff --check` passed; diff-check only reported CRLF warnings in existing
  edited files.
- 2026-07-05: Added first runtime UI slice. `game_content` now exposes
  `game_content_location_count/at` and location objects from `locations.json`.
  `game_actions` exposes read-only unlock/can-move predicates for UI.
  Added `world_map_screen.c`: `Карта` lists authored locations and moves to
  available ones; `Место` shows current location objects and exits without
  starting combat. Updated top HUD location title from `world.current_location_id`.
  Evidence: `world_story_test`, `game_dialogue_test`, `game_combat_test`,
  `scene_interactions_test`, `generate_state_test.py`, `game` build, and
  runtime `--capture` smoke at `960x540` and `390x844` all passed. Capture files
  were temporary and removed; `git diff --check` passed with only CRLF warnings.
- 2026-07-04: Continue story/world slice: add data-driven location object interactions for dialogue, inspect, and combat handoff.
- 2026-07-04: Added data-driven location object interactions: generated inspect_object quest steps, domain inspect action, object availability checks from requirements, and runtime map/place rows now open dialogue, inspect quest hotspots, or hand off combat objects to existing combat flow. Evidence: world_story_test, game_dialogue_test, game_combat_test, scene_interactions_test, game build, capture smoke 960x540 and 390x844, git diff --check. Live DevAPI click smoke was attempted but background native launch exits before port opens in this shell; foreground runtime with --devapi stays alive.
- 2026-07-04: Continue story/world slice: add data-driven visual map node layout.
- 2026-07-04: Implemented data-driven visual world map graph: locations.json map x/y -> generated content fields -> Clay graph nodes/exit edges; verified world_story_test, game_dialogue_test, game_combat_test, scene_interactions_test, game build, 960x540 and 390x844 capture smoke, git diff --check.
- 2026-07-04: Added DevAPI-stable semantic ids for world travel UI: world_map/node/<location_id>, world_place/object/<object_id>, world_place/exit/<target_location_id>. Added prepare_world_map_move_gate scenario and scenario unit test. Evidence: py -3.12 -m unittest games.rb-dark-rpg.devapi.scenarios_test passed; responsive_viewports scenario passed for 960x540 and 390x844 with state=world_map_move_gate and location=hub_gate_outskirts; summary tmp/rb_dark_rpg_world_map_move_gate/summary.json; git diff --check passed with only pre-existing CRLF warnings.
- 2026-07-04: Added and verified old-mill quest-object scenario: prepare_old_mill_inspect_mark opens Place, taps semantic object world_place/object/old_mill.black_sun_mark, verifies completed_step_ids include inspect_old_mill and q002_bread_for_post reaches ready_to_turn_in. Evidence: py -3.12 -m unittest games.rb-dark-rpg.devapi.scenarios_test passed; responsive_viewports scenario passed for 960x540 and 390x844; summary tmp/rb_dark_rpg_old_mill_inspect_mark/summary.json; git diff --check passed with only pre-existing CRLF warnings.
- 2026-07-05: Added generic data-driven location interaction selection. `locations.json` can now put requirements on individual object interactions; generated runtime content preserves all interactions instead of collapsing to the first one. `game_actions_select_location_interaction` chooses the first interaction whose requirements match state, and Place UI opens that selected interaction. Guard now opens `dlg_gate_guard_turn_in` when `q001_gate_pass` is `ready_to_turn_in`, otherwise falls back to intro. Added DevAPI scenario `prepare_gate_guard_turn_in_from_place`. Evidence: `world_story_test`, `game_dialogue_test`, `game` build, `py -3.12 -m unittest games.rb-dark-rpg.devapi.scenarios_test`, and responsive runtime scenario at `960x540` + `390x844` passed; summary `tmp/rb_dark_rpg_gate_turn_in_place/summary.json`.
- 2026-07-05: Continued q002 story slice after guard: added data-driven Староста/elder interaction in `hub_last_post`, q002 accept/in-progress/turn-in/completed dialogue ids, authored rewards from dialogue data, `quest_step` location requirements, and old-mill inspect now advances q002 to `report_to_elder` instead of directly ready/completed. Evidence passed: `world_story_test`, `game_dialogue_test`, `game_combat_test`, `game` build/content compatibility validation, `py -3.12 -m unittest games.rb-dark-rpg.devapi.scenarios_test`, and `git diff --check` (CRLF warnings only). Runtime responsive q002/guard replay is blocked by a current bottom-nav hit-test issue: DevAPI can resolve and queue `ui.click("bottom_nav/slot/place")`, but Place does not open in native runtime; last failed evidence log uses `tmp/rb_dark_rpg_gate_turn_in_recheck_one`.
