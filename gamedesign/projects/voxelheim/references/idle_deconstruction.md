# Idle Auto-Battle RPG — Deconstruction (for Voxelheim)

Mode: **central deconstruction** (the idle loop + economy drive the game).
Reference question: the auto-battle idle loop, stage/boss pacing, currencies,
and prestige math for "hero auto-fights up stages -> gold -> upgrades -> bosses
-> prestige -> offline" (Voxelheim).

## Source packet status (HONEST)

**Secondary, not first-hand observed frames.** Evidence = official wikis +
formula guides + store/genre design talks. No first-hand gameplay video frames
were captured. Per `reference_deconstruction.md` this is "source packet
incomplete (secondary)" — strong enough to ground the loop + economy NUMBERS
(the formulas are published), but UI/feel claims are `secondary`/`inferred`, and
a later pass should add real gameplay-frame captures or lead-provided footage.

## Source matrix

| Source | Link | Quality | Proves | Uncertain |
|---|---|---|---|---|
| Clicker Heroes Wiki — Formulas / Zones / Hero Souls | clickerheroes.fandom.com/wiki/Formulas · /Zones · /Hero_Souls | wiki (secondary) | monster HP scaling, boss cadence, prestige (Hero Souls) math | exact current-version values, moment-to-moment UI |
| Tap Titans 2 / Tap Titans Wiki — Prestige / Relics + formula guides | tap-titans.fandom.com/wiki/Prestige · /Relics · tap-titans.com/formulas | wiki/guide (secondary) | HP formula, prestige relic math, artifact meta | balance patch drift |
| Pecorella, "Quest for Progress" (GDC) | see `gamedesign/sources/idle_incremental_design_sources_2026-06-16.md` | GDC talk | idle math norms: cost>value growth, prestige at +50-200%, fractional prestige scaling | — |
| Current build | `gamedesign/projects/voxelheim/visual/proof/release_candidate.png` + `src/voxelheim_main.c` | our capture | what we have now (real-time, not idle) | — |

## Per-game breakdown

### Clicker Heroes (closest 1:1 ref) — `secondary`
- **Loop:** hero + party auto-attack the monster on the current zone; kill ->
  gold; gold buys/levels heroes (the "upgrades"); auto-advance zones.
- **Stage/HP:** monster L1 = **10 HP**; grows **~x1.55 per level** (L1-140),
  then ~x1.145 (L140-500), slowing later. Steep early exponential.
- **Boss:** every **5 levels**, boss HP ~**x10** a normal monster; every 100
  levels a guaranteed Hero-Soul boss; Primal bosses (5% from L105) drop souls.
- **Prestige (Ascension):** reset for **Hero Souls** = `floor(total_hero_levels
  / 2000)`; **each Hero Soul = +10% all damage** (permanent). Souls also spent
  on Ancients (a meta tree).
- **Why it works:** steep exponential -> frequent visible jumps; a near-
  affordable hero upgrade always; ascension converts a stalled run into a flat
  +X% damage so the next run blasts the old wall.

### Tap Titans 2 — `secondary`
- **Loop:** tap + auto-attack titans up stages; boss-per-stage gate; gold ->
  hero/skill upgrades.
- **HP:** `MaxHP = 18.5 * 1.57^min(stage,150) * 1.17^max(stage-150,0)`; boss HP
  = MaxHP x [2,4,6,7,10] by tier. (Steep early, gentler after 150.)
- **Prestige:** unlock at main-hero L600; soft reset; earn **Relics** (1 relic ~
  1000 hero levels + a stage bonus every 15 stages, x2-3 if full team) -> buy
  permanent **Artifacts**. After stage 80, +1 relic per 10 stages.
- **Why it works:** two-layer meta (relics -> artifacts), staged relic income,
  "when to prestige" is the core decision.

## Comparison

| | Clicker Heroes | Tap Titans 2 | Voxelheim (target) |
|---|---|---|---|
| Combat | auto-attack | tap + auto | **auto** (pure idle) |
| HP growth / encounter | ~x1.55/level | ~x1.57/stage | **steepen toward ~x1.4-1.55** (see fix) |
| Boss | every 5, x10 HP | per-stage tiers x2-10 | every 10, x8 (ok) |
| Soft currency | gold -> hero levels | gold -> hero levels | gold -> 4 upgrades |
| Prestige currency | Hero Souls = totalLevels/2000 | Relics ~ heroLevels/1000 | **Frost Shards (fix formula)** |
| Per-prestige bonus | +10% dmg / soul | artifacts | **borrow: +X% dmg/gold per shard** |
| Meta depth | Ancients tree | Artifacts | shard upgrades (keep small) |
| Offline | yes | yes | yes (8h) |

## Borrow / Avoid / Copy-risk

- **Borrow (Clicker Heroes):** steep early exponential HP so kills visibly speed
  up; boss every N as the pacing gate; **prestige currency from ACCUMULATED
  progress with a divisor** (souls = totalLevels/2000), and **a flat +% per
  prestige unit** (+10%/soul) — simple, legible, exactly our Frost-Shard intent.
- **Borrow (TT2):** "when to prestige" as the meta decision; relic income that
  scales with how far you pushed.
- **Borrow (Pecorella):** reset is worth it at **+50-200%** meta gain; scale the
  prestige currency with a **fractional** exponent, not super-linear.
- **Avoid:** TT2's deep two-layer artifact tree + tap-DPS (we are pure idle,
  first slice = small); CH's late-game Ancient complexity; any energy/IAP gates.
- **Copy-risk:** none (formulas/structures are genre-standard); keep our snowy
  Frost-Keep skin and names — do not reuse "Hero Souls"/"Relics"/"Titans".

## Mismatch vs current build (`src/voxelheim_main.c`)

1. Build is **real-time tap-to-move with 3 fixed goblins**, not an endless
   auto-battle stream of scaling monsters. MUST convert.
2. **No gold / upgrades / stages / bosses / prestige / offline** wired — the
   whole idle economy is missing.
3. **`data/balance.json` scaling is far too flat vs refs** (see fix) — it would
   not feel like an idle climb.

## balance.json corrections (grounded in the refs)

- **Upgrades must be MULTIPLICATIVE, not additive (gap exposed by the build,
  2026-06-16).** Clicker Heroes / Tap Titans 2 scale hero damage
  MULTIPLICATIVELY (hero-level DPS, % bonuses), so flat DPS tracks the
  exponential HP climb and power visibly COMPOUNDS — the core idle dopamine. Our
  v2 balance.json had Sword = flat **+3 dmg/level**, which cannot track HP
  ×1.45/stage → mid bosses become impossible walls (the build had to add a
  relative-timer band-aid), and each upgrade feels weaker over time. FIX: make
  damage multiplicative — e.g. Sword = **×1.10-1.15 damage/level** (or hero
  damage = base × Π(upgrade multipliers)), and prestige shards = a global
  **×damage / ×gold** multiplier (+10%/shard, as Clicker Heroes). This is the
  "deeper lever" the build flagged (T0006). [My deconstruction missed the
  upgrade SCALING SHAPE; it focused on HP/gold/prestige scaling.]
  - **IMPLEMENTED + VERIFIED (balance.json v3, 2026-06-17).** Hero damage =
    `hero_base_damage(5) × 1.12^sword_level × (1.10)^shard_damage_level`. Sword
    `damage_mult_per_level = 1.12` (≈3.3 sword levels recover one stage's HP
    ×1.45 step). Prestige shard global_damage/global_gold apply
    MULTIPLICATIVELY as `(1 + 10/100)^level` (+10%/level compounding, Clicker
    Heroes Hero Souls). The boss `timer_relative_mult` band-aid was REMOVED →
    fixed `timer_s = 30`. Headless probe (`voxelheim_play_test.py 9182`,
    35/35 pass): on a BASE run (no prior prestige) the stage-10 boss is beaten
    in ~3-6s and the stage-20 boss in <1s (both inside the fixed 30s timer),
    prestige@25 is reached, a Frost-Shard global-damage purchase raises base
    damage ≥×1.10, and the post-prestige run reaches an early milestone clearly
    faster than a no-shard run. Natural prestige wall lands at the stage-80 boss
    (Boots hit the 0.2s interval floor + gold saturates int32), which is the
    intended "run stalls → prestige to push past" idle moment.
- **Steepen monster growth.** Refs use ~x1.55-1.57 per encounter. Our
  `hp_growth_per_stage = 1.15` over a 10-kill stage is ~x1.014/kill — far too
  flat. Either grow HP **per kill** (~x1.25-1.45/kill) or raise
  `hp_growth_per_stage` to ~**1.9-2.5** so each stage is a real step. Keep
  gold growth a touch below HP growth so upgrades stay meaningful.
- **Fix the prestige formula.** Replace `floor((stage/10)^1.5)` (super-linear,
  wrong direction) with an **accumulation/divisor or fractional** form, e.g.
  `frost_shards = floor((highest_stage)^0.5)` or `floor(total_gold_earned /
  K)` — tuned so a prestige grants **+50-200%** more shards than the last
  (Pecorella). 
- **Per-shard bonus:** adopt CH's legibility — each Frost Shard spent = a flat
  **+% global damage / gold** (start ~+8-10%/level).
- Keep boss (every 10, x8) and offline (8h) — both in ref range.

## Next proof

Native idle screenshot/scenario: gold ticking from auto-kills, buying "Sword +1"
visibly raises kill-speed, the **stage counter climbs with a steep difficulty
curve** (not flat), a boss at stage 10, and a prestige that grants Frost Shards
sized for a +50-200% next-run jump. This proves the loop + the corrected economy,
i.e. the AGENTS.md Game/core-loop gate — judged by a game-design critic.
