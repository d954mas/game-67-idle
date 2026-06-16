# Voxelheim GDD

## One-Line Concept

A bright, blocky **casual action-RPG**: roam a snowy fantasy realm, fight
monsters, clear a dragon-guarded keep, loot, and level up — Skyrim's sense of
adventure in an instantly-readable Roblox-style shape.

## Visual Direction — LOCKED: "Bright Roblox Adventure" (Theme A)

Locked 2026-06-16 by the lead. Target = `visual/fake_shot_first_screen.png`.

- Saturated, cheerful palette; bright daylight; soft shadows; high readability.
- Blocky / chunky low-poly forms, clean toy-like plastic materials, bold
  outlines, soft cel shading.
- 3/4 top-down ("2.5D" feel) for casual readability.
- Rendered in-engine with **REAL sprite assets** (`nt_sprite_renderer` + atlas
  pack), never the debug shape renderer (AGENTS.md).
- The fake shot is **direction** (mood / palette / composition / readability),
  not a pixel target.
- Avoid: realism, muddy / low-contrast, grimdark, debug primitives.

## Audience

Casual players (Roblox-style). Simple controls, clear progression, juicy
feedback. A first-time player must understand goal + action in ~10s.

## Core Loop

1. Move toward a **visible** objective.
2. Fight a **readable** threat.
3. Get loot + XP (juicy feedback).
4. See power go up (hero changes **and** a number).
5. A new short-term objective appears.

## First Playable Slice — "Frost Keep Approach"

ONE goal, ONE primary action (AGENTS.md first-screen discipline).

- **Goal:** reach and clear the glowing **Frost Keep** entrance at the end of
  the snowy path.
- **Primary action:** tap / click to move; the hero **auto-attacks** enemies in
  range (casual — no combo system in the slice).
- **First 30 seconds:** spawn on the path → quest banner "Reach the Frost Keep"
  → walk up → 1–2 frost enemies block the path → defeat them → step into the
  glowing keep portal → **Victory**.
- **Reward moment:** loot sparkle + XP → a visible **LEVEL UP** (number ticks up,
  hero glows / scales slightly, HP bar grows).
- **HUD (from fake shot):** top-left HP + stamina bars + level badge; top-right
  minimap; bottom hotbar; quest banner under the minimap.
- **FTUE ≤ 3 beats** (on-screen text): (1) "Tap to move" → (2) "Defeat the
  monsters" → (3) "Enter the glowing keep".
- **Minimal RPG systems:** HP, XP / level (one visible badge), one auto-attack
  weapon, loot → power. Persisted via the `state/` schema.
- **Out of slice (defer):** open world, inventory / equipment screen, skill
  trees, multiple zones, quest chains, co-op, and the **dragon as a fightable
  boss** (in the slice it is decorative / distant).

## Visual session contract (write before runtime visual code)

- **Goal:** a readable first screen that reaches the Theme-A fake-shot direction
  with real sprites.
- **Non-goal:** full combat / economy / open world; pixel-matching the fake shot.
- **Proof:** native screenshot + `node tools/ai.mjs gate --visual-strict`
  against the fake-shot direction; source/runtime asset manifests + pixel audit.
- **Stop condition:** visual gate reaches the direction/quality bar **and** a
  newcomer reads goal + action in ~10s (teachability gate).
- **Likely files:** new `src/` voxelheim screen, asset atlas, `state/` schema,
  `gamedesign/projects/voxelheim/*`.

## Art Direction (seed — full art bible in the art task)

Palette: bright sky/portal blues, warm stone browns/greys, pale-blue snow,
saturated hero blue + a warm accent. Chunky dark outlines, soft cel shading,
mascot-readable hero silhouette. Generate source sheets via **agy** in the
Bright Roblox style (see the `delegated-image-generation` skill); cut + audit
through `generated-game-ui-assets`.

See `game_implementation_plan.md` for the build roadmap and gates.
