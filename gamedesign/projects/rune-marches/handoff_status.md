---
type: Handoff Status
title: Rune Marches Handoff Status
description: Current gate, blockers, and source order for implementation.
tags: [handoff, status]
timestamp: 2026-06-13T00:00:00Z
---

# Rune Marches Handoff Status

## Status

`partial`: first design gate, native playable slice, generated runtime asset
pass, side quest choice, Old Bell Tower inspect action, Reedmere Crossing
second road, Moss Shrine kindness payoff, Greenfen Causeway route hook, first
Greenfen combat beat, Spark Ward II lore sink, Briar Gate/Moonwell route
choice, first chosen-route encounters, branch landmark discovery, reward chip,
and journal panel exist. The first XP-to-level payoff now exists as Warden Rank
II. The reference evidence board is still not strong enough to claim final
UI/art/economy readiness.

## Active Goal

Create a casual Skyrim-like original fantasy RPG for PC and web/mobile, then
prepare it for Poki-audience testing. The project must cover visual direction,
gameplay, balance, and FTUE.

## Current Decision

First playable slice is `Miregate -> Wispfen Road -> Mire Wisp -> bell rope
side choice -> Spark Ward I -> Old Bell Tower inspect -> Reedmere Crossing ->
Reed Raider -> optional Moss Shrine blessing -> Greenfen Causeway route hook ->
Fen Shade -> Warden Rank II -> Spark Ward II -> Briar Gate or Moonwell ->
Briar Stalker or Moonwell Sentinel -> Ashen Cairn or Starfall Grotto -> journal
endpoint`.

## Implementation Readiness

Ready:

- concept and pillars;
- first 30 seconds and first 5 minutes;
- first encounter numbers;
- first upgrade cost/effect;
- UI flow and DevAPI node intent;
- native-first validation approach;
- native implementation in `src/main.c`, `src/game_state_actions.*`, and
  `state/game_state.schema.json`;
- generated runtime asset pass in `assets/runtime/rune-marches-v1/`;
- passive-profiled validation scenario for side quest choice, tower inspect,
  Reedmere Crossing, Moss Shrine, Greenfen Causeway, first Greenfen beat,
  Warden Rank II, Spark Ward II, route choice, first chosen-route encounters,
  branch landmark discovery, and save/load.

Not ready:

- final art;
- final UI styling;
- deep reference/pacing claims;
- Poki submission/readiness claims;
- web/mobile runtime proof.

## Current Blockers

- Reference deconstruction needs actual gameplay frames or timestamped raw
  gameplay observations before final art/UI/economy pacing.
- Web/mobile proof is intentionally paused after user questioned the web-build
  lane; native PC remains the current harness.
- Runtime art is first-pass cropped/generated art, not final transparent source
  sheets or compressed web pack.

## Next Action

Continue native PC FTUE/content expansion with the first Ashen Cairn or
Starfall Grotto playable content beat, then finish the reference evidence gap
before claiming final UI/art/economy readiness. Resume web/mobile only when the
user wants that lane active again.

## Latest Evidence

- Build: `cmake --build --preset native-debug`
- Compatibility smoke: `py -3.12 tools/devapi/smoke_test.py 9123`
- Full probe: `py -3.12 tools/devapi/full_probe.py 9123`
- Rune scenario: `py -3.12 tmp/rune_marches_scenario.py`
- Desktop screenshot: `tmp/rune_marches/native_first_slice_labeled.png`
- Portrait screenshot: `tmp/rune_marches/native_first_slice_portrait_current.png`
- Moss Shrine desktop screenshot:
  `tmp/rune_marches/native_moss_shrine_labeled.png`
- Moss Shrine portrait screenshot:
  `tmp/rune_marches/native_moss_shrine_portrait.png`
- Greenfen Causeway desktop screenshot:
  `tmp/rune_marches/native_greenfen_causeway_labeled.png`
- Greenfen Causeway portrait screenshot:
  `tmp/rune_marches/native_greenfen_causeway_portrait.png`
- First Greenfen beat desktop screenshot:
  `tmp/rune_marches/native_greenfen_beat_labeled.png`
- First Greenfen beat portrait screenshot:
  `tmp/rune_marches/native_greenfen_beat_portrait.png`
- Spark Ward II desktop screenshot:
  `tmp/rune_marches/native_spark_ward_2_labeled.png`
- Spark Ward II portrait screenshot:
  `tmp/rune_marches/native_spark_ward_2_portrait.png`
- Warden Rank II desktop screenshot:
  `tmp/rune_marches/native_warden_rank_2_labeled.png`
- Warden Rank II portrait screenshot:
  `tmp/rune_marches/native_warden_rank_2_portrait.png`
- Reward chip desktop screenshot:
  `tmp/rune_marches/native_reward_chip_labeled.png`
- Reward chip portrait screenshot:
  `tmp/rune_marches/native_reward_chip_portrait.png`
- Route choice desktop screenshot:
  `tmp/rune_marches/native_route_choice_labeled.png`
- Route choice portrait screenshot:
  `tmp/rune_marches/native_route_choice_portrait.png`
- Route encounter desktop screenshot:
  `tmp/rune_marches/native_route_encounter_labeled.png`
- Route encounter portrait screenshot:
  `tmp/rune_marches/native_route_encounter_portrait.png`
- Branch landmark desktop screenshot:
  `tmp/rune_marches/native_branch_landmark_labeled.png`
- Branch landmark portrait screenshot:
  `tmp/rune_marches/native_branch_landmark_portrait.png`
- Generated visual direction:
  `gamedesign/projects/rune-marches/art/fake_shots/rune-marches-gameplay-v1.png`
- Passive AI profile scope: `T0003/post-greenfen-route-choice` in
  `tmp/session_profiles/session_profile_2026-06-13.jsonl`.
