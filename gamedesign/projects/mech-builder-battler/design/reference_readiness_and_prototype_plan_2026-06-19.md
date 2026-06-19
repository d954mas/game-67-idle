---
type: Project Design Review
title: Reference Readiness And Prototype Plan
description: Evidence-backed readiness audit and first playable decomposition for Mech Builder Battler.
tags: [project, references, readiness, prototype, core-loop, mechanics, meta, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# Reference Readiness And Prototype Plan

Scope: consolidate the current research, review, reference deconstruction,
core-loop, gameplay, mechanics, and meta work into a production-facing plan for
the first native PC playable slice.

This document does not replace the GDD. It answers one practical question:
what can be implemented now without pretending that incomplete reference
packets prove exact UI, economy, balance, or final art?

## Verdict

The project is ready to start a first native PC prototype slice, with a strict
boundary:

- ready now: original hangar -> battle -> reward -> upgrade -> retest loop,
  based on accepted lead decisions and project synthesis;
- not ready yet: exact Mech Arena, CATS, or Mechangelion UI/economy/combat
  copying, because the central deconstructions still lack current owned
  gameplay timestamps, screenshot boards, and a current-build mismatch capture;
- visual bar: the prototype must use model-like 3D assets, lighting, shadows,
  normals/material response, emissive accents, and juicy combat feedback;
  debug shapes are allowed only as temporary scaffolding and cannot pass visual
  proof.

## Source Readiness Matrix

| Reference | Current role | Evidence strength | Borrow now | Still blocked |
|---|---|---|---|---|
| [Mech Arena](../references/mech_arena_deconstruction_2026-06-19.md) | short-session mobile mech action and hangar grammar | official/store/support plus secondary visual page; source-packet incomplete | hangar as home, short battle, virtual joystick/action-button grammar, special/module identity, anti-scope for pilots/implants/events | exact HUD layout, first-session tutorial, economy pacing, battle timing |
| [CATS](../references/cats_deconstruction_2026-06-19.md) | casual buildcraft and quick battle proof | store/guide/secondary visuals; source-packet incomplete | build -> quick proof -> reward -> rebuild loop, physical slots, readable counter-build idea | exact reward box cadence, garage UI layout, current monetization/interruption flow |
| [Mechangelion](../references/mechangelion_deconstruction_2026-06-19.md) | ultra-casual robot spectacle and boss simplicity | weakest packet; store/secondary evidence only | few large combat buttons, oversized enemy spectacle, weapon/defense upgrade clarity | exact first launch, battle HUD, boss pacing, economy |
| [Mobile controls](../references/mobile_control_patterns_2026-06-19.md) | accepted first input model | enough for first prototype adapter | floating virtual joystick / drag movement zone on mobile target; WASD in native PC harness | custom control editor, exact HUD placement |
| [Visual packet](../references/visual_reference_packet_2026-06-19.md) and [visual target review](visual_target_review_2026-06-19.md) | visual target and fake-shot bar | enough for first visual direction, not final runtime asset proof | vivid stylized 3D mech, industrial salvage sport, clear silhouettes, bright effects, hangar hero framing | final asset sourcing, runtime screenshot gate |

## Accepted Decisions

These decisions are accepted enough for the first prototype:

- one owned mech;
- PvE first;
- native PC harness for fast implementation and iteration;
- mobile/web remain UX targets, not the current implementation surface;
- landscape-first three-quarter/isometric camera;
- semi-auto arena combat;
- floating virtual joystick / drag movement zone on mobile target;
- WASD movement in the PC harness;
- auto-target/highlight instead of precision shooter aim;
- one short dash;
- `Cooling` UI label for heat/overheat rhythm;
- battle rewards salvage/resources;
- the player buys/crafts the first module in the hangar;
- first purchasable module: shoulder rockets;
- second battle proves rockets against drone swarm;
- industrial salvage sport tone;
- first mini-boss target: Foundry Warden industrial machine.

## Core Loop Decomposition

### Moment Loop

1. Player repositions the mech with WASD in the native harness.
2. Auto-target selects or highlights the nearest/highest-priority enemy.
3. Primary cannon fires with clear recoil, sparks, and hit feedback.
4. Player triggers dash or shoulder rockets.
5. Heat rises; `Cooling` feedback asks the player to briefly reposition or wait.
6. Enemy dies with explosion/salvage feedback.

Prototype proof: the player can understand the loop in one battle screenshot
without reading a manual.

### Session Loop

1. Hangar shows the owned mech and one clear `Battle` action.
2. Battle clears 3-5 drones and optionally one shield/charger role.
3. Reward grants salvage/resources.
4. Hangar highlights the shoulder rocket purchase/craft.
5. The mech silhouette or attack effect changes after equip.
6. Second battle asks the player to test the rockets against drones.

Prototype proof: capture six screens in order: hangar, battle start, special
effect, reward, upgraded hangar, retest prompt.

### Meta Loop

Battle -> salvage -> buy/craft/equip part -> unlock or retest mission ->
compare build result.

First slice meta must stay narrow:

- one soft resource;
- one guided module purchase/craft;
- no chest timer;
- no ad multiplier;
- no PvP matchmaking;
- no pilot/implant/roster layer.

## Gameplay Decomposition

### Systems Needed For Prototype 1

| System | Minimum playable behavior | Visual proof |
|---|---|---|
| Hangar state | mech visible, battle prompt, one locked/affordable module slot | mech is largest object; shoulder slot reads as a real part location |
| Player movement | WASD movement tuned to heavy but responsive feel | legs/body motion or ground movement sells weight |
| Auto-target | nearest/high-priority enemy highlighted | target ring, outline, or aim beam is readable |
| Primary cannon | simple repeated attack | muzzle flash, recoil, sparks, damage feedback |
| Dash | short defensive/mobility action | blur/boost trail, cooldown/heat response |
| Heat/Cooling | special use raises heat; cooldown recovers | `Cooling` meter/vent glow is readable |
| Shoulder rockets | purchased/equipped after first reward | visible shoulder pod or distinct rocket burst |
| Enemy wave | drones move/attack/die | drones are smaller than mech and readable as swarm |
| Reward | salvage amount and next destination | reward points to shoulder module, not generic coin grind |

### Systems Deferred

- exact mobile control customization;
- manual aim;
- squad roster;
- pilots/implants;
- PvP;
- event/tournament/live-service surfaces;
- random chest timers;
- multi-currency economy;
- full MechLab spreadsheet.

## Mechanics And Meta Guardrails

- Every first-loop upgrade must be visible on the model or in the effect.
- The first loss cannot require grinding or payment.
- The first reward cannot be a timer chest.
- The first purchase/craft must be deterministic.
- Combat has no more than three active controls in the first fight.
- If a mechanic cannot be explained by a screenshot or one short animation, it
  waits.
- Enemy roles teach build meaning: drones prove rockets; charger proves dash or
  shield; shield guard proves stagger/flank; Foundry Warden proves the whole
  first loop only after the basic loop reads.

## Visual Prototype Contract

- Goal: vivid 3D mech slice that already reads like a mech game, not a debug
  sandbox.
- Non-goal: final production asset set, mobile export, exact reference HUD, or
  monetization UI.
- Proof: native screenshot sequence plus visual mismatch audit against the fake
  shots and reference packets.
- Stop condition: if the native screenshot reads as tooling, debug shapes, or
  unclear UI, feature/content expansion freezes until the visual mismatch is
  fixed.
- Likely files: `src/clean_seed_main.c`, project asset folders, future
  `gamedesign/projects/mech-builder-battler/evidence/` captures, and
  `tasks/active/` implementation tasks.

## Reference Gaps To Close After Prototype Screenshot

The next research pass should compare a real native capture against the
reference packets. Until then, the current-build mismatch is:

- no playable hangar;
- no playable battle;
- no reward screen;
- no visible upgrade;
- no runtime 3D mech model;
- no screenshot proof that phone-scale controls/readability work.

After the first capture exists, update the central deconstructions with:

1. current native screenshot path;
2. mismatch list against Mech Arena, CATS, and Mechangelion;
3. screenshot evidence board for our own first loop;
4. one next scoped code/art proof.

## Implementation Task Decomposition

Recommended order:

1. **Prototype skeleton:** replace the clean seed surface with a simple 3D
   hangar/battle state machine and DevAPI screenshot hooks.
2. **Model-first visual baseline:** integrate or generate starter mech, drone,
   arena, and hangar assets with lighting/materials before polishing UI.
3. **Battle loop:** WASD movement, auto-target, primary cannon, dash, heat,
   drone wave, win condition.
4. **Reward and upgrade:** salvage reward, guided shoulder rocket purchase,
   equip state, visible model/effect change.
5. **Retest proof:** second battle where rockets clear drones faster and the
   screenshot sequence proves the loop.
6. **Review pass:** visual strict gate, source mismatch update, and task
   closure only after screenshot evidence exists.

## Readiness Digest

- Mode: reference readiness audit plus prototype decomposition.
- Sources checked: current project deconstructions, control packet, visual
  packet, first-slice spec, mechanics/meta matrix, and fresh store/support page
  checks for the central mobile analogs on 2026-06-19.
- Observed/source-backed facts: mobile mech refs foreground short combat,
  mech/robot/weapon customization, hangar or build surfaces, and progression;
  CATS-like buildcraft proves quick build feedback; Mechangelion-like robot
  games show that simple robot spectacle can work for broad casual audiences.
- Current-build mismatch: there is no current mech prototype capture yet.
- Borrow: hangar hero object, short proof battle, physical part slots, visible
  reward-to-upgrade return, large simple controls, juicy mech effects.
- Avoid: PvP service, roster bloat, pilots/implants, chest timers as first
  reward, forced ads, exact UI/silhouette copying.
- Next proof: native PC screenshot sequence for hangar -> battle -> special ->
  reward -> upgraded hangar -> retest prompt.
