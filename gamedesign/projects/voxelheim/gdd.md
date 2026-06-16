# Voxelheim GDD — Idle RPG

Genre **LOCKED: idle / incremental RPG** (2026-06-16, lead). Reference grounding:
`references/idle_reference_digest.md`. Visual direction unchanged (Theme A,
below). Economy numbers: `data/balance.json`.

## Hook (one sentence)

**Your blocky hero auto-battles up an endless snowy path of monsters toward the
Frost Keep — spend the gold they drop on upgrades, leave it running to grow rich
while you're away, and prestige to climb the mountain faster.**

## Core Loop (the GAME, with numbers)

1. The hero stands on the path and **auto-attacks** the nearest monster (base 5
   dmg every 1.0s). Monsters walk down the path in a continuous stream.
2. A kill → **gold drop** (+ a coin pop) and the next monster spawns. Clear the
   stage's quota (10 kills) → **advance a stage**.
3. **Every 10th stage = a timed BOSS** (30s): big HP, big gold; fail = retry the
   boss (no loss of progress).
4. Spend gold on **4 upgrades** (cost ×1.09 per level — the exponential wall):
   - **Sword** → +damage. **Boots** → faster attacks. **Armor** → +max HP/regen
     (boss survivability). **Luck** → +gold find %.
5. Higher stage = tougher monsters (HP ×1.15/stage) but more gold (×1.18/stage)
   → number-go-up. There is always **one upgrade you can almost afford**.
6. When progress stalls, **PRESTIGE** ("Return to the valley"): reset stage +
   gold + the 4 upgrades; gain **Frost Shards** ∝ highest stage reached. Spend
   Frost Shards on **permanent** multipliers (global damage, gold, start stage,
   offline rate). Next run blasts past the old wall.
7. **Offline:** while away, the hero auto-farms your highest cleared stage at
   50% rate (cap 8h). Return → "While you were away: +X gold" + Collect.

## Currencies (2 — clear roles)

- **Gold** (soft): from kills; spent on the 4 upgrades; **resets on prestige**.
- **Frost Shards** (meta): earned on prestige ∝ highest stage; spent on
  permanent multipliers; **never reset**.

## Progression axes

Stage number (1 → ∞, bosses every 10) · 4 upgrade levels · prestige tier +
Frost-Shard upgrades. Milestone unlocks (e.g. prestige unlocks at stage 25;
offline unlocks after the first boss).

## "Next 5 minutes" pull (retention)

A near-affordable upgrade ticking closer · the next boss / next zone milestone ·
offline gold to collect · a prestige that makes the next run visibly faster.

## FTUE (<=3 beats, on-screen)

1. "Your hero fights on its own — watch the gold pile up."
2. "Tap **Sword** to upgrade — hit harder, kill faster." (pulse the button)
3. "Push as far as you can. When it slows, **Prestige** for a permanent boost."
   (revealed when prestige unlocks)

## Session shape

2-5 min active (buy upgrades, push stages, maybe prestige) → idle/offline →
come back, collect, repeat. Standard idle cadence.

## First idle slice (build target)

One screen: auto-combat stream + gold counter + stage counter + the 4 upgrade
buttons (live cost/effect) + a boss every 10 + prestige (unlock @25) + an
offline-earnings popup on launch. Reuse all current art.

## Visual Direction — LOCKED: "Bright Roblox Adventure" (Theme A)

Unchanged from before — see `visual/art_bible.md` + `visual/fake_shot_first_screen.png`.
Saturated, blocky, readable; real sprites via `nt_sprite_renderer`. The existing
hero/goblin/keep/path/backdrop/UI assets are reused; the hotbar art becomes the
upgrade panel.

## No-Go

- No real-time movement/aiming (it's idle — the hero auto-fights).
- No punishing death / energy gates / pay-to-win.
- No >2 early currencies, no deep skill webs (first slice).
- Not the whole mountain — first slice is the one auto-battle screen + the loop.

## Build plan

The current `src/voxelheim_main.c` real-time loop is converted to idle: stationary
auto-fighting hero, endless spawns, gold drops, stage scaling, 4 upgrades,
bosses, prestige, offline. State schema gains gold/stage/upgrade-levels/
frost-shards/last-seen-time. See the next task after this design is accepted.
