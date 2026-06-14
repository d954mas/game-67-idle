---
type: GDD
title: Rune Marches First Slice GDD
description: First playable slice design for the casual open-world RPG.
tags: [gdd, rpg, ftue, balance, open-world]
timestamp: 2026-06-13T00:00:00Z
---

# Rune Marches First Slice GDD

## Product Goal

Build a casual open-world fantasy RPG that gives a Skyrim-like sense of
freedom and place, a Daggerfall-like sense of many routes and factions, and a
The Quest-like mobile-readable RPG structure, without copying any IP or
shipping a dense hardcore RPG first session.

## First 30 Seconds

1. Player sees `Miregate` on the left, `Wispfen Road` in the center, and a
   locked `Old Bell Tower` marker on the right.
2. Top bar shows HP, mana, silver, XP, and ward rank.
3. Primary action is `Scout Road`.
4. First click/tap starts a short scout check and reveals a Mire Wisp.
5. Player chooses `Strike` or `Spark`.
6. Win gives visible silver, XP, and a rune spark.
7. Journal panel starts empty but reserves space for main, side, road, spell,
   and reputation progress.

Text budget before first input: one objective line and one button label.

## First 5 Minutes

1. Complete two road scouts.
2. Defeat or repel two Mire Wisps.
3. Earn at least 12 silver and 1 rune spark.
4. Buy `Spark Ward I`.
5. See spell damage increase and unlock `Old Bell Tower`.
6. Inspect the tower bell and unlock the Reedmere Crossing road.
7. Defeat the first Reed Raider and see the journal reach the first endpoint.
8. If the bell rope was returned for kindness, light the Moss Shrine and gain
   `spirit_favor`.
9. Spend that favor to open `Greenfen Causeway`, the next authored route hook.
10. Scout Greenfen, defeat the first Fen Shade, reach Warden Rank II, and gain
    rune lore.
11. Study rune lore to unlock `Spark Ward II`.
12. Choose the next route: `Briar Gate` as the main road or `Moonwell` as the
    optional oath route.
13. Clear the first route-specific beat: defeat the Briar Stalker or calm the
    Moonwell Sentinel.
14. Map the next branch landmark: `Ashen Cairn` for Briar Gate or
    `Starfall Grotto` for Moonwell.

## Core Loop

```text
Explore intent -> Scout Road -> encounter/check -> reward ->
map/status change -> upgrade or quest turn-in -> unlock next location
```

## First Slice Systems

- Primary action: scout an adjacent road.
- Passive/idle action: road patrol progress fills every 4 seconds while the
  player is on the main screen.
- Activity/job: `road_scout`.
- Upgrade: `spark_ward_1`.
- Upgrade sink: `spark_ward_2`.
- Stats affected: XP, mana, spell power, ward rank.
- First level-up: reaching 20 XP promotes the player to Warden Rank II,
  restores HP to 24 max, and adds one ward progress tick.
- Currency source: encounter win and quest turn-in.
- Currency sink: spell upgrade.
- Unlock: Old Bell Tower after first upgrade.
- Second unlock: Reedmere Crossing after Old Bell Tower inspect.
- Visual/status change: Wispfen Road changes from unsafe purple to calmer blue;
  Old Bell Tower marker becomes active; Reedmere Crossing appears as the next
  reachable road.
- Reason to return: claim patrol reward, resolve the bell rope choice, and
  open the next road.
- First choice payoff: kindness unlocks an optional Moss Shrine blessing after
  Reedmere is cleared.
- Post-payoff routing: `spirit_favor` opens Greenfen Causeway and leads into
  the first Greenfen scout beat instead of a dead-end reward.
- First lore sink: Greenfen rune lore unlocks `Spark Ward II`, making magic
  progression visible before the session ends.
- Post-Greenfen route choice: Spark Ward II reveals `Briar Gate` and
  `Moonwell`, letting the player choose a main-road or side-oath endpoint.
- First chosen-route beat: the selected route immediately opens a short combat
  or trial so the choice becomes a playable world action, not a static marker.
- Branch landmark discovery: clearing that beat reveals one next location per
  route, keeping the static open world expanding after the first branch.

## Activities

### Activity: Road Scout

- Unlock condition: available from start.
- Input: click/tap `Scout Road`.
- Duration/cooldown: immediate first action; later 4 second patrol fill for
  passive reward.
- Cost: none.
- Reward: 6 silver, 4 XP, 1 rune spark on first encounter win.
- Stat effects: `road_safety +1`; `ward_rank` progress increases by 1.
- Failure/blocked state: if HP reaches 0, return to Miregate with 1 HP and no
  silver from that encounter.
- Visual feedback: road marker pulses; encounter panel opens; the latest reward
  appears as a readable journal chip, with later art pass reserved for motion.
- Upgrade affected by: `spark_ward_1` shortens encounter by increasing spell
  damage.
- UI location: center action panel.

### Activity: Miregate Rest

- Unlock condition: HP below max or after first combat.
- Input: click/tap `Rest`.
- Duration/cooldown: immediate.
- Cost: 3 silver after first free rest.
- Reward: restore HP to max.
- Stat effects: no XP.
- Failure/blocked state: disabled if already full HP.
- Visual feedback: hamlet lantern warms and HP bar refills.
- Upgrade affected by: none in first slice.
- UI location: lower action row after combat.

## Upgrades

### Level-Up: Warden Rank II

- Unlock condition: reach 20 total XP, usually on the first Greenfen victory.
- Cost: none.
- Effect: player level from 1 to 2; max HP from 20 to 24; HP restored to new
  max; ward progress +1.
- Why player wants it: makes XP visibly useful before any menu-heavy character
  sheet exists.
- Visible before/after: top bar changes from `LVL 1` to `LVL 2` and HP cap
  changes to `24`.
- Max level or scaling: only level 2 is implemented in this slice.

### Upgrade: Spark Ward I

- Unlock condition: win first Mire Wisp encounter.
- Cost: 12 silver, 1 rune spark.
- Effect: `Spark` damage from 5 to 7; max mana +2.
- Why player wants it: ends first enemy in fewer turns and unlocks Old Bell
  Tower.
- Visible before/after: spell button number changes; tower marker unlocks.
- Max level or scaling: level 1 only in first slice.
- Affected activity: road scout encounter.

### Upgrade: Spark Ward II

- Unlock condition: defeat the first Fen Shade and earn `rune_lore`.
- Cost: 1 rune lore.
- Effect: `Spark` damage from 7 to 9; max mana from 12 to 14.
- Why player wants it: proves the Greenfen reward has a practical magic payoff
  and prepares the player for later roads.
- Visible before/after: upgrade button changes to `Study`, then `Ward II`; top
  mana and journal spell damage update.
- Max level or scaling: level 2 only in this slice.
- Affected activity: later Greenfen and future road encounters.

## Main Quest

### Quest: Wake The Road Stone

- Step 1: Scout Wispfen Road.
- Step 2: Defeat or repel a Mire Wisp.
- Step 3: Upgrade Spark Ward.
- Step 4: Unlock Old Bell Tower.
- Step 5: Inspect Old Bell Tower.
- Step 6: Unlock Reedmere Crossing.
- Step 7: Defeat the first Reed Raider.
- Step 8: Optional Moss Shrine blessing.
- Step 9: Open Greenfen Causeway.
- Step 10: Scout Greenfen Causeway.
- Step 11: Defeat the first Fen Shade.
- Step 12: Study rune lore and unlock Spark Ward II.
- Step 13: Choose `Briar Gate` or `Moonwell` after Spark Ward II.
- Step 14: Clear the first chosen-route threat or oath trial.
- Step 15: Map `Ashen Cairn` or `Starfall Grotto`.
- First-slice endpoint: `MAIN 18/18` with one explicit post-Greenfen route
  choice, one route-specific beat, and one branch landmark, proving authored
  branch content without opening a full dungeon yet.

## Side Quest

### Quest: Missing Bell Rope

- Trigger: after first scout.
- Request: a Miregate child asks the Warden to recover a bell rope charm from
  the road.
- First-slice implementation: one optional reward choice after the second Wisp
  encounter.
- Reward option A: +6 silver.
- Reward option B: +1 kindness reputation.
- Payoff: kindness reputation can light the Moss Shrine after Reedmere is safe.

### Optional Objective: Moss Shrine

- Trigger: `kindness_reputation >= 1` and `east_road_safety >= 1`.
- Input: `Blessing`.
- Reward: +1 `spirit_favor`, +1 ward progress, mana restored to current max.
- Purpose: make the first side quest choice visibly affect later exploration.
- Blocked state: if the rope was traded for silver, the shrine remains a later
  world hook instead of an immediate action.

### Route Hook: Greenfen Causeway

- Trigger: `spirit_favor >= 1` after the Moss Shrine is lit.
- Input: `Open Pass`.
- Reward: unlocks `Greenfen Causeway`, sets `causeway_safety` to 1, advances
  main progress to `10/10`.
- Purpose: make the first side-choice payoff route the player toward a larger
  authored open world without building the full next region in this slice.

### Activity: Greenfen Scout

- Unlock condition: Greenfen Causeway is open.
- Input: `Greenfen`.
- First enemy: Fen Shade.
- First reward: +10 silver, +8 XP, +1 `rune_lore`,
  `greenfen_safety +1`.
- First progression payoff: if total XP reaches 20, trigger Warden Rank II and
  restore HP to 24 max.
- Expected first win: Rest if needed, then Spark twice.
- Purpose: prove the next-route hook is playable and not just a map marker.

### Route Choice: Briar Gate / Moonwell

- Trigger: `Spark Ward II` learned.
- Input A: `Briar Gate` on the primary action.
- Input B: `Moonwell` on the upgrade/side action.
- Reward A: unlocks `Briar Gate`, marks the main road, advances main progress
  to `14/14`.
- Reward B: unlocks `Moonwell`, grants +1 `moonwell_blessing`, advances main
  progress to `14/14`.
- Purpose: make the first session end on a clear open-world choice instead of
  a single linear endpoint.

### Activity: Briar Gate Scout

- Unlock condition: `Briar Gate` route chosen.
- Input: `Briar Fight` / primary route action.
- First enemy: Briar Stalker.
- First reward: +12 silver, +10 XP, `briar_gate_safety +1`, main progress to
  `16/16`.
- Expected first win: Spark twice with Spark Ward II.
- Purpose: make the main-road branch feel dangerous and materially rewarding.
- Next hook: unlocks `Ashen Cairn` discovery.

### Activity: Moonwell Trial

- Unlock condition: `Moonwell` route chosen.
- Input: `Moon Trial` / primary route action.
- First enemy: Moonwell Sentinel.
- First reward: +6 silver, +6 XP, +1 `moonwell_blessing`,
  `moonwell_safety +1`, mana restored to current max, main progress to
  `16/16`.
- Expected first win: Spark twice with Spark Ward II.
- Purpose: make the side-oath branch feel magical and utility-focused.
- Next hook: unlocks `Starfall Grotto` discovery.

### Landmark Discovery: Ashen Cairn

- Unlock condition: `briar_gate_safety >= 1`.
- Input: `Ashen Map` / primary route action.
- Reward: +4 XP, unlocks `Ashen Cairn`, main progress to `18/18`.
- Purpose: show the main route leading toward a future dungeon without building
  that dungeon yet.

### Landmark Discovery: Starfall Grotto

- Unlock condition: `moonwell_safety >= 1`.
- Input: `Star Map` / primary route action.
- Reward: +4 XP, +1 `rune_lore`, unlocks `Starfall Grotto`, main progress to
  `18/18`.
- Purpose: show the side-oath route leading toward a future magic cave and
  give the player a small lore reason to return.

## Second Road

### Location: Reedmere Crossing

- Unlock condition: inspect Old Bell Tower after Spark Ward I.
- Input: `Scout East`.
- First enemy: Reed Raider.
- First reward: +8 silver, +6 XP, `east_road_safety +1`.
- Journal signal: `MAIN 18/18`, `SIDE 3/3`, `EAST 1`, optional `FAVOR 1`,
  `GREEN 1`, route safety (`BRIAR` or `MOON`), branch landmark text, chosen
  route text, and latest reward chip.
- Out of scope for this slice: full Reedmere town, faction screen, dungeon, or
  repeatable economy.

## FTUE Rules

- First meaningful action is visible immediately.
- Only one primary action is emphasized until the player wins the first
  encounter.
- Upgrade panel appears after the first reward, not before.
- Reward chip must name the concrete gain in a short phrase, for example
  `+6 SILVER +4 XP +SPARK` or `WARD II DMG 9 MP 14`.
- Disabled buttons must show exact missing cost in UI nodes or button state.
- A player can recover from loss with rest; no hard fail in the first session.

## Platform Rules

- Minimum mobile portrait target: 360 x 640.
- Primary touch target: at least 48 CSS pixels or equivalent runtime size.
- No hover-only information.
- Main action remains reachable in lower half on portrait.
- Top bar must not cover the map or combat panel.
- Native PC screenshot is the first visual proof; web/mobile portrait proof
  follows after native behavior is stable.

## Art Direction Seed

Style target: readable painted low-fantasy, broad shapes, clear silhouettes,
warm village light vs cold marsh magic. The first runtime may use shape art,
but final art should make the player avatar, road, enemy, upgrade, and locked
landmark readable in a still screenshot.

## Risks

- Fun risk: scouting may feel like a menu, not exploration.
  - Smallest proof: native screenshot with map state plus input audit where one
    click opens encounter and visible reward changes.
- UX risk: RPG stats may overwhelm casual users.
  - Smallest proof: first 60 second playtest script with one goal and one
    upgrade.
- Production risk: open world scope can balloon.
  - Smallest proof: keep first slice to 7 locations, 5 enemies, 2 upgrades, 2
    quests, 1 side choice, 1 optional payoff action, 1 route hook, 1 Greenfen
    scout beat, 1 auto level-up, 1 lore sink, and 1 chosen-route beat per
    branch.
