---
id: T0003
title: Native first playable RPG slice
status: review
epic: E001
priority: P0
tags: [native, gameplay, state, devapi, ftue]
created: 2026-06-13
updated: 2026-06-13
---

## What

Replace the native `Game Seed` template with the Rune Marches first playable
slice: Miregate map, Wispfen Road scout, Mire Wisp encounter, Spark Ward I
upgrade, Old Bell Tower unlock, Reedmere Crossing second road, quest journal,
Moss Shrine kindness payoff, Greenfen Causeway route hook, persistence, and
  first Greenfen combat beat, Warden Rank II level-up, Spark Ward II lore sink,
  post-Greenfen Briar Gate/Moonwell route choice, first chosen-route
  encounters, branch landmark discovery, reward clarity chip, persistence, and
  DevAPI automation.

## Done when

- [x] Fresh state opens on a readable Rune Marches map/status screen.
- [x] Scout Road starts the first Mire Wisp encounter.
- [x] Strike, Spark, Guard, Retreat, Rest, and Spark Ward I actions update
  persistent state according to the design JSON contracts.
- [x] Winning grants visible silver, XP, rune spark, and road safety progress.
- [x] Spark Ward I is cost-gated, purchasable, changes spell damage, and
  unlocks Old Bell Tower.
- [x] Side quest choice after the second road fight resolves to either silver
  or kindness reputation.
- [x] Old Bell Tower has a native inspect action after unlock and advances the
  main quest endpoint.
- [x] Reedmere Crossing unlocks after tower inspect, starts a Reed Raider
  encounter, grants a distinct reward, and persists east-road safety.
- [x] Native journal panel shows main/side/east-road progress, spell damage,
  kindness, and the current endpoint hook.
- [x] Returning the bell rope for kindness unlocks an optional Moss Shrine
  blessing after Reedmere is cleared, granting spirit favor and ward progress.
- [x] Spirit favor opens Greenfen Causeway as a visible next-route hook, updates
  main progress to 10/10, and persists through save/load.
- [x] Greenfen scout starts a Fen Shade encounter, rewards silver/XP/lore,
  advances main progress to 12/12, and persists through save/load.
- [x] Reaching 20 XP auto-promotes the player to Warden Rank II, raises HP max
  to 24, restores HP, and persists through save/load.
- [x] Studying rune lore unlocks Spark Ward II, raises Spark damage and max
  mana, advances main progress to 13/13, and persists through save/load.
- [x] Choosing Briar Gate or Moonwell after Spark Ward II advances main
  progress to 14/14, persists through save/load, updates the route button, and
  records route-choice telemetry.
- [x] Briar Gate starts a Briar Stalker encounter and Moonwell starts a
  Moonwell Sentinel trial after route choice; both advance main progress to
  16/16, persist through save/load, and record route-clear telemetry.
- [x] After the first branch beat, Briar Gate reveals Ashen Cairn and Moonwell
  reveals Starfall Grotto; both advance main progress to 18/18, persist through
  save/load, and record landmark-open telemetry.
- [x] Latest reward/upgrade payoff is shown as a readable native reward chip
  and exposed as `reward.last` / `rune.reward_text`.
- [x] DevAPI exposes state, reset, scout, combat, rest, upgrade, UI tree, and
  screenshot proof for automation.
- [x] Native build and scenario validation pass, with screenshot evidence under
  `tmp/rune_marches/`.

## Open questions

- Should the first runtime pass use only shape-renderer visuals, or should a
  gameplay fake shot be generated first for art direction review?

## Log

- 2026-06-13: Backlog task created from
  `gamedesign/projects/rune-marches/game_implementation_plan.md`.
- 2026-06-13: Implemented placeholder native Rune Marches slice in
  `state/game_state.schema.json`, generated state files,
  `src/game_state_actions.*`, and `src/main.c`. Evidence: `cmake --build
  --preset native-debug`, `py -3.12 tools/devapi/smoke_test.py 9123`,
  `py -3.12 tools/devapi/full_probe.py 9123`, and `py -3.12
  tmp/rune_marches_scenario.py`; screenshot
  `tmp/rune_marches/native_first_slice.png`.
- 2026-06-13: Improved native readability with block-font labels, corrected
  shape rect coordinate semantics, and added portrait-aware layout. Evidence:
  `cmake --build --preset native-debug`, `py -3.12 tools/devapi/smoke_test.py
  9123`, `py -3.12 tools/devapi/full_probe.py 9123`, desktop screenshot
  `tmp/rune_marches/native_first_slice_labeled.png`, and portrait screenshot
  `tmp/rune_marches/native_first_slice_portrait_current.png`.
- 2026-06-13: Added first native quest expansion: second road win can find the
  bell rope charm, player can choose `Take 6` or `Give Rope`, kindness
  reputation persists, and unlocked Old Bell Tower can be inspected to reach
  `QUEST 6/6` and reveal the next-road hook. Evidence was captured through
  passive-profiled native build, smoke/full probes, and
  `tmp/rune_marches_scenario.py` desktop + portrait screenshots.
- 2026-06-13: Added Reedmere Crossing as the second-road native expansion:
  tower inspect unlocks the east road, `Scout East` starts a Reed Raider
  encounter, victory grants +8 silver/+6 XP and east-road safety, and the
  journal panel exposes `MAIN 8/8`, `SIDE 3/3`, `EAST 1`, `KINDNESS`, and
  endpoint text. Evidence: passive-profiled native build, smoke/full probes,
  desktop screenshot `tmp/rune_marches/native_first_slice_labeled.png`, and
  portrait screenshot
  `tmp/rune_marches/native_first_slice_portrait_current.png`.
- 2026-06-13: Added Moss Shrine optional side objective as the first gameplay
  payoff for the kindness choice. After Reedmere is cleared, `BLESSING` /
  `game.rune.light_moss_shrine` lights the shrine, grants `spirit_favor +1`,
  adds ward progress, restores mana to current max, persists through save/load,
  and shows `FAVOR 1` in the journal. Evidence: passive-profiled native build,
  smoke/full DevAPI probes, desktop screenshot
  `tmp/rune_marches/native_moss_shrine_labeled.png`, and portrait screenshot
  `tmp/rune_marches/native_moss_shrine_portrait.png`.
- 2026-06-13: Added Greenfen Causeway as the post-`FAVOR 1` route hook. The
  primary action becomes `OPEN PASS`, `game.rune.open_causeway` unlocks the
  new map marker, sets `causeway_safety` to 1, advances main progress to
  `10/10`, persists through save/load, and leaves the next-region endpoint
  visible as `CAUSEWAY`. Evidence: passive-profiled native build, smoke/full
  DevAPI probes, desktop screenshot
  `tmp/rune_marches/native_greenfen_causeway_labeled.png`, and portrait
  screenshot `tmp/rune_marches/native_greenfen_causeway_portrait.png`.
- 2026-06-13: Added the first Greenfen gameplay beat. `GREENFEN` /
  `game.rune.scout_greenfen` starts a Fen Shade encounter; victory grants +10
  silver, +8 XP, `greenfen_safety +1`, and `rune_lore +1`, advances main
  progress to `12/12`, persists through save/load, and keeps the portrait
  journal compact with `S/E/G/L` progress. Evidence: passive-profiled native
  build, smoke/full DevAPI probes, desktop screenshot
  `tmp/rune_marches/native_greenfen_beat_labeled.png`, and portrait screenshot
  `tmp/rune_marches/native_greenfen_beat_portrait.png`.
- 2026-06-13: Added Spark Ward II as the first Greenfen lore sink. `STUDY` /
  `game.rune.upgrade.spark_ward_2` consumes `rune_lore`, raises Spark damage
  to 9, raises max mana to 14, advances main progress to `13/13`, persists
  through save/load, and leaves `WARD II` visible in desktop and portrait UI.
  Evidence: passive-profiled native build, smoke/full DevAPI probes, desktop
  screenshot `tmp/rune_marches/native_spark_ward_2_labeled.png`, and portrait
  screenshot `tmp/rune_marches/native_spark_ward_2_portrait.png`.
- 2026-06-13: Added the first visible XP level-up. Total XP now triggers
  Warden Rank II at 20 XP, raises max HP to 24, restores HP on the level-up,
  exposes `player_level` and `hp_max` through DevAPI, and persists through
  save/load. Evidence: passive-profiled native build, smoke/full DevAPI probes,
  configurable-port desktop + portrait scenarios, desktop screenshot
  `tmp/rune_marches/native_warden_rank_2_labeled.png`, and portrait screenshot
  `tmp/rune_marches/native_warden_rank_2_portrait.png`.
- 2026-06-13: Added native reward clarity chip. Rewards and upgrade payoffs now
  set `rune.reward_text`, the journal renders a readable reward chip, DevAPI
  exposes `reward.last`, and the scenario verifies first reward, Warden Rank II,
  and Spark Ward II chip text. Evidence: passive-profiled native build,
  smoke/full DevAPI probes, playtest probe, desktop screenshot
  `tmp/rune_marches/native_reward_chip_labeled.png`, and portrait screenshot
  `tmp/rune_marches/native_reward_chip_portrait.png`.
- 2026-06-13: Added post-Greenfen route choice. After Spark Ward II, `BRIAR
  GATE` / `game.rune.choose_briar_gate` marks the main route and `MOONWELL` /
  `game.rune.choose_moonwell` marks the side oath route with +1
  `moonwell_blessing`; both advance main progress to `14/14`, persist through
  save/load, expose `rune_route_post_greenfen_choice`, and keep primary action
  on the selected endpoint. Also raised local DevAPI endpoint capacity from 48
  to 96 so `game.capture.framebuffer` remains registered as screenshot proof.
  Evidence: passive-profiled native build, smoke/full DevAPI probes, desktop
  screenshot `tmp/rune_marches/native_route_choice_labeled.png`, portrait
  screenshot `tmp/rune_marches/native_route_choice_portrait.png`, playtest
  probe `tmp/rune_marches/playtest_probe_report.json`, and taskboard validate.
- 2026-06-13: Added first chosen-route encounters. After choosing Briar Gate,
  `BRIAR FIGHT` / `game.rune.scout_briar_gate` starts a Briar Stalker fight;
  after choosing Moonwell, `MOON TRIAL` / `game.rune.scout_moonwell` starts a
  Moonwell Sentinel trial. Wins advance main progress to `16/16`, update
  branch safety/blessing state, emit `rune_route_briar_clear` /
  `rune_route_moonwell_clear`, and persist through save/load. Evidence:
  passive-profiled native build, smoke/full DevAPI probes, desktop screenshot
  `tmp/rune_marches/native_route_encounter_labeled.png`, portrait screenshot
  `tmp/rune_marches/native_route_encounter_portrait.png`, playtest probe
  `tmp/rune_marches/playtest_probe_report.json`, and taskboard validate.
- 2026-06-13: Added branch landmark discovery after the first route beat.
  `ASHEN MAP` / `game.rune.discover_ashen_cairn` unlocks Ashen Cairn after
  Briar Gate is safe; `STAR MAP` / `game.rune.discover_starfall_grotto`
  unlocks Starfall Grotto after Moonwell is calm. Both advance main progress to
  `18/18`, update map labels, emit `rune_route_ashen_cairn_open` /
  `rune_route_starfall_grotto_open`, and persist through save/load. Evidence:
  passive-profiled native build, smoke/full DevAPI probes, desktop screenshot
  `tmp/rune_marches/native_branch_landmark_labeled.png`, portrait screenshot
  `tmp/rune_marches/native_branch_landmark_portrait.png`, playtest probe
  `tmp/rune_marches/playtest_probe_report.json`, and taskboard validate.
