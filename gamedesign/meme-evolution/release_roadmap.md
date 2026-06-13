# 67 World Release Roadmap

Status: working roadmap for the persistent goal: release-quality game with about
1 hour of gameplay, FTUE, tutorial, balance, and polished visuals.

## Release Definition

The release target is not met until the native PC build has:

- a complete 1-hour progression arc;
- a first-time user experience a child can understand without explanation;
- tutorial guidance that teaches spawn, merge, collection, coins, upgrades, and
  world progress;
- enough 67 variants, goals, and unlocks to sustain the first hour;
- generated/polished visual assets integrated into the native build;
- save/load working for player progress;
- native screenshots and DevAPI/emulated-input evidence for the main path;
- no web prototype dependency.

## 1-Hour Gameplay Arc

The first hour is split into six 10-minute beats. Timing is a balance target,
not a hard timer.

| Time | Player experience | Content gate | System gate |
|---:|---|---|---|
| 0-2 min | Tap mystery box, merge first pair, unlock Berry 67 | Tiny, Berry | FTUE step 1 |
| 2-8 min | Learn collection tray, earn coins, buy faster box | Banana | Upgrade 1 |
| 8-18 min | Board starts filling; player learns matching strategy | Arcade through Rainbow | Pair hints and stuck-board clear |
| 18-30 min | Better Crate becomes the main accelerator | Neon through Lava | Repeatable crate-tier upgrades |
| 30-45 min | Late variants and bigger coin payouts | Donut through Ninja | Board-pressure recovery |
| 45-60 min | Chase Cosmic 67 and unlock next-world teaser | Galaxy, Golden, Cosmic | Save/load, long-session pacing |

Release can promise 67 total variants, but the first hour needs roughly 25-30
implemented variants with clear silhouettes, not all 67 fully tuned.

## FTUE Requirements

FTUE must be playable, visual, and skippable only after the first completed
merge. It should not be a text wall.

1. Highlight the mystery box.
   - Required action: spawn `tiny_67`.
   - Success: one Tiny 67 appears on the board.

2. Highlight the box again.
   - Required action: spawn a second `tiny_67`.
   - Success: matching pair pulses.

3. Highlight both matching board slots.
   - Required action: click or drag the two matching Tiny 67s.
   - Success: Berry 67 appears, collection slot unlocks, coins pop.

4. Highlight collection tray.
   - Required action: observe the next silhouette.
   - Success: goal changes to next variant.

5. Highlight upgrade when affordable.
   - Required action: buy Faster Box.
   - Success: box cooldown visibly shrinks.

## Tutorial And UI Rules

- The main playable action must be on the board, not in a row of debug buttons.
- The spawn source is a mystery box/crate, matching Cow Evolution's field-first
  feel without copying its art.
- Matching pairs pulse when available.
- Tapping an empty slot points back to the box.
- Tapping one character selects it; tapping a matching character merges.
- Board-full state must visibly say "merge a pair" and highlight available
  pairs.
- The top bar must show coins and `N / 67` progress.
- The collection tray must show locked silhouettes and newly unlocked slots.

## Content Roadmap

### Batch 1: First 7

- Tiny 67
- Berry 67
- Banana 67
- Smoothie 67
- Cool 67
- Portal 67
- Mystery 67

### Batch 2: Variants 8-18

Theme: Fruit + toybox meme energy.

- Jelly 67
- Lemon 67
- Watermelon 67
- Bubblegum 67
- Sticker 67
- Party 67
- Arcade 67
- Cloud 67
- Crown 67
- Rocket 67
- Rainbow 67

### Batch 3: Variants 19-30

Theme: 67 World expands.

- Neon 67
- Gummy 67
- Pixel 67
- Lava 67
- Donut 67
- Slime 67
- Disco 67
- Dragon 67
- Ninja 67
- Galaxy 67
- Golden 67
- Cosmic 67

## Balance Targets

- First merge: under 30 seconds.
- Third variant: under 2 minutes.
- First upgrade: under 1 minute for the tuned fast-child prototype.
- Board-full moment: after the player understands merging, not before.
- First 15 variants: about 10-15 minutes in the simulator.
- 25+ discovered variants: about 40-45 minutes.
- Cosmic 67: 55-60 minute simulator window; current deterministic sim reaches
  Cosmic at 57.19 minutes.

The early curve should be generous. Later variants can be slower, but every
5-7 minutes should show a visible new thing: variant, upgrade, board expansion,
world animation, or milestone.

## Native Implementation Phases

1. Child-testable core board.
   - Mystery box spawn.
   - Slot selection/merge.
   - Pair hints.
   - Goal panel.
   - One screenshot must look like a game, not a debug UI.

2. FTUE and tutorial.
   - Step state.
   - Highlight system.
   - Required action checks.
   - Skippable after first merge only.

3. 1-hour balance model.
   - 25-30 variants in data.
   - Upgrade costs and passive income tuned.
   - Board-full recovery.
   - Milestones.
   - Current proof: `py -3.12 tools/balance/simulate_67_world.py` reaches
     Cosmic in the 55-60 minute target window.

4. Visual asset integration.
   - Generated character sprites or sprite sheets.
   - Generated board, crate, UI panels, and feedback effects.
   - Asset manifest and pack/build path.

5. Persistence and restart.
   - Save board, coins, discovered variants, tutorial state, upgrades.
   - Autosave proof through DevAPI scenario.

6. Release validation.
   - Native debug scenario for FTUE.
   - Native release build.
   - Screenshot set: fresh start, first merge, mid-session, milestone.
   - Manual child-test checklist.

## Current Runtime Status

- Native PC runtime and DevAPI now expose variants 1-30.
- The collection drawer shows a sliding 7-slot window so late Batch 2 progress
  and Batch 3 progress remain readable instead of squeezing all cards into one
  row.
- Batch 2 and Batch 3 state fields, merge chain, passive incomes, discovery
  bonuses, and next-goal labels are implemented through `cosmic_67`.
- Evidence:
  `build/captures/scenarios/first_67_loop_30_variants_v1.png` and
  `build/captures/scenarios/extended_67_progression_30_variants_v1.png`.

## Current Gap

The current native prototype proves state, spawn, slot-click merge, DevAPI,
field-first visuals, readable HUD, a 30-variant release-track progression
chain, Better Crate progression, stuck-board recovery, and a one-hour balance
simulator that reaches Cosmic 67 at 57.19 minutes without direct state forcing.
It is still not release-quality. The next immediate gates are:

- improve the progress-upgrade HUD label/visual states for Better Crate and
  cleanup moments;
- validate mobile layout and native release packaging;
- run a longer manual child-test checklist and tune if the simulator does not
  match observed play;
- keep using native PC as the primary harness; no web path is used.
