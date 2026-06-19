---
type: Project Design Review
title: Mech Builder Battler Design Review
description: Review verdict for the current reference, GDD, mechanics, meta, fake-shot, and first-slice package.
tags: [project, design-review, gdd, references, mechanics, meta, mobile, web, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# Design Review

Status: review verdict for the current concept package.

Scope: this reviews the current project wiki for `mech-builder-battler`. It
does not accept the design on behalf of the lead and does not unlock runtime
implementation.

## Inputs Reviewed

- [Project README](../README.md)
- [Mobile mech analogs](../references/mobile_mech_analogs_2026-06-19.md)
- [Mech Arena deconstruction](../references/mech_arena_deconstruction_2026-06-19.md)
- [CATS deconstruction](../references/cats_deconstruction_2026-06-19.md)
- [Mechangelion deconstruction](../references/mechangelion_deconstruction_2026-06-19.md)
- [Visual reference packet](../references/visual_reference_packet_2026-06-19.md)
- [Reference evidence capture plan](../references/reference_evidence_capture_plan_2026-06-19.md)
- [Core loop and meta decomposition](core_loop_gameplay_meta.md)
- [GDD draft](gdd_draft_2026-06-19.md)
- [First slice spec](first_slice_spec_2026-06-19.md)
- [Mechanics and meta matrix](mechanics_meta_matrix_2026-06-19.md)
- [Lead review packet](lead_review_packet_2026-06-19.md)
- [Reference traceability audit](reference_traceability_audit_2026-06-19.md)
- [Fake-shot art request](fake_shot_art_request_2026-06-19.md)

## Verdict

The package is strong enough for lead decision review and concept fake-shot
work. It is not strong enough for exact implementation.

The strongest current shape is:

> One owned modular 3D mech, PvE-first, short semi-auto battles,
> resource-to-purchase-to-slot upgrade loop, visible part/effect changes, native
> PC iteration with mobile/web UX constraints.

This is coherent with the user's request: 3D mechs, battles, grind, upgrades,
assembly for playstyle, casual mobile/web expectations, and native PC iteration.

## What Is Working

### Clear Player Need Lane

The first screen and first minute are correctly owned by accessible progression:

- one mech;
- obvious `Battle` action;
- short PvE proof;
- visible reward;
- guided install/upgrade;
- immediate retest prompt.

Active mastery appears later through enemy roles, heat rhythm, build choices,
and optional challenge tuning. This avoids mixing too much mastery pressure into
the first minute.

### Genre Expectation Is Preserved

The current design does not flatten the mech fantasy into a generic shooter:

- physical part slots matter;
- weapons/modules change silhouette or effects;
- hangar is ownership hub;
- combat proves the current build;
- upgrades return to the model, not only to a stat row.

This preserves the expectation that a mech game is about machine identity and
loadout consequences.

### Casual Translation Is Mostly Correct

The design removes the heaviest reference risks:

- no PvP service as the first mode;
- no pilot/implant stack;
- no roster management in first loop;
- no offer/ad/chest pressure before build proof;
- no precision shooter aim requirement;
- no full MechLab in the first minute.

### Scope Is Honest

The docs consistently state that the current reference study is source-packet
incomplete and that exact UI/economy/combat pacing still needs evidence boards
or native PC proof.

## Main Risks

### P0: Review Queue Can Look Like Acceptance

There are many tasks in `review`, but the lead has not accepted the central
decisions. A later agent could mistake "review task complete" for "design
approved".

Required guardrail:

- Keep implementation blocked until a decision checklist is explicitly accepted
  or edited.
- Do not promote `gdd_draft_2026-06-19.md` to `GDD.md` yet.

### P0: References Are Directional, Not Implementation-Ready

The reference docs support direction, but not exact UI/combat/economy copying.
Store pages and secondary sources do not prove first-session timing, reward
cadence, HUD hierarchy, or economy pressure.

Required guardrail:

- Fill evidence boards for Mech Arena and CATS before exact implementation
  claims.
- Keep Mechangelion weak unless stronger gameplay evidence is captured.
- Treat War Robots as scale/anti-pattern unless a specific PvE capture is
  studied.

### P1: Semi-Auto Arena Is The Right Bet, But Not Proven

Semi-auto arena is the best current compromise between Mech Arena action and
CATS build proof. The first input model is accepted as floating virtual
joystick / drag movement zone on mobile target, with WASD in the native PC
harness, but the feel still needs prototype proof.

Risk:

- direct movement can become too demanding for casual mobile;
- too much auto-combat can weaken the mech-pilot fantasy.

Prototype proof needed:

- prove that movement, one special, and one defense/mobility action are still
  readable and forgiving at phone scale.

### P1: Shoulder Rockets Are Visually Strong But Balance-Risky

Shoulder rockets are the accepted first purchasable module because they change
silhouette and answer drone swarms. They can also imply too much power too
early.

Mitigation:

- first rocket module should prove a visible improvement, not a full power
  fantasy;
- second battle should show drones clearing faster, not every enemy melting.

### P1: Heat Needs Clear `Cooling` Presentation

Heat fits the mech fantasy and gives good visual feedback. The accepted UI
label is `Cooling`, so the prototype must clearly show that burst actions heat
the mech and waiting lets it cool down.

Mitigation:

- fake shot can show heat visually through amber vents;
- prototype UI labels the limiter `Cooling`;
- amber vents and warning pulses must make the heat risk readable.

### P1: Tone Can Drift Into Gray Industrial Noise

Industrial salvage sport is coherent, but it can become muddy if the visual
direction overuses gray metal and grime.

Mitigation:

- keep cyan reactor accents, amber heat, gold/green rewards, and bright enemy
  role colors;
- reject fake shots where the mech silhouette is hidden by realistic greeble.

### Visual Fidelity Is A Core Requirement

The mech fantasy depends on model presence. The first playable should use
real/model-like 3D assets, lit materials, shadow contact, bevel/normal detail,
emissive accents, and readable modular silhouettes as early as possible. Debug
shapes can help engineering, but they cannot be the accepted product
screenshot.

## Decision Quality

### Accept Unless Lead Rejects

These are strong enough to treat as default for the next review pass:

- one owned player mech first;
- PvE first;
- hangar -> battle -> reward -> upgrade -> retest;
- first battle reward is resources/salvage, followed by guided hangar
  purchase/craft;
- no PvP/service monetization clutter in first loop;
- native PC implementation/iteration with mobile/web UX constraints;
- fake shots required before final art;
- no exact copying of reference UI, names, silhouettes, or monetization flows.

### Accept For Fake Shots, Then Test

These are good hypotheses, not final proof:

- landscape-first fake shots/native slice;
- semi-auto arena;
- auto-targeting/no precision aim;
- fixed three-quarter/isometric camera;
- shoulder rockets as first purchasable module;
- heat mechanics with `Cooling` UI label;
- industrial machine mini-boss;
- industrial salvage sport tone.

### Do Not Decide Silently

These need explicit lead choice or prototype evidence:

- exact input model;
- whether the first fake shots need another visual iteration;
- whether Mechangelion remains a reference after evidence capture;
- whether the GDD draft becomes canonical `GDD.md`.

## Required Next Evidence

Before exact implementation:

1. Mech Arena evidence board:
   hangar, battle HUD, special, result/reward, upgrade, friction.
2. CATS evidence board:
   build screen, slot attachment, battle proof, reward, upgrade/fusion,
   timer/ad/box friction.
3. Fake-shot target:
   hangar, battle, reward/upgrade.
4. Native PC slice scope:
   first input model, proof screens, acceptance gate.

## Recommended Lead Review Flow

Use one short pass instead of reviewing every document independently:

1. Accept or reject the core promise:
   one owned modular mech -> short PvE proof -> visible part upgrade -> retest.
2. Pick the fake-shot target:
   landscape-first semi-auto arena with resource reward -> guided shoulder
   rocket purchase -> drone swarm proof.
3. Decide whether Mechangelion is strong enough to keep, or demote it to weak
   simplicity reference.
4. Produce fake shots and/or start the rough native PC slice.

## Implementation Stop Rules

Do not start a native PC implementation task until all are true:

- lead accepts or edits the core decisions;
- rough native slice follows the accepted visual direction; final art polish
  waits for accepted visual target evidence;
- first input model is chosen for the slice;
- implementation task is scoped from the accepted first-slice spec;
- exact reference copying remains blocked unless evidence boards are filled.

Do not start web/mobile export, frontend path, or engine/tool work without
explicit approval.

## Review Summary

The concept is viable and better than a generic mech shooter because it centers
the build-battle-upgrade loop around one visible machine. The main work now is
not inventing more systems. The main work is narrowing decisions, accepting a
visual target, and proving the riskiest assumptions with evidence boards or a
small native PC slice.
