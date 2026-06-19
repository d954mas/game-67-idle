---
type: Project Mechanics Matrix
title: Mech Builder Battler Mechanics And Meta Matrix
description: Reference-backed breakdown of combat, assembly, grind, upgrades, and meta scope for the casual mech builder battler.
tags: [project, mechanics, meta, progression, combat, mobile, web, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# Mechanics And Meta Matrix

Status: design synthesis for lead review, not implementation-ready.

This document translates the current reference set into concrete mechanics and
meta decisions for `mech-builder-battler`. It is project-specific and belongs in
the project wiki, not reusable `knowledge/`.

Development/iteration target: native PC harness with mobile-style controls,
readability, and session length. Web/mobile export remains deferred.

## Study Mode

Mode: mechanics/meta synthesis audit.

Inputs:

- [Mobile mech analogs](../references/mobile_mech_analogs_2026-06-19.md)
- [Mech Arena deconstruction](../references/mech_arena_deconstruction_2026-06-19.md)
- [CATS deconstruction](../references/cats_deconstruction_2026-06-19.md)
- [Mechangelion deconstruction](../references/mechangelion_deconstruction_2026-06-19.md)
- [GDD draft](gdd_draft_2026-06-19.md)
- [First slice spec](first_slice_spec_2026-06-19.md)
- Fresh store-page check on 2026-06-19 for Mech Arena, CATS, and War Robots.

Implementation boundary:

- No code, no final art, no economy tuning from this doc alone.
- Store pages prove market-facing features and risk surfaces, not exact first
  minute pacing or balance.
- Exact UI, economy values, enemy timings, and final art still need fake shots,
  gameplay evidence, or native PC slice proof.

## Fresh Source Refresh

| Source | Evidence label | Checked | What it supports | What it does not prove |
|---|---|---:|---|---|
| Mech Arena Google Play, `https://play.google.com/store/apps/details?id=com.plarium.mechlegion&hl=en_US` | observed store page | 2026-06-19 | 25+ mechs, 90+ weapons, hangar/build fantasy, custom TPS controls, special abilities, pilots/implants, events/tournaments, short battles, mobile/desktop account continuity, ads/IAP/random-item pressure | First-session tutorial, exact HUD hierarchy, exact economy pacing |
| CATS Google Play, `https://play.google.com/store/apps/details?id=com.zeptolab.cats.google&hl=en_US` | observed store page | 2026-06-19 | build-and-battle premise, attachments/weapons, lab/engineering fantasy, 1v1 arena, ranking, customization, casual/stylized positioning, ads/IAP/random-item pressure | Whether battles are direct-control now, first-session crate cadence, exact part balance |
| War Robots Google Play, `https://play.google.com/store/apps/details?id=com.pixonic.wwr&hl=en_US` | observed store page | 2026-06-19 | large robot combat demand, over-50 robot breadth, weapons/modules, style expression, clans, solo/PvP modes, ads/IAP/random-item pressure, heavy live-service/event surface | First-slice suitability, PvE pacing, casual onboarding, fair economy |

## Reference Read

### What The Genre Expects

The mech fantasy is not just "robot shoots enemy". The minimum expected verbs
are:

- choose a machine or build;
- mount weapons/modules;
- fight and see the build matter;
- earn parts/currency;
- return to the hangar;
- make the mech visibly stronger or different;
- test the change.

For our casual target, this becomes one short loop:

> Hangar choice -> readable fight -> reward -> visible install/upgrade -> retest.

### What The Genre Overloads

The mobile analogs also converge on systems that are too heavy for our first
slice:

- multiple robot roster;
- pilots/implants/co-pilots;
- clans, events, tournaments, ranked ladders;
- random-item monetization;
- multiple currencies and offer surfaces;
- deep weapon/module catalogs before the player understands one mech;
- PvP matchmaking and balance pressure.

These are evidence that the market accepts depth, but they are anti-scope for a
casual first slice.

## Core Loop Contract

### Moment Loop

| Step | Player action | Screen feedback | Design purpose |
|---|---|---|---|
| Aim/position | Move, dash, or choose danger zone | Mech turns, legs move, enemy telegraph changes | Preserve pilot agency without precision shooter demands |
| Fire cycle | Primary attack auto-fires or uses one large button | Hit sparks, damage ticks, target stagger | Make build power visible every few seconds |
| Burst choice | Trigger special/module | Rockets, shield, dash, beam, or vent effect | Give the player one memorable active decision |
| Cooldown/heat | Wait/reposition briefly | Heat bar/vents cool down | Add rhythm without ammo spreadsheets |
| Kill/clear | Enemy dies or wave ends | Explosion, salvage burst, progress tick | Tie combat result to reward |

### Session Loop

| Step | Required first-slice proof |
|---|---|
| Hangar | One mech, one dominant `Battle` action, 3-5 clear slots |
| Battle | 45-90 second PvE fight, no precision aim requirement |
| Reward | Salvage/resources; hangar purchase/craft destination visible |
| Upgrade | One tap guided buy/craft/install or upgrade; model/effect changes |
| Retest | Second battle prompt proves the change immediately |

### Meta Loop

| Layer | First-slice version | Later depth |
|---|---|---|
| Power | Upgrade one part or equip one visible module | Rarity, tiers, synergy, set bonuses |
| Style | Choose between weapon/module archetypes | Full build families |
| Mastery | Learn enemy telegraphs and heat rhythm | Boss modifiers, optional challenges |
| Collection | One purchased/crafted part or blueprint unlock | Catalog, crafting, cosmetics |

## Mech Assembly Model

First-slice assembly should be slot-light and physical:

| Slot | First-slice role | Visible change | Gameplay change | Defer |
|---|---|---|---|---|
| Core/body | Defines health and silhouette | Torso/chest shape | HP/heat capacity | Multiple chassis classes |
| Main weapon | Default damage language | Arm cannon/barrel/beam | Range, fire rate, hit style | 90+ weapon catalog |
| Legs/drive | Movement feel | Treads, biped legs, hover base | Speed, dash, knockback resistance | Fine-grained weight sim |
| Special module | One build identity button | Shoulder rockets, shield pod, vent stack | Burst, defense, control | Pilot/implant modifiers |
| Paint/decal | Ownership and reward charm | Color/accent/decal | No first-slice stats | Cosmetic shop |

Rule: if a part cannot be seen on the mech or in the attack effect, it should
not be the first guided purchase/craft.

Visual rule: part visibility depends on real 3D presentation. Shoulder rockets,
drone enemies, and the starter mech need model-like geometry, lighting,
shadows, normals/bevels, emissive accents, juicy effects, and material contrast
in the first playable proof.

## Combat Model

Recommended first model: semi-auto PvE arena.

Why:

- Mech Arena and War Robots prove active robot combat demand, but their live
  PvP complexity is too high.
- CATS proves build -> proof -> rebuild, but no-agency battle would weaken the
  pilot fantasy.
- Semi-auto keeps the player involved with movement/specials while avoiding
  precise shooter aim on touch.

### Control Budget

First fight controls:

- movement or reposition zone;
- one special button;
- one short dash button;
- heat feedback.

No first-slice controls:

- manual weapon swapping;
- manual aim reticle precision;
- squad commands;
- consumables;
- pilot skills;
- multiple specials.

### Enemy Role Matrix

| Enemy | What it teaches | Counter-build signal | First-slice status |
|---|---|---|---|
| Drone swarm | Area damage and target switching | Rockets clear groups | Include |
| Shield guard | Positioning or stagger | Cannon stagger or flanking | Include if readable |
| Charger | Dash/brace timing | Shield/brace blocks impact | Include if shield is first purchasable module |
| Sniper turret | Danger line and cover | Dash or long-range weapon | Defer unless battle reads clearly |
| Industrial mini-boss | Spectacle and pattern read | Use first purchased/crafted module | Optional after loop is proven |

## Progression And Grind

The grind should be visible, short, and tied to playstyle. It should not be
"number goes up in a hidden spreadsheet".

### Resource Model

| Resource | Source | Sink | First-slice use | Risk |
|---|---|---|---|---|
| Salvage | Battle clear, first-clear bonus | Upgrade/install part | Primary soft resource | Can become generic coin grind |
| Blueprint | First clear, mission target | Unlock one part/module | Explains part progress | Can become random-fragment frustration |
| Core cell | Boss/challenge reward | Major tier upgrade | Defer | Adds currency clutter |
| Paint chip | Optional reward | Cosmetic ownership | Defer or fake-shot only | Distracts from build proof |

First loop should show salvage/resources first, then one guided module
purchase/craft or optional blueprint progress in the hangar. More currencies
should wait until the player understands the build loop.

### Upgrade Cadence

| Time window | Expected player feeling | Allowed systems |
|---|---|---|
| First 2-4 minutes | "I earned resources, chose a thing, and bolted it on" | Guided purchase/craft, one visible module |
| First 10 minutes | "I can choose a style" | Two or three part options, simple upgrade |
| First session | "My build beats some enemies better than others" | Enemy roles, one optional challenge |
| Later | "I am shaping a garage identity" | Archetypes, cosmetics, deeper part families |

### Grind Guardrails

- Every early grind step must name what it unlocks.
- No random-item pressure before the second battle prompt.
- No ad multiplier or offer surface in the first loop.
- No upgrade that changes only a hidden stat.
- No failure state that says "grind more" before the first build proof.

## Build Archetypes

| Archetype | Parts | Player promise | Enemy proof | Risk |
|---|---|---|---|---|
| Striker | Cannon, balanced legs, dash | Simple direct damage | Drone + shield guard | Too generic if effects are weak |
| Missile Breaker | Shoulder rockets, slower legs | Clears groups, big spectacle | Drone swarm | Can trivialize first wave |
| Shield Bruiser | Shield pod, heavy legs | Survives telegraphed hits | Charger | Can feel passive |
| Laser Ranger | Beam, light legs | Sustained focus damage | Sniper/turret or guard | Can demand too much aim |

First slice should prove only one archetype change. Shoulder rockets are the
accepted first purchasable module, and drone swarm is the accepted second fight
proof.

## MVP Mechanics Matrix

| System | First slice | Later | Reject for first version |
|---|---|---|---|
| Player mech count | One owned mech | Optional alternate chassis | Squad roster |
| Combat | PvE semi-auto arena | Optional challenge modes | PvP matchmaking |
| Targeting | Auto-target/highlight | Manual target cycling | Precision shooter aim |
| Tactical limiter | Heat mechanics with `Cooling` UI label | Part-specific overheat behavior | Ammo inventory |
| Rewards | Salvage/resources -> guided first module purchase/craft | Blueprints, cosmetics | Chest timers as first reward |
| Upgrades | Guided one-slot install | Upgrade tree/tiers | Pilot/implant stack |
| Meta | Mission path with enemy roles | Events/challenges | Clans/tournaments/battle pass |
| Economy | 1-2 visible resources | Crafting/cosmetic resources | Five-currency live-service economy |

## Translation Decisions

Borrow:

- hangar as ownership hub;
- physical part slots;
- short battle proof;
- special module identity;
- visible combat feedback;
- reward-to-slot return.

Simplify:

- one mech instead of roster;
- one active special instead of ability stack;
- one soft resource plus one guided first module purchase/craft;
- PvE roles instead of PvP meta;
- guided equip instead of full MechLab.

Avoid:

- pilots/implants/co-pilots in the first slice;
- clans, tournaments, leaderboards, seasonal events;
- random-item pressure before the loop is understood;
- popups/offers as session entry;
- exact UI layouts, names, silhouettes, or monetization flows from references.

## Native PC Slice Proof

The first native PC slice should prove mobile-style design with screenshots:

1. Hangar: one mech, part slots, battle action.
2. Battle: readable 3D mech, enemy roles, large controls, heat.
3. Special: first purchased/crafted module visibly changes effect.
4. Reward: salvage/resources and hangar purchase/craft destination.
5. Upgrade: visible install or upgraded effect.
6. Retest prompt: second battle framed as "try the new build".

If those screens do not read at phone scale, the mechanics are not simple
enough yet.

## Open Decisions

- First input model: accepted floating virtual joystick / drag movement zone
  for mobile target; WASD for native PC harness.
- First purchasable module: accepted shoulder rockets.
- First limiter label: accepted `Cooling`, backed by heat mechanics.
- First orientation: accepted landscape-first.
- First mini-boss: accepted Foundry Warden industrial machine.

## Reference Digest

- Mode: mechanics/meta synthesis audit.
- Sources checked: project deconstructions plus refreshed Google Play pages for
  Mech Arena, CATS, and War Robots on 2026-06-19.
- Observed facts:
  - Mech Arena store page exposes a large mech/weapon catalog, custom controls,
    special abilities, pilots/implants, events, and short battles.
  - CATS store page centers build-and-battle, attachments/weapons, lab
    tuning, 1v1 arena, ranking, and customization.
  - War Robots store page exposes robot/weapon/module breadth, clans, solo/PvP
    modes, ads/IAP/random-item pressure, and live event surfaces.
- Current-build mismatch: no native PC mech slice exists yet; the current seed
  has no hangar, battle, reward, upgrade, or retest proof.
- Borrow: hangar, physical parts, short proof battle, reward-to-slot return.
- Avoid: roster/PvP/live-service/pilot/implant/event complexity in first slice.
- Copy-risk: exact mech silhouettes, UI layouts, names, and monetization flows.
- Next native proof: phone-scale screenshot sequence for hangar, battle,
  special, reward, upgrade, and retest prompt.
