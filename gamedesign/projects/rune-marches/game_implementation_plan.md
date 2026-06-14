---
type: Implementation Plan
title: Rune Marches First Playable Implementation Plan
description: Build handoff for the native-first casual RPG slice.
tags: [implementation, native, web, mobile, validation]
timestamp: 2026-06-13T00:00:00Z
---

# Rune Marches First Playable Implementation Plan

## Source-Of-Truth Order

1. `AGENTS.md`
2. `gamedesign/projects/rune-marches/handoff_status.md`
3. `gamedesign/projects/rune-marches/concept.md`
4. `gamedesign/projects/rune-marches/gdd.md`
5. `gamedesign/projects/rune-marches/data/balance.json`
6. `gamedesign/projects/rune-marches/data/combat.json`
7. `gamedesign/projects/rune-marches/data/ui_flow.json`
8. `gamedesign/projects/rune-marches/data/asset_manifest.json`
9. `gamedesign/projects/rune-marches/references/reference_deconstruction.md`

## First Playable Slice Packet

- Player starts at: Miregate.
- Screen shown first: static map with Miregate, Wispfen Road, locked Old Bell
  Tower, top status bar, objective, and Scout Road action.
- Available actions: Scout Road, Strike, Spark, Guard, Retreat, Rest, buy
  Spark Ward I, choose bell rope reward, inspect Old Bell Tower, Scout East,
  light Moss Shrine, open Greenfen Causeway, Scout Greenfen, study rune lore,
  choose Briar Gate, choose Moonwell, Scout Briar Gate, Moonwell Trial, map
  Ashen Cairn, map Starfall Grotto.
- Currencies/stats: HP, mana, silver, XP, rune sparks, ward rank, spell level.
- First activity/job: Road Scout.
- First reward: 6 silver, 4 XP, first-win rune spark.
- First upgrade: Spark Ward I, cost 12 silver and 1 rune spark.
- First enemy/obstacle: Mire Wisp.
- Player actions and exact effects: from `data/combat.json`.
- Enemy/check actions and exact effects: from `data/combat.json`.
- Win/loss/retreat outcomes: from `data/combat.json`.
- Recovery path: Miregate Rest.
- First visual/status change: Wispfen Road safety increases; Old Bell Tower
  unlocks after Spark Ward I.
- First side choice: after the second road win, recover the bell rope charm and
  choose +6 silver or +1 kindness reputation.
- First main-quest endpoint: inspect Old Bell Tower after Spark Ward I; the
  bell points east and unlocks Reedmere Crossing.
- Second road endpoint: defeat the first Reed Raider and show `Reedmere is
  open` in the journal.
- Optional side payoff: if the rope was returned for kindness, light Moss
  Shrine after Reedmere and show `FAVOR 1`.
- Route hook endpoint: spend favor to open Greenfen Causeway and show `PASS 1`.
- First Greenfen beat: scout Greenfen, defeat Fen Shade, and show `GREEN 1`
  plus `LORE 1`.
- First level-up: total XP reaches 20 around Greenfen, changes the top bar to
  `LVL 2`, raises HP max to 24, and restores HP.
- First lore sink: study rune lore to unlock Spark Ward II, show `WARD II`,
  `SPARK DMG 9`, and main progress `13/13`.
- Post-Greenfen route choice: choose `Briar Gate` or `Moonwell`, show the
  chosen endpoint and main progress `14/14`.
- First chosen-route beat: Briar Gate starts a Briar Stalker fight; Moonwell
  starts a Moonwell Sentinel trial; both advance main progress to `16/16`.
- First branch landmark discovery: Briar Gate reveals `Ashen Cairn`; Moonwell
  reveals `Starfall Grotto`; both advance main progress to `18/18`.
- Journal expectation: main, side, east-road, spell damage, kindness, and
  route/landmark status are visible in native desktop and portrait layouts.
- Save/load expectation: progress persists through existing JSON save path.
- Out of scope: full inventory, full faction system, final art, procedural
  world, monetization, Poki submission.

## Phases

1. Data/state schema
   - Add Rune Marches state fields for location, HP, mana, silver, XP, sparks,
     quest step, road safety, enemy HP, encounter state, and upgrade state.
2. Core loop actions
   - Replace template cycle action with scout/combat/rest/upgrade reducers.
3. Native UI and shape visuals
   - Draw map, status bars, encounter panel, action buttons, locked/unlocked
     landmark, and reward meter using shape renderer.
4. DevAPI automation
- Register UI nodes and endpoints for reset, scout, combat actions, rest,
     upgrade, side quest reward choice, tower inspect, east scout, Moss Shrine,
     Greenfen Causeway, Greenfen scout, Spark Ward II, route choice, telemetry,
     screenshot capture, and state.
5. Persistence
   - Confirm save/load keeps first-slice progress.
6. Web/mobile follow-up
   - Paused until the user reactivates this lane. Before any web server,
     localhost, browser build, or frontend tooling work, restate the explicit
     permission that allows it.

## Acceptance Gates

- Player can scout Wispfen Road from a fresh state.
- First scout opens a Mire Wisp encounter.
- Spark reduces enemy HP and costs mana.
- Winning changes visible silver, XP, rune sparks, and road safety.
- Spark Ward I cannot be bought before cost is met and can be bought after.
- Buying Spark Ward I changes spell damage and unlocks Old Bell Tower.
- Returning or trading the bell rope charm changes persistent side quest state.
- Inspecting Old Bell Tower after unlock advances the main quest endpoint and
  persists through save/load.
- Reedmere Crossing unlocks after tower inspect, starts a Reed Raider
  encounter, grants a distinct reward, and persists east-road safety.
- Journal panel exposes main/side/east-road progress and endpoint text.
- Reward chip exposes the latest concrete reward or upgrade payoff and is
  visible in desktop and portrait screenshots.
- Moss Shrine unlocks only from kindness + Reedmere clear, grants spirit favor,
  and persists through save/load.
- Greenfen Causeway opens from spirit favor, advances main progress to 10/10,
  and persists through save/load.
- Greenfen scout opens Fen Shade, rewards silver/XP/lore, advances main
  progress to 12/12, and persists through save/load.
- Reaching 20 XP auto-promotes the player to Warden Rank II, raises HP max to
  24, restores HP, and persists through save/load.
- Studying rune lore consumes `rune_lore`, unlocks Spark Ward II, raises spark
  damage to 9, raises max mana to 14, advances main progress to 13/13, and
  persists through save/load.
- Choosing Briar Gate or Moonwell after Spark Ward II advances main progress to
  14/14, persists through save/load, and primary action keeps the selected
  route instead of returning to Greenfen.
- Scouting Briar Gate or starting the Moonwell Trial after route choice opens a
  route-specific encounter, rewards the branch, advances main progress to
  16/16, records route-clear telemetry, and persists through save/load.
- Mapping Ashen Cairn or Starfall Grotto after the first branch beat unlocks a
  route-specific next landmark, advances main progress to 18/18, records
  landmark-open telemetry, and persists through save/load.
- Loss or retreat returns the player to Miregate with recoverable state.
- Restart/save behavior preserves player progress.
- Native screenshot is nonblank and clearly shows map, status, primary action,
  encounter or upgrade state.
- Emulated input proves the main action path.

## Commands

- Build: `cmake --build --preset native-debug`
- State codegen if schema changes: part of the build through CMake, or run
  `python tools/state_codegen/generate_state.py` only if diagnosing codegen.
- Native run: `build/game_seed/native-debug/game_seed.exe --devapi --fresh-state`
- Smoke/probe: `python tools/devapi/smoke_test.py --port 9123`
- Full probe candidate: `python tools/devapi/full_probe.py --port 9123`
- Screenshot proof: DevAPI endpoint `game.capture.framebuffer` to
  `tmp/rune_marches/native_first_slice.ppm`
- Web build: paused; discover exact preset from `CMakePresets.json` only after
  the user reactivates the web/mobile lane.

## Next-Chat Prompt

Use the project rules in `AGENTS.md`. Implement the native first playable slice
from:

1. `gamedesign/projects/rune-marches/handoff_status.md`
2. `gamedesign/projects/rune-marches/gdd.md`
3. `gamedesign/projects/rune-marches/data/balance.json`
4. `gamedesign/projects/rune-marches/data/combat.json`
5. `gamedesign/projects/rune-marches/data/ui_flow.json`

Goal:
Replace the template shape-cycle gameplay with the Rune Marches first slice:
Miregate map, Wispfen Road scout, Mire Wisp turn/check combat, Spark Ward I,
Old Bell Tower, Reedmere, Moss Shrine, Greenfen, Spark Ward II persistence,
Briar Gate/Moonwell route choice, first chosen-route encounters, branch
landmark discovery, DevAPI automation, and native screenshot proof.

Scope:

- Must implement: state schema, reducers, native shape UI, DevAPI nodes and
  endpoints, save/load, native validation.
- Out of scope: final art, full world, full quest journal, web/mobile proof
  until native proof passes.

Validation:

- Run `cmake --build --preset native-debug`.
- Run a fresh-state native DevAPI scenario for scout -> spark -> win ->
  upgrade.
- Capture native screenshot to `tmp/rune_marches/native_first_slice.ppm`.

Do not edit:

- `external/neotolis-engine`
- raw/temp folders except `tmp/`
- `.codex/skills` unless updating reusable workflow intentionally.
