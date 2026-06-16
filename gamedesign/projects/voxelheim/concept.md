# Voxelheim — Concept (Stage Gate 1)

> Status: DRAFT concept for visual-direction exploration. Locked decisions move
> to `gdd.md`; this file holds the one-paragraph pitch + pillars.

## Fantasy / Hook

You are a fresh adventurer dropped into a blocky open-world realm of snowy
peaks, ruined keeps, and sleeping dragons. Pick up a sword, raid a dungeon,
level your hero, and come back to town richer and stronger. **"Skyrim you can
play with your friends in an afternoon"** — the freedom and dragons of an
open-world fantasy RPG, in the bright, instantly-readable, social shape of a
Roblox experience.

## Genre / Platform / Session

- Genre: open-world fantasy action-RPG (single-player slice first; social/co-op
  is a later layer, not the first slice).
- Platform: **native PC first** (per AGENTS.md). Visual target leans
  Roblox/console-casual, not photoreal.
- Session: 10–20 min loops — one dungeon run or one region of exploration,
  with persistent character progression between sessions.

## Core Verbs (player actions)

1. **Explore** — roam a blocky open region, spot landmarks (a ruined tower, a
   dragon roost), choose where to go.
2. **Fight** — real-time melee + a magic/ranged option against world enemies
   and a dungeon boss.
3. **Loot & Equip** — find gear/loot, equip it, see your hero visibly change.
4. **Level up** — spend earned XP/points to get stronger; return stronger.

## 3 Design Pillars (+ what violates each)

1. **Readable at a glance.** Every threat, pickup, exit, and objective reads in
   <2 seconds. *Violation:* cluttered HUD, muddy palette, enemies that blend
   into terrain.
2. **Always an adventure ahead.** From any spot the player can see at least one
   tempting destination. *Violation:* empty corridors, "go here because the
   quest says so" with nothing visible to pull you.
3. **Power you can see.** Progression shows on the character and the screen, not
   only in menus. *Violation:* upgrades that are pure stat numbers with no
   visible/feedback change.

## Progression Metric

Hero Power (level + equipped gear tier), surfaced as a single readable number/
badge, climbing run over run.

## First Slice (one goal, one primary action — to be detailed in gdd.md)

One readable region with one visible dungeon entrance; primary action = enter
and clear the dungeon to its boss; reward = loot + a level-up that visibly
changes the hero. FTUE ≤ 3 beats.

## No-Go List

- No photoreal rendering; no grimdark gore. Bright, friendly, blocky.
- No debug shape-renderer game visuals (AGENTS.md): real sprites/generated art
  through the asset path only.
- No 14-beat onboarding (the Rune Marches failure). FTUE ≤ 3 beats.
- No web prototype detour; native PC first.
- First slice is NOT the whole open world — one region, one dungeon, one boss.

## Open Visual Question (this session)

Which art **theme** sells the fantasy best? Exploring 3 fake-shot directions
before locking the art bible — see `tmp/voxelheim_fakeshots/` and the visual
gate. The three directions under test:

1. **Bright Roblox Adventure** — saturated, toy-like, daylight, max approachable.
2. **Dark Nordic Epic** — moody, snowy, dramatic light, Skyrim-leaning, older.
3. **Stylized Painterly Low-poly** — premium hand-painted low-poly fantasy.
