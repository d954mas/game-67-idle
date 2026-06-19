---
type: Project Decision Handoff
title: Mech Builder Battler Decision Handoff
description: Compact accept/edit packet that turns the current design review into a start-ready native PC slice decision.
tags: [project, decisions, handoff, first-slice, native-pc, mobile, web, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# Decision Handoff

Status: partial lead acceptance in progress.

Accepted on 2026-06-19:

- Core promise: one owned mech, PvE first, hangar -> battle -> reward ->
  upgrade -> second battle/retest.
- First control model: semi-auto arena.
- First input: floating virtual joystick / drag movement zone for mobile
  target, WASD for native PC harness.
- First reward structure: battle grants resources/salvage, then the player
  buys or crafts the first module in the hangar.
- First purchasable module: shoulder rockets.
- First proof enemy: drone swarm.
- First defense/mobility action: short dash.
- Limiter UI label: `Cooling` for player-facing UI, backed by heat mechanics.
- Camera: fixed three-quarter/isometric.
- Orientation: landscape-first.
- Tone: industrial salvage sport with bright accents.
- First mini-boss: Foundry Warden industrial machine.

Still pending:

- generate or accept fake-shot visuals;
- create the first native PC implementation task.

Purpose: reduce the current pre-production package to the smallest decision set
needed to start the first native PC slice without losing the mobile/web target.

## Inputs

- [Design review](design_review_2026-06-19.md)
- [Lead review packet](lead_review_packet_2026-06-19.md)
- [First slice spec](first_slice_spec_2026-06-19.md)
- [Mechanics and meta matrix](mechanics_meta_matrix_2026-06-19.md)
- [Fake-shot art request](fake_shot_art_request_2026-06-19.md)
- [Reference evidence capture plan](../references/reference_evidence_capture_plan_2026-06-19.md)
- [Mobile control patterns](../references/mobile_control_patterns_2026-06-19.md)

## Recommended Accept Packet

If the lead accepts the current direction, the first slice should be:

> Native PC prototype of a landscape-first, PvE-first, one-mech, semi-auto 3D
> arena battler where the first battle grants salvage/resources, the hangar
> guides the player to buy or craft a first module, then a second fight proves
> the change.

This packet keeps mobile/web as the UX target:

- touch-scale controls;
- phone-readable UI density;
- short session length;
- readable silhouettes and effects;
- vivid 3D mech presentation: real/model-like 3D assets, lights, shadows,
  normals, bevels, painted metal, material response, and juicy combat effects
  are genre expectations from the first playable, not late polish;
- no web/mobile export work until explicitly approved.

## Decision Ballot

Mark or edit these before implementation.

| Decision | Recommended value | Alternatives | Start impact |
|---|---|---|---|
| Core promise | accepted: one owned modular mech -> short PvE proof -> visible upgrade -> retest | squad, PvP, auto-only | Accepted on 2026-06-19 |
| First mode | accepted: PvE | PvP, async comparison | Accepted on 2026-06-19 |
| First control model | accepted: semi-auto arena | tactical auto-move, CATS-like sim | Accepted on 2026-06-19 |
| First input | accepted: floating virtual joystick / drag movement zone for mobile target; WASD for native PC harness | fixed virtual stick, tap-to-move, auto-move | Accepted on 2026-06-19 |
| First reward structure | accepted: resources/salvage -> guided hangar purchase/craft | direct module drop, random chest | Accepted on 2026-06-19 |
| First purchasable module | accepted: shoulder rockets | shield module, legs, paint | Accepted on 2026-06-19 |
| First proof enemy | accepted: drone swarm | charger, shield guard | Accepted on 2026-06-19 |
| First defense/mobility | accepted: short dash | shield, brace | Accepted on 2026-06-19 |
| First limiter | accepted: heat mechanics with `Cooling` UI label | energy/power meter | Accepted on 2026-06-19 |
| Camera | accepted: fixed three-quarter/isometric | behind-shoulder-lite | Accepted on 2026-06-19 |
| Orientation | accepted: landscape-first for fake shots and native slice | portrait-first, responsive | Accepted on 2026-06-19 |
| Tone | accepted: industrial salvage sport with bright accents | heroic anime, toy combat | Accepted on 2026-06-19 |
| First mini-boss | accepted: Foundry Warden industrial machine | rival mech, turret rig | Accepted on 2026-06-19 |

Recommended concrete choices for first implementation:

- input: floating virtual joystick on a broad left-side movement zone;
- PC harness input: WASD movement;
- actions: one special button, one short dash button;
- reward: salvage/resources enough to buy or craft the first module;
- first purchasable module: shoulder rockets;
- enemy proof: drone swarm;
- label: use `Cooling` in UI for the heat limiter;
- camera: fixed three-quarter/isometric;
- orientation: landscape-first;
- tone: industrial salvage sport with bright accents;
- first mini-boss: Foundry Warden industrial machine;
- fake shots: hangar, battle, reward/upgrade, all landscape-first.

## Start-Ready Native PC Slice

Once the ballot is accepted or edited, create one implementation task with this
scope:

### Goal

Build a native PC playable proof of:

> hangar -> battle -> reward -> equip/upgrade -> second battle prompt.

### Required Screens

1. Hangar:
   - one modular mech;
   - visible part slots;
   - dominant battle action;
   - locked/empty shoulder module hint.
2. Battle:
   - small 3D arena;
   - readable mech and drone enemies;
   - large mobile-style control zones;
   - heat/cooling feedback;
   - one special and one short dash action.
3. Reward:
   - salvage/resources gained;
   - affordable first module or craft action shown in the hangar;
   - slot destination shown before purchase/install.
4. Upgrade:
   - purchased/crafted module installed or visually previewed;
   - model/effect changes.
5. Retest:
   - second battle prompt explicitly says to test the new part.

### Required Proof

- native PC screenshot or capture of all five required screens;
- phone-scale crop/readability check;
- no offers, ads, PvP, pilots, implants, chest timers, clans, battle pass, or
  random-item pressure in the first loop;
- final screenshot sequence reads as a game, not a debug template.

## What Can Wait

Do not block first implementation on:

- full Mech Arena/CATS evidence boards, as long as implementation avoids exact
  copying and stays within the translated first-slice spec;
- final economy values;
- final art assets, but the prototype should still use GLB/model-like 3D
  assets, lighting, shadows, and material response rather than debug-shape
  presentation;
- web/mobile export;
- full title/brand;
- monetization;
- multiple mech roster;
- deep part catalog.

Still do before exact reference copying:

- fill evidence boards and observation ledgers for any reference-specific UI,
  economy, pacing, or art claim.

## Immediate Next Work Options

### Option A: Accept And Start Native PC Slice

Use if the lead accepts the recommended ballot.

Next action:

- create a scoped implementation task from this handoff and
  [first slice spec](first_slice_spec_2026-06-19.md);
- implement native PC only;
- keep web/mobile export deferred.

Risk:

- fake shots are being generated as visual targets; first runtime visuals can
  start rough but must stay phone-readable and match the accepted direction.

### Option B: Generate/Accept Fake Shots First

Use if visual target risk is the main concern.

Next action:

- accept or edit [fake-shot art request](fake_shot_art_request_2026-06-19.md);
- generate concept fake shots for hangar, battle, reward/upgrade;
- then start native PC slice against those images.

Risk:

- delays playable proof, but reduces visual drift.

### Option C: Fill Evidence Boards First

Use if exact reference grounding matters before any prototype.

Next action:

- follow [reference evidence capture plan](../references/reference_evidence_capture_plan_2026-06-19.md);
- fill Mech Arena and CATS evidence boards;
- demote or strengthen Mechangelion.

Risk:

- delays making the game; useful only if the lead wants stronger proof before
  accepting the first-slice direction.

## Recommended Path

Recommended: Option A with one caveat.

Start the native PC slice after lead accepts/edits the decision ballot, while
keeping implementation deliberately translated rather than copied from refs.
Use fake shots or evidence boards in parallel only for visual/reference risk,
not as a blocker for proving the basic loop.

## Acceptance Shortcut

If the lead says:

> Accept current mech direction. Start native PC slice.

Then the accepted defaults are:

- one mech;
- PvE first;
- landscape-first;
- semi-auto arena;
- floating virtual joystick / drag movement zone;
- WASD for native PC harness;
- auto-target;
- resources/salvage as the first battle reward;
- guided first module purchase/craft in the hangar;
- shoulder rockets as the first purchasable module;
- drone swarm proof;
- short dash as starter defense/mobility;
- heat mechanics with `Cooling` UI label;
- fixed three-quarter/isometric camera;
- industrial salvage sport tone;
- Foundry Warden industrial mini-boss;
- no web/mobile export yet.

If any of these are wrong, edit them before starting implementation.
