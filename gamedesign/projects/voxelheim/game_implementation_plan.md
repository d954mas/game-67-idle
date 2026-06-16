# Voxelheim — Implementation Plan

Status: **implementation-ready except command discovery** (native build/run
commands for a new screen must be confirmed against the engine — see Phase 1).

Goal: ship the "Frost Keep Approach" first slice (see `gdd.md`) on the native PC
build, reaching the Theme-A fake-shot direction, with real sprite art.

## Guardrails (from AGENTS.md)

- **Native PC first.** No web detour.
- **Visual-first freeze:** while the first screen fails the visual gate, do NOT
  expand systems, state, content, or automation. Fix the screen first.
- **Real assets only** through `nt_sprite_renderer` + atlas. Shape renderer is
  debug-only. Procedural/programmer-art does not close an art task.
- **Definition of done** = the native screen reaches the fake-shot direction AND
  the core moment feels right AND a newcomer can play it (teachability), not
  "probes green".
- Image generation is delegated to **agy** (see `delegated-image-generation`).

## Phases

### P0 — Art direction lock + first source sheets  (visual-first)
- Write the art bible (palette, materials, outline/shading, hero silhouette,
  enemy silhouette, environment kit, UI kit, FX) under `visual/`.
- Generate Bright Roblox **source sheets** via agy: hero (idle/walk/attack),
  one frost enemy, environment kit (snow ground, stone path, keep + glowing
  portal, pines, rocks, distant mountains/dragon), UI kit (HP/stamina bars,
  level badge, minimap frame, hotbar, quest banner), FX (hit, loot sparkle,
  level-up burst).
- Cut + audit through `generated-game-ui-assets` (intake → crop → runtime PNGs
  → pixel/atlas audits). Build the atlas pack.
- Proof: contact sheets + clean atlas; assets reach the fake-shot quality bar.

### P1 — Readable static first screen  (visual-first FREEZE target)
- New native screen renders the composed scene with real sprites: snowy path →
  Frost Keep with glowing portal → hero → distant dragon → full HUD overlay.
- **Confirm + record the native build/run/screenshot commands** for a new
  screen (CMake/preset/VS Code task discovery; see `game-feature-iteration`).
- Capture native screenshot; write the screenshot-vs-fake-shot mismatch list.
- **Gate:** `node tools/ai.mjs gate --visual-strict` reaches Theme-A direction.
  Do not proceed to P2 until this passes.

### P2 — Movement + camera
- Click/tap-to-move hero along the path; camera follows; stamina ties to sprint
  (optional). Synthetic-input smoke probe.

### P3 — Combat + one enemy + win condition
- Spawn 1–2 frost enemies; hero auto-attacks in range; HP + death; win = enter
  the keep portal after the path is clear. Readable hit feedback.

### P4 — Reward + progression (state-backed)
- Loot drop + XP → visible LEVEL UP (number, hero glow/scale, HP grows). Wire
  HUD (HP/stamina/level/quest) to generated `GameState`; persist via `state/`
  schema + migrations. DevAPI state commands for bot/test setup.

### P5 — FTUE + teachability
- 3-beat onboarding text (gdd.md). **Teachability gate:** a brand-new player
  understands goal + action in ~10s, every system has on-screen text/labels.

### P6 — Polish + product gate
- Juice (hit feedback, level-up burst, sfx), audio pass. Final visual + teach
  gates, native screenshot/product-read proof, smoke/full probes green.

## Art needs (P0 inventory)

hero (idle/walk/attack) · frost enemy · snow ground tile · stone path · Frost
Keep + glowing portal · pines · rocks · distant mountains + dragon · UI: HP bar,
stamina bar, level badge, minimap frame, hotbar slot, quest banner, primary
button · FX: hit spark, loot sparkle, level-up burst.

## State (P4)

`state/` schema: hp/maxhp, level, xp, gear/power, run progress, FTUE flags.
Regenerate the C `GameState` API via `tools/state_codegen/`; add a migration.

## Top risks → smallest owner action

1. **Fun** (is the move→fight→loot→levelup moment satisfying?) → P3/P4 native
   playtest, not just probes.
2. **Production** (matching the bright blocky look in a 2D sprite engine) → P0
   art bible + agy source sheets reviewed against the fake shot before P1.
3. **UX/teachability** (casual newcomer gets it in 10s) → P5 teachability gate.

## Next implementation prompt

"Start P0: write `visual/art_bible.md`, then generate the Bright Roblox source
sheets via agy and cut/audit them through `generated-game-ui-assets`. Do not
write runtime screen code until P1's first screen can pass the visual gate."
