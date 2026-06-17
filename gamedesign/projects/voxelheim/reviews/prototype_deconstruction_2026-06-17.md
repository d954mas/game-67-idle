---
type: Project Review
title: Voxelheim Prototype Deconstruction
description: Product/design teardown of the failed Voxelheim idle prototype and rescue direction.
tags: [voxelheim, prototype, teardown, ui, ux, gdd, rescue]
checked: 2026-06-17
---

# Voxelheim Prototype Deconstruction

## Verdict

The current Voxelheim prototype is not a successful game yet. It is a good
asset/rendering proof wrapped around a generic idle loop. The main failure is
not one missing feature; it is that the screen does not communicate a desirable
fantasy, the UI does not make the next action obvious, and the progression is
mostly invisible numbers.

User feedback that the project is ugly, unclear, hard to read, and too simple
is consistent with the evidence below.

## Evidence Checked

| Evidence | What it proves | Limit |
|---|---|---|
| `build/captures/after_final.png` | current native idle screen: combat, HUD, bottom upgrades | one moment, not full session |
| `gamedesign/projects/voxelheim/visual/idle_fakeshot.png` | intended idle visual target | fake shot, not runtime proof |
| `src/voxelheim_main.c` | implemented loop: auto-combat, 4 upgrades, bosses, prestige/offline | source has drift from newer balance docs |
| `gamedesign/projects/voxelheim/data/balance.json` | intended v4 depth additions: Frost Fury, Crit, Greed, Multi, prestige tree | several v4 systems are not implemented |
| `tasks/STATUS.md` | live status still says old action-RPG release candidate | stale; conflicts with current idle direction |
| user feedback, 2026-06-17 | UI/UX perceived as ugly, unclear, unreadable; GDD too simple/banal | qualitative but decisive lead signal |

## What Works

- The project has a coherent bright snowy asset direction with real sprites.
- The native harness, screenshot capture, DevAPI, and probes are valuable.
- The idle skeleton exists: auto-combat, gold, upgrades, stage counter, boss,
  prestige, and offline grant.
- The fake shot and current screen point to a readable toy/blocky audience if
  the UI is rebuilt instead of patched.

## Core Failures

### 1. The fantasy is generic

The current fantasy is "blocky hero auto-fights monsters toward a keep." That is
not enough against live idle RPG competitors. It lacks a product hook a player
can repeat, such as a weird avatar, a collectible identity, a visible home/base,
or a satisfying transformation loop.

### 2. The game has no visible transformation

Gold and stages rise, but the world does not meaningfully change. The keep is a
background object, not the player's project. The hero does not visibly grow in a
way that explains why the player should care. This makes the loop feel like a
spreadsheet over a pretty backdrop.

### 3. The main action is weak

Pure idle can work, but successful idle screens still create a dominant next
decision: tap, claim, choose a card, open loot, level a hero, repair a room,
prestige, or fight a boss. Voxelheim's first screen has four similar upgrade
buttons and no strong primary action. A new player sees "watch" more than "play."

### 4. UI hierarchy is wrong

Current hierarchy in `after_final.png`:

1. black HUD plates and bottom buttons dominate the screen;
2. the keep and background compete with the actual combat;
3. the enemy is not aspirational or central enough;
4. the minimap/brand plate takes premium space without gameplay value;
5. the bottom upgrade row compresses icon, level, stat, and price into crowded
   dark buttons.

The result is not only low readability. It also teaches the wrong thing: the
game looks like an adventure scene with a shop bar, not like a strong idle RPG
decision screen.

### 5. UI style fights the art

The art direction is bright, soft, snowy, toy-like. The UI uses heavy black
rectangles, dark green buttons, hard outlines, and mixed icon styles. It feels
like a debug/tool overlay placed on top of a game illustration.

### 6. Source-of-truth drift is now product risk

- `tasks/STATUS.md` still describes an action-RPG release candidate.
- `gdd.md` describes idle Voxelheim.
- `balance.json` v4 describes Frost Fury, Crit, Greed, Multi, and a prestige
  tree.
- `src/voxelheim_main.c` implements only Sword/Boots/Armor/Luck and 4 shard
  upgrades; no Frost Fury and no expanded run-upgrade set.

This drift makes agents likely to implement the wrong next step.

### 7. The GDD is too safe

The idle GDD correctly names genre parts, but "gold -> upgrades -> bosses ->
prestige -> offline" is table stakes. It does not create a Voxelheim-specific
reason to exist. The rescue design needs a second loop that is visible and
thematic, not just more multipliers.

## Rescue Thesis

Voxelheim should become **Frost Keep Rebuilder**:

> A cozy blocky idle RPG where the hero raids snowy monsters to recover magic
> blocks, then the player rebuilds the Frost Keep room by room. Every run makes
> the hero stronger, but every repair visibly changes the diorama.

This keeps what works:

- blocky snowy art;
- native auto-combat;
- idle/incremental pacing;
- bosses and prestige.

It changes what failed:

- the keep becomes the core progression object, not a background;
- the player has one obvious next action: claim loot, choose a relic, or repair
  a room;
- UI becomes a readable product surface, not a black overlay;
- progression is visible as rooms, companions, gear, and diorama upgrades.

## Required Design Changes

1. Replace "four equal upgrade buttons" as the main screen with a strong
   `Next Action` area.
2. Remove the minimap from the first slice. It consumes attention and does not
   answer a current gameplay question.
3. Make the combat presentation one large enemy and one readable hero action,
   with rewards flying to resources.
4. Add a visible keep-repair track: Gate -> Forge -> Campfire for the first
   slice.
5. Add a recurring choice beat: after a short combat packet, choose 1 of 3
   loot/rune cards.
6. Rebuild the UI kit around bright frosted panels, large text, separate icons,
   and explicit component states: locked, affordable, ready, active, disabled.
7. Treat old `balance.json` as legacy idle constants until the rescue loop is
   encoded in `data/rescue_loop.json`.

## Next Native Proof

The next build should prove one screenshot, not a whole game:

- central combat lane with one large enemy;
- top compact resources: Gold, Blocks, Keep Rank;
- right or left visible Frost Keep repair panel with 3 rooms;
- one dominant `Claim Loot` or `Repair Gate` action;
- a 3-card choice modal after the first packet;
- text readable in a zoom crop;
- no minimap, no black debug plates, no cramped four-button shop row.

If that still reads as confusing in a still screenshot, feature expansion stays
frozen.
