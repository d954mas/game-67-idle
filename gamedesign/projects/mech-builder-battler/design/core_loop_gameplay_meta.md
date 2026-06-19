---
type: Project Design Notes
title: Core Loop Gameplay And Meta Decomposition
description: Derived design shape for the casual mobile/web 3D mech builder battler.
tags: [project, core-loop, gameplay, mechanics, meta, mobile, web, casual]
timestamp: 2026-06-19T00:00:00Z
---

# Core Loop, Gameplay, Mechanics, And Meta

Scope: early GDD decomposition for the casual mobile/web 3D mech builder
battler. These are project-specific conclusions derived from the current user
direction and the [mobile mech analogs](../references/mobile_mech_analogs_2026-06-19.md).

## Product Positioning

One-line concept:

> A casual 3D mobile/web mech builder battler where the player upgrades one
> expressive mech, tests builds in short PvE battles, and sees every meaningful
> part change both the model and the play style.

Primary player need lane: accessible progression.

Secondary lane: active mastery, introduced after the first build-battle-upgrade
loop is readable.

## Core Promise

- I own a cool 3D mech.
- I can understand my next action immediately.
- I battle for 60-180 seconds.
- I earn salvage/resources.
- I spend them on a module and bolt it onto the mech.
- The mech looks and plays differently.
- I can try a harder fight or a different build style.

## Core Loop

### 10-Second Loop

1. Player enters a small combat arena.
2. The mech auto-targets or presents a clear target.
3. Player uses one obvious attack/special.
4. Enemy takes visible damage with sparks, knockback, shield break, or part pop.
5. Player gets immediate feedback: damage number, stagger, resource, cooldown,
   heat, or progress tick.

### 60-Second Loop

1. Defeat a small wave or single mini-boss phase.
2. Use one build-defining decision: dash, shield, missile burst, laser channel,
   melee strike, or drone deploy.
3. Collect salvage/resources.
4. See a reward summary.
5. Return to hangar with one upgrade prompt.

### 5-Minute Loop

1. Complete 2-3 short encounters.
2. Upgrade or swap one part.
3. Unlock a new mission node, enemy type, or build option.
4. Compare the previous and new mech state visually.
5. Decide whether to push a harder mission or farm a known one.

## Gameplay Pillars

### 1. Touch-First Spectacle

- Battle is readable on a phone screen.
- The player does not need precise shooter aim.
- Effects are strong but do not hide the mech, enemies, health, or primary
  action.
- The camera shows the full mech silhouette often enough to sell ownership.

### 2. Parts Change Style

Good part examples:

- Heavy legs: slower, more armor, less knockback, better missile stability.
- Jump legs: short hop/dash, lower armor, better flank bonus.
- Shoulder rockets: delayed burst, area clear, high heat.
- Energy blade: dash strike, combo finisher, short-range risk.
- Shield core: frontal guard, counter window, slower energy recovery.

Bad part examples:

- "+2% damage" with no model change.
- "Rare cannon" that acts the same as the starter cannon.
- A cosmetic-only part presented as mechanical progression.

### 3. Fast Upgrade Return

- The first session should not bury the player in inventory.
- Every reward screen should answer: what did I get, where does it go, what
  changed, and what can I try next?
- "Equip best" can exist, but the player should still see why the suggested
  item matters.

### 4. PvE First

- PvP is out of scope for the first version.
- Enemy design carries learning: swarm, shield, sniper, charger, turret,
  mini-boss.
- Difficulty grows through enemy roles and soft counters, not only HP scaling.

### 5. Hangar As Home

- The hangar is the emotional center.
- The mech should be large, animated, inspectable, and responsive to part
  changes.
- Paint and cosmetics matter, but should not replace mechanical build identity.

## Control Model Candidates

### Recommended First Slice: Semi-Auto Arena

- Movement: floating virtual joystick / drag movement zone for mobile target,
  with WASD in the native PC harness.
- Targeting: auto-lock nearest/high-priority target.
- Primary attack: auto-fire or large hold button.
- Skill buttons: 1 short dash + 1 special weapon.
- Camera: fixed three-quarter/isometric view that keeps phone readability.

Why: best balance between "I pilot a mech" and casual accessibility.

### Alternative: Auto-Battler With Tactical Specials

- Movement and base attacks are automatic.
- Player chooses when to trigger dash, shield, missile, finisher, or repair.
- Buildcraft matters more than moment execution.

Why: safer for casual/web, but weaker pilot fantasy.

### Alternative: CATS-Like Build Simulation

- Player builds the mech.
- Fight resolves mostly automatically.
- Player watches the result and upgrades.

Why: strongest accessibility, but risks losing 3D piloted-mech expectation.

## Mechanics Set

### First Slice Mechanics

- One player mech.
- One small 3D arena.
- Auto-target.
- Primary weapon.
- One special weapon.
- One defensive/mobility action.
- Health.
- Heat mechanics with `Cooling` UI label as the single tactical limiter.
- Salvage reward.
- One upgrade/equip action in hangar.

### Early Part Slots

1. Core/body: health, energy/heat capacity, visual torso.
2. Weapon: primary attack pattern.
3. Legs/drive: movement style and weight feel.
4. Special module: rockets, shield, blade, drone, repair burst.
5. Paint/decal: expression without power complexity.

### Enemy Roles

- Drone swarm: teaches area damage.
- Shield bot: teaches flank, heavy shot, or melee break.
- Sniper turret: teaches dash/cover.
- Charger: teaches timing and defensive action.
- Mini-boss: tests the whole build with telegraphed attacks.

## Meta Progression

### Meta Loop

Battle -> salvage/resources -> buy/craft part or progress blueprint ->
equip/upgrade -> unlock mission -> battle.

### Resources

- Salvage: common upgrade currency, visible as scrap parts.
- Blueprints: unlock a new part family after enough shards.
- Core cells: rare milestone resource for core/body upgrades.
- Paint chips: cosmetic reward that does not block power.

Keep currencies few in the first version. If a new resource cannot explain a
new decision, do not add it.

### Upgrade Philosophy

- Leveling improves baseline stats, but milestones should add behavior:
  shorter cooldown, extra rocket, wider shield, faster dash, heat refund on
  kill, stagger bonus.
- Old parts should stay viable through roles or upgrade paths. Avoid making
  every season/zone invalidate the previous set.
- Random rewards can exist as spice, but the player should have deterministic
  paths to target a desired build.

## First-Minute Design

Target:

1. First screen shows the mech in hangar and one clear "Battle" action.
2. First tap launches a short tutorial fight.
3. First fight asks only move/attack/special, not build optimization.
4. First purchase/craft visibly attaches to the mech.
5. First upgrade changes either the model or attack effect.
6. Second fight immediately proves the change.

Avoid:

- Offer walls before the first battle.
- A full inventory grid before the player knows why parts matter.
- PvP matchmaking before the player understands controls.
- Long lore intro.
- Text-only upgrade explanation.

## Visual Direction Notes

- 3D models should be stylized and readable, not hyper-realistic kitbash noise.
- The mech needs a strong silhouette at phone scale.
- Big part changes should alter silhouette: shoulder pod, arm cannon, blade,
  shield, leg profile, reactor glow.
- Effects should be color-coded by function: damage, shield, heat, reward,
  unlock.
- The hangar should be calm and inspectable; battle should be energetic and
  readable.

## Economy And Monetization Guardrails

This document does not design monetization, but the refs show repeated player
complaints around ads, popups, pay pressure, and power creep. Early GDD should
therefore treat these as constraints:

- No forced monetization interruption before the first satisfying loop.
- Rewarded ads, if later used, must be optional bonus acceleration, not the
  expected way to pass difficulty spikes.
- No pay-only mechanical counter required for normal progression.
- No offer popup stack on session start.
- No randomized reward path as the only way to complete a build archetype.

## First Vertical Slice Recommendation

Build the first playable proof around:

- Hangar screen with one visible 3D mech.
- One battle arena.
- One enemy wave and one mini-boss.
- Three loadout options:
  - starter cannon + dash,
  - shoulder rockets + heat,
  - shield bash + heavier legs.
- Reward: salvage/resources enough for one guided module purchase/craft.
- Upgrade: buy/craft and equip the module, then show the model or effect
  change.
- Proof screenshot sequence:
  1. hangar first screen,
  2. battle with primary action ready,
  3. special effect active,
  4. reward summary,
  5. hangar after equip.

## Risks To Resolve Before Implementation

- Slice scope: implementation and iteration use the native PC harness, but the
  first slice must preserve mobile/web controls, UI scale, readability, and
  session length.
- Input model: semi-auto arena vs auto-battler vs build-simulation is not yet
  decided.
- Orientation: landscape-first for the first native PC slice and fake shots.
- Central refs: current deconstructions are source-packet incomplete and need
  stronger gameplay/screenshot evidence before exact UI/combat/economy copying.
- Art: no accepted mech model style or fake shot exists yet.

## Next Research Pass

1. Lead review: accept/rework the GDD, first-slice spec, mechanics/meta matrix,
   and fake-shot art request.
2. Stronger reference evidence: capture gameplay/screenshot boards for Mech
   Arena, CATS, and Mechangelion before exact UI/combat/economy implementation.
3. Fake shots: hangar, battle, and reward/upgrade targets before final art.
4. Native PC slice planning: use the accepted floating virtual joystick /
   drag movement zone model with WASD harness input, then define the
   phone-scale screenshot proof sequence.
