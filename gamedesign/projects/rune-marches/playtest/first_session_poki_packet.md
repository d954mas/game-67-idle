---
type: Playtest Packet
title: Rune Marches First-Session Poki Audience Packet
description: FTUE test script, success metrics, screenshots, survey, and telemetry map for the first casual audience validation.
tags: [poki, playtest, ftue, telemetry, mobile]
timestamp: 2026-06-13T00:00:00Z
---

# Rune Marches First-Session Poki Audience Packet

## Status

`native-runtime-hooks-ready`: this packet can be used against the current
native build to validate flow, wording, and runtime milestone telemetry. It is
not a Poki submission packet and does not reactivate web/mobile build work.

## Source Digest

Official Poki sources checked on 2026-06-13 are recorded in
`gamedesign/projects/rune-marches/sources/poki_platform_notes.md`.

Working constraints for this test:

- support desktop and mobile/tablet once web is active;
- keep canvas/fullscreen planning compatible with a 16:9 viewport;
- avoid external stores, links, third-party ads, and unapproved SDKs;
- test save behavior under restricted browser storage later;
- map FTUE telemetry to Poki SDK gameplay and custom measure events later;
- treat average play time over 3 minutes and at least 25% of players over 3
  minutes as early strong signals, not as guaranteed acceptance criteria.

## Test Goal

Find whether a casual player understands the first Rune Marches loop without
instruction:

```text
see map -> scout road -> use Strike/Spark -> get reward -> choose side reward
-> buy Spark Ward I -> unlock tower -> open Reedmere/Greenfen -> get Warden
Rank II -> study Spark Ward II -> choose Briar Gate or Moonwell -> clear the
first chosen-route beat -> map Ashen Cairn or Starfall Grotto
```

## Audience

First informal test:

- 8-12 players if recruited manually;
- casual web/mobile players, not RPG experts only;
- target age/tone assumption: broad teen-friendly fantasy, no mature content;
- session length: 5-7 minutes, stop earlier if player stalls hard.

Do not claim Poki Player Fit readiness from this sample. A later platform test
needs the web build, SDK integration, and a larger sample.

## Success Metrics

| Metric | Target | Evidence |
| --- | ---: | --- |
| First meaningful action | <= 5 seconds | `ftue_first_action` |
| First combat action | <= 20 seconds | `combat_action_used` |
| First reward understood | >= 80% answer correctly | survey Q1/Q2 |
| First reward time | <= 45 seconds | `ftue_first_reward` |
| Spark Ward I reached | >= 60% within 2 minutes | `upgrade_spark_ward_1` |
| Bell rope choice understood | >= 70% can explain tradeoff | survey Q3 |
| No hard stall | <= 20% stuck over 30 seconds | `stall_30s` |
| Warden Rank II reached | >= 35% within 5 minutes | `level_warden_rank_2` |
| Spark Ward II reached | >= 25% within 7 minutes | `upgrade_spark_ward_2` |
| Post-Greenfen route choice | >= 20% within 8 minutes | `rune_route_post_greenfen_choice` |
| First chosen-route beat clear | >= 15% within 9 minutes | `rune_route_briar_clear` or `rune_route_moonwell_clear` |
| First branch landmark mapped | >= 10% within 10 minutes | `rune_route_ashen_cairn_open` or `rune_route_starfall_grotto_open` |
| Desire to continue | average >= 4/5 | survey Q8 |
| Early playtime signal | avg > 3 minutes, >= 25% over 3 minutes | session timer |

## Test Script

### Internal Native Proxy

Use this before web/mobile work resumes.

1. Launch the native build fresh.
2. Give only this prompt: "Explore the road and make the map safer."
3. Do not explain controls unless the player is stuck for 30 seconds.
4. Observe the first input, first combat decision, first reward reaction, first
   upgrade attempt, and whether the player notices the next-route marker.
5. Stop after the first branch landmark, 10 minutes, or hard confusion.
6. Capture one desktop and one portrait screenshot at the final state.

### Desktop Browser Later

Run only after web build work is explicitly active again.

1. Use a fresh browser profile.
2. Test 960 x 540 or similar 16:9 viewport first.
3. Confirm the first action, combat buttons, top status, journal, and map are
   readable without zoom.
4. Confirm no external links/stores/ads/branding are present.
5. Confirm save/load behavior after a reload.

### Mobile Portrait Later

Run only after web build work is explicitly active again.

1. Use a 360 x 640 portrait viewport.
2. Confirm all primary touch targets are reachable without hover.
3. Confirm top status text does not overlap.
4. Confirm combat buttons and the primary map action remain in the lower half.
5. Confirm a player can reach first reward and first upgrade with touch input.

## Screenshot Checklist

- Fresh state: map, `Scout Road`, HP/MP/silver/XP visible.
- First combat: enemy HP, Strike/Spark/Guard/Retreat visible.
- First reward: silver/XP/spark changed and a readable reward chip appears.
- Spark Ward I: tower unlocked and spell damage increased.
- Bell rope choice: both reward buttons visible.
- Reedmere: east road visible and journal updated.
- Greenfen: causeway visible and first Fen Shade beat complete.
- Warden Rank II: `LVL 2` and `HP 24/24` visible.
- Spark Ward II: `WARD II`, `SPARK DMG 9`, and route choice visible.
- Route choice: `BRIAR GATE` or `MOONWELL`, reward chip, and `MAIN 14/14`
  visible.
- Route beat: Briar Stalker or Moonwell Sentinel cleared, branch safety/blessing
  visible, and `MAIN 16/16` visible.
- Branch landmark: `ASHEN` or `STARFALL`, reward chip, and `MAIN 18/18`
  visible.
- Portrait proof: no overlapping critical text on 360 x 640.

Current native proof paths:

- `tmp/rune_marches/native_branch_landmark_labeled.png`
- `tmp/rune_marches/native_branch_landmark_portrait.png`
- `tmp/rune_marches/playtest_probe_report.json`
- `tmp/rune_marches/playtest_probe.png`

## Telemetry Event Map

Machine-readable event definitions live in
`gamedesign/projects/rune-marches/data/playtest_telemetry.json`.

Current native endpoints:

- `game.rune.telemetry`: read session counters for each packet event.
- `game.rune.telemetry.reset`: clear counters and start a fresh session event.

Native automation probe:

```text
py -3.12 tools/playtest/rune_marches_probe.py --port 9125 --window-size 960x540 --output tmp/rune_marches/playtest_probe_report.json --screenshot tmp/rune_marches/playtest_probe.png
```

The probe writes a JSON report with milestone counts, first-frame timings, and
target checks for the automated proxy path. These timings are automation
coverage proof only; human audience timings still require testers.

Event naming pattern:

```text
rune_<area>_<action_or_milestone>
```

Native mapping uses DevAPI state and scenario checkpoints. Web mapping should
call Poki SDK gameplay events and custom measure events only after the web lane
is active.

## Survey

Ask immediately after the session:

1. What was your first goal?
2. What did you get after the first fight?
3. What was the bell rope choice asking you to decide?
4. Did the next place on the map feel clear?
5. Was combat too easy, too hard, or about right?
6. Could you read HP, mana, silver, XP, and the main quest?
7. Did the fantasy feel like exploring a bigger world?
8. From 1-5, how much do you want to continue?
9. What was the most confusing moment?
10. What would you tap/click next if the session continued?

## Decision Rules

- If first action exceeds 5 seconds for more than 30% of testers, simplify the
  first screen and primary button text before adding more content.
- If first reward is not understood by at least 80%, add reward animation or
  clearer reward chips before expanding the world.
- If Spark Ward I is missed by more than 40%, make the upgrade affordance more
  explicit.
- If mobile portrait has any critical overlap, fix layout before web/mobile
  proof.
- If desire to continue averages below 4/5, do not expand content until the
  first loop reward clarity improves.
