# Voxelheim (idle) vs References — Review (2026-06-17)

Comparative review of the built idle slice against the deconstruction references,
on **gameplay** and **visual**. Evidence labels: `observed` (our captures / the
deconstruction's published facts) / `secondary` (ref store/wiki/compare articles,
no first-hand frames) / `inferred`.

## Evidence

- Our build (current, commit 99c0edf): `build/captures/mult_early.png`,
  `mult_upgrades.png`, `mult_boss.png`, `mult_prestige.png` (`observed`).
- Our visual target: `visual/fake_shot_first_screen.png` (`observed`).
- Gameplay refs: `references/idle_deconstruction.md` (Clicker Heroes / Tap
  Titans 2 published formulas) (`observed`-in-wiki / `secondary`).
- Ref visual/UX: appgrooves/clickerheroes.com/gamerforfun compares (`secondary`).

## Gameplay comparison

| Aspect | Clicker Heroes | Tap Titans 2 | Voxelheim | Verdict |
|---|---|---|---|---|
| Core loop | auto-attack -> gold -> level heroes -> boss -> ascend | tap+auto -> gold -> upgrades -> boss -> prestige | auto -> gold -> 4 upgrades -> boss -> prestige | ✓ correct genre skeleton |
| Damage scaling | multiplicative (hero DPS) | multiplicative (% bonuses) | multiplicative (Sword x1.12) | ✓ fixed this session |
| **Upgrade breadth / decisions** | dozens of heroes + per-hero upgrades + Ancients tree | many hero/skill upgrades + artifact tree | **only 4 upgrades** | ⚠ **thin** — refs' "what to buy next" depth comes from a LONG list; 4 runs out of decisions fast |
| **Active layer** | tap click-damage + active skills | tap-DPS + active skills + pets | **pure idle, nothing to tap** | ⚠ refs give moment-to-moment ACTIONS (tap, skills, pets); ours has none |
| Prestige meta | Hero Souls (+10%/soul) + Ancients tree | relics -> artifact tree | Frost Shards -> 4 multipliers | ✓ structure; ⚠ shallow vs a tree |
| Boss | every 5, x10 HP, timer | per-stage tiers | every 10, x8, 30s | ✓ |
| Offline / session | yes | yes | yes (8h) | ✓ |

**Gameplay verdict:** the loop is now correct and grounded (multiplicative,
compounding, bosses/prestige/offline verified 35/35). The gap vs the refs is
**decision-richness / stickiness**: only 4 upgrades, a shallow prestige meta,
and no active layer. CH/TT2 keep players via a long upgrade/hero list, a meta
tree, and active skills/pets/tap. Ours is a clean but MINIMAL idle — fine as a
first slice, thin for retention.

## Visual comparison

| Aspect | Clicker Heroes | Tap Titans 2 | Voxelheim | Verdict |
|---|---|---|---|---|
| Polish / palette | minimal, flat, number-focused | polished hand-drawn, VFX-heavy | bright Roblox, good polish | ✓ above CH, near TT2 |
| **The fight presentation** | one monster, central | ONE titan, big & central, hero attacks it | **monster STREAM stacks into a vertical "tower"** | ⚠ **biggest visual bug** — reads as a totem, not an enemy being fought |
| Composition shape | combat-center + side hero list | combat-center + bottom skill bar | adventure backdrop (path->keep) + small bottom panel | ⚠ reads as an ADVENTURE scene, not an idle BATTLER |
| Upgrade UI prominence | prominent hero list | prominent skill/upgrade bars | small 4-button bottom panel | ⚠ under-emphasised for an idle (the panel IS the game) |
| Number/feedback focus | numbers dominate | DPS + crit numbers pop | gold/stage counters + floaters | ~ ok, could push the juice |

**Visual verdict:** art QUALITY is good and on-direction (Theme A, close to
TT2's polish, far above CH's minimalism). But the COMPOSITION does not yet read
as an idle auto-battler: (1) the monster stream renders as a stacked **tower**
instead of one prominent enemy the hero fights; (2) the screen is still shaped
like the **adventure fake shot** (path-to-keep backdrop + a small panel) rather
than an idle layout (prominent enemy + a prominent upgrade panel + numbers).
Note: `fake_shot_first_screen.png` was authored for the ACTION concept and is now
a partial mismatch with the idle genre — the visual target should be re-aimed for
idle.

## Top prioritized gaps (to match the refs)

1. **[Visual BLOCKER] Fix the monster "tower".** Present the fight like CH/TT2:
   one prominent enemy (the current monster/boss) front-and-center that the hero
   attacks, with the next few queued small behind it — not a vertical stack.
2. **[Gameplay MAJOR] Add decision-richness.** More upgrades / a small upgrade
   or prestige TREE, so "what to buy next" stays interesting past the first
   minutes (CH/TT2's core stickiness).
3. **[Gameplay MAJOR] An active layer (optional but genre-standard).** A tappable
   ability (e.g. a "rally"/crit on tap, or a single active skill) gives
   moment-to-moment action; pure-idle is the calmest, least-sticky end.
4. **[Visual MAJOR] Re-aim the composition + visual target for idle.** Make the
   upgrade panel prominent (the panel IS the game), keep the enemy central; a new
   idle fake shot (or annotate the current one as action-era).
5. **[Polish] Push combat juice** (hit numbers, crits, gold burst) toward TT2's
   feedback density.

## Honest status

The loop is grounded and verified (passes the Game/core-loop gate); the art
quality reaches the Theme-A bar. Against the refs, the build is a **clean but
minimal idle with one real visual bug (the monster tower) and a depth/stickiness
gap (4 upgrades, no active layer, shallow meta)**. None are blockers to "it's a
real idle game"; they are the deltas to reach the refs' polish + retention.
