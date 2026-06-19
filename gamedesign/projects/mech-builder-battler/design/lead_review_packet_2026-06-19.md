---
type: Project Lead Review Packet
title: Mech Builder Battler Lead Review Packet
description: Decision packet for reviewing the first-pass GDD and first-slice spec before fake shots or implementation.
tags: [project, review, decisions, gdd, mvp, mobile, web, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# Lead Review Packet

Purpose: compress the current research, GDD, and first-slice spec into a small
set of decisions the lead can accept, reject, or reframe before fake shots or
native PC slice work.

This packet does not unlock implementation by itself. It clarifies what should
be accepted first.

## Inputs Under Review

- [Mobile mech analogs](../references/mobile_mech_analogs_2026-06-19.md)
- [Visual reference packet](../references/visual_reference_packet_2026-06-19.md)
- [GDD draft](gdd_draft_2026-06-19.md)
- [First slice spec](first_slice_spec_2026-06-19.md)

## Current Recommendation

Accept the current direction as:

> A landscape-first, PvE-first, one-mech, semi-auto 3D arena battler where the
> first battle gives salvage/resources, the player buys or crafts a first
> module in the hangar, then the next fight proves the change.

This is the cleanest compression of the reference study:

- Mech Arena contributes hangar/action readability.
- CATS contributes build -> proof -> rebuild.
- Mechangelion contributes simple controls and boss readability.
- War Robots contributes scale/weapon fantasy as a caution against clutter.

## Decision Matrix

| Decision | Recommendation | Why | Risk If Wrong | Review Action |
|---|---|---|---|---|
| First control model | Semi-auto arena | Preserves mech-pilot fantasy while staying casual | Too much movement friction on mobile/web | Accept for fake shot; test input later |
| First orientation | Landscape-first | Gives 3D mech, controls, and enemy readability enough horizontal space | Portrait market fit may be weaker for casual | Accepted on 2026-06-19 |
| First reward structure | Resources/salvage -> guided hangar purchase/craft | Adds agency and starts the economy without random loot pressure | If too abstract, reward can feel like only a number | Accepted on 2026-06-19 |
| First purchasable module | Shoulder rockets | Most visible silhouette change; proves build power against drones | Could imply high power too early | Accepted on 2026-06-19 |
| First defense/mobility | Short dash | Simple, readable, works with telegraphed danger | Can feel too action-heavy if casual audience resists movement | Accepted on 2026-06-19 |
| Tactical limiter | Heat mechanics with `Cooling` UI label | Strong mech fantasy, easy visual feedback through vents/glow, clearer casual label | `Cooling` can hide that overheating is a risk if feedback is weak | Accepted on 2026-06-19 |
| First mini-boss | Industrial machine / Foundry Warden | Avoids needing enemy mech customization; clear telegraphs | Less personal than rival mech | Accepted on 2026-06-19 |
| Camera | Fixed three-quarter/isometric | Phone-readable and asset-efficient | Less immersive than behind-shoulder | Accepted on 2026-06-19 |
| Tone | Industrial salvage sport | Supports parts, scrap, hangar, and bright action | Could become too gray if art direction drifts | Accepted on 2026-06-19 |

## Decisions To Accept Now

### 1. One Mech, Not A Squad

Recommendation: accept.

Why:

- Keeps ownership strong.
- Avoids Mech Arena squad complexity.
- Makes every part reward visible and meaningful.

Rejected for first slice:

- roster collection;
- five-mech lineup;
- pilot/implant layers.

### 2. PvE First

Recommendation: accept.

Why:

- Avoids multiplayer service balance.
- Lets enemy roles teach build choices.
- Reduces monetization/pay-to-win pressure.

Rejected for first slice:

- PvP matchmaking;
- clans;
- tournaments;
- ranked leagues.

### 3. Semi-Auto Arena

Recommendation: accept for fake shot and first prototype design.

Definition:

- Player controls movement or positioning.
- Targeting is automatic.
- Primary attack is automatic or one large button.
- Player actively triggers one special and one short dash action.

Fallback:

- If input tests fail, move to tactical auto-battler with player-triggered
  specials.

### 4. Resources First, Then A Guided Module Purchase

Reward structure: accepted.

First purchasable module: shoulder rockets, accepted on 2026-06-19.

Why:

- Resources make the first reward feel earned without removing player agency.
- The purchase/craft step teaches that battles feed the hangar economy.
- Easy to show on the model.
- Easy to show in combat.
- Strong against drone swarm.
- Creates immediate "my mech changed" proof.

Design implication:

- First battle teaches starter cannon.
- Reward grants salvage/resources.
- Hangar guides the player to buy/craft the first module.
- Second battle spawns drones so rockets create visible improvement.

### 5. Heat Mechanics With `Cooling` UI

Decision: accepted.

Why:

- Strong mechanical identity.
- Visualizable through weapon glow, vents, warning color.
- Creates a simple rhythm without ammo complexity.

UI rule:

- Use `Cooling` on the HUD and tooltip copy.
- Use amber vents, warning pulse, and short lockout feedback to communicate
  the underlying heat/overheat risk.

## Decisions Still Gated

These should not be silently decided during implementation:

- exact web/mobile export path;
- final orientation beyond first fake shot;
- exact input tuning and feel beyond the accepted floating joystick / WASD
  model;
- final art style and asset pipeline;
- exact UI layout;
- monetization/economy beyond guardrails.

## Fake Shot Brief

If these recommendations are accepted, the next design/art step is three fake
shots:

1. **Hangar**
   - landscape-first frame;
   - one chunky modular mech;
   - shoulder sockets visible but locked/empty;
   - `Battle` as dominant action;
   - no offers, no event clutter.
2. **Battle**
   - fixed three-quarter arena;
   - drone swarm enemy;
   - starter cannon or rocket burst visible;
   - heat feedback visible;
   - large touch zones/buttons.
3. **Reward/Upgrade**
   - salvage/resources reward;
   - guided first module purchase/craft;
   - slot destination highlighted;
   - before/after mech change;
   - second battle prompt.

Fake shot success question:

> Can a viewer understand that a part was earned, bolted onto the mech, and
> will change the next fight?

## If The Lead Rejects A Recommendation

Use this reframe map:

| Rejected Item | Most Likely Pivot |
|---|---|
| Landscape-first | Portrait hangar + tactical auto-battle; reduce movement controls |
| Semi-auto arena | Tactical auto-battler with timed specials |
| Shoulder rockets as first purchase | Shield module first; replace drone proof with charger proof |
| Heat limiter | Energy limiter; simpler blue meter shared by dash/special |
| Industrial mini-boss | Rival mech; requires enemy mech silhouette and copy-risk care |
| Industrial salvage sport tone | Colorful toy combat; more saturated palette, less grime |

## Review Checklist

Mark these before moving to fake shots or implementation:

- [x] Accept one-mech PvE first slice.
- [x] Accept landscape-first fake shots.
- [x] Accept semi-auto arena as first control model.
- [x] Accept resources/salvage as the first battle reward, followed by hangar
      purchase/craft.
- [x] Accept shoulder rockets as the first purchasable module.
- [x] Accept short dash as starter defense/mobility.
- [x] Accept heat mechanics with `Cooling` UI label as first limiter.
- [x] Accept industrial machine / Foundry Warden mini-boss.
- [x] Accept fixed three-quarter/isometric camera.
- [x] Accept industrial salvage sport tone.
- [x] Keep dated draft/spec as source documents; implementation starts from
      this handoff plus the first-slice spec.
- [x] Use `first_slice_spec_2026-06-19.md` as the source for the first native
      PC implementation task.

## Current Implementation Gate

Implementation remains blocked until:

1. The lead accepts or edits the above decisions.
2. A fake-shot target or equivalent visual proof exists for final art polish;
   rough native PC implementation may start from the accepted handoff.
3. The native PC slice is scoped with mobile-style controls/readability.
4. A scoped implementation task is created from the accepted spec.
