---
type: Project First Slice Spec
title: Mech Builder Battler First Slice Spec
description: Production-facing first vertical slice specification for the casual mobile/web 3D mech builder battler.
tags: [project, spec, mvp, vertical-slice, mobile, web, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# First Slice Spec

Status: design-ready for lead review, not implementation-ready.

This spec translates the first-pass [GDD draft](gdd_draft_2026-06-19.md) into a
small vertical slice target. It is intentionally scoped to one playable loop:
hangar -> battle -> reward -> upgrade -> second battle prompt.

Implementation remains gated on:

- native PC slice plan that preserves mobile/web UX constraints;
- accepted handoff decisions and visual direction; final art polish still
  needs fake shot or visual target evidence;
- current-build/prototype mismatch capture;
- stronger screenshot/gameplay evidence before copying exact reference UI,
  economy, combat pacing, or final art.

## Design Inputs

- [GDD draft](gdd_draft_2026-06-19.md)
- [Core loop and meta decomposition](core_loop_gameplay_meta.md)
- [Visual reference packet](../references/visual_reference_packet_2026-06-19.md)
- [Mech Arena deconstruction](../references/mech_arena_deconstruction_2026-06-19.md)
- [CATS deconstruction](../references/cats_deconstruction_2026-06-19.md)
- [Mechangelion deconstruction](../references/mechangelion_deconstruction_2026-06-19.md)

## Slice Goal

Prove that a casual player can understand and enjoy the core promise:

> My mech changes, I fight quickly, I earn resources, I buy or craft a module,
> and the next fight shows the difference.

The slice is successful only if the loop is visible without explanation in a
phone-scale screenshot sequence.

## Fixed Decisions For This Slice

- Mode: PvE only.
- Player object: one mech, not a squad.
- Main structure: hangar -> battle -> reward -> upgrade -> battle.
- Combat model: semi-auto arena.
- Orientation: landscape-first.
- Camera: fixed three-quarter/isometric.
- Input model: floating virtual joystick / drag movement zone for mobile
  target, WASD for native PC harness.
- Targeting: automatic target selection/highlight.
- Tactical limiter: heat mechanics with `Cooling` UI label.
- First reward structure: battle grants resources/salvage; the player buys or
  crafts the first module in the hangar.
- First upgrade type: visible part or visible attack-effect change.
- No monetization surfaces.
- No PvP, clans, pilots, implants, battle pass, random chest timers, or offer
  popups.
- Visual baseline: vivid 3D mech presentation is a first-slice requirement.
  Use real/model-like GLB assets or generated/kitbashed models with lighting,
  shadows, normals, bevels, painted metal, and material response. Shape/debug
  renderers are engineering scaffolding only and cannot be accepted as the
  visual proof.

## Deferred Decisions

These decisions must be resolved before runtime implementation:

- Export path: web/mobile delivery is deferred and must not replace the native
  PC harness without approval.
- Final visual polish target: accepted fake shot for hangar, battle, and
  reward/upgrade.

## Player Flow

### First Session Flow

1. **Hangar start**
   - Player sees one large mech.
   - `Battle` is the dominant action.
   - One upgrade slot is hinted but not required.
2. **Battle entry**
   - Player enters a small arena.
   - One enemy wave spawns.
   - UI shows movement/touch zone, primary attack state, one special, heat.
3. **Combat proof**
   - Player uses starter cannon and short dash.
   - Enemy reacts with sparks, stagger, or knockback.
   - Heat changes visibly.
4. **Reward**
   - Battle ends quickly.
   - Player receives salvage/resources.
   - Reward screen points back to the hangar purchase/craft action.
5. **Upgrade**
   - Hangar highlights an affordable first module or craft.
   - Player buys/crafts and equips or upgrades the module.
   - Mech model or attack effect changes.
6. **Second battle prompt**
   - Player is invited to test the change.
   - No shop/ad/popup interrupts before this prompt.

### Screenshot Proof Sequence

Required evidence later:

1. Hangar first screen.
2. Battle with enemy and controls visible.
3. Special/part effect active.
4. Reward screen.
5. Hangar after equip/upgrade.
6. Second battle prompt.

## Screen Contracts

### Hangar Screen

Purpose: ownership and next action.

Must show:

- one large 3D mech;
- clear `Battle` action;
- 3-5 part slots or hotspots;
- one upgrade/equip prompt;
- salvage counter only if it explains the upgrade;
- locked/progress hint for future part family.

Must not show:

- offer popup;
- event carousel;
- dense inventory grid;
- five-mech squad lineup;
- unrelated currencies;
- tiny stat spreadsheet.

Player actions:

- tap `Battle`;
- inspect one part slot;
- equip or upgrade highlighted reward after battle.

Acceptance:

- A new viewer can identify the mech, the primary action, and one future
  progression hook in five seconds.

### Battle Screen

Purpose: prove the current build.

Must show:

- player mech readable at phone scale;
- at least one enemy role;
- top health/wave/progress area;
- movement/touch control area;
- one primary attack state;
- one special action button;
- one defense/mobility action;
- heat state.

Player actions:

- move or reposition;
- trigger special;
- trigger defense/mobility;
- finish encounter.

Acceptance:

- The player can understand what to press without precision aim.
- The mech silhouette remains visible through effects.
- Enemy role is readable from shape and behavior.

### Reward Screen

Purpose: explain gain and destination.

Must show:

- salvage gained;
- affordable purchase/craft target or blueprint progress;
- slot destination for the chosen module;
- one next action: `Equip`, `Upgrade`, or `Continue`.

Must not show:

- chest timer as the first reward;
- ad multiplier as mandatory path;
- multiple unrelated reward tracks.

Acceptance:

- Player understands what changed and where to use it.

### Upgrade Screen / Hangar Return

Purpose: convert reward into visible progress.

Must show:

- highlighted slot;
- before/after or current/new part comparison;
- one behavior or visual change;
- `Test` or `Battle` prompt.

Acceptance:

- The upgraded mech or attack effect is visibly different before the player
  starts the next fight.

## Combat Rules

### Player Mech

Baseline values are conceptual until tuning starts:

- Health: enough to survive two tutorial mistakes.
- Movement: slow enough to feel heavy, responsive enough for mobile.
- Heat: one shared limiter for primary/special pressure.
- Starter primary: medium-range cannon.
- Starter mobility/defense: short dash.
- Starter special: locked until first purchase/craft or available as a weak tutorial
  version.

### Cooling / Heat

Rules:

- Primary attacks generate low heat.
- Special generates high heat.
- Heat cools over time.
- Overheat blocks special first, then slows primary fire if needed.
- UI should label the limiter `Cooling` and show color/vent glow plus a simple
  meter.

Why:

- Heat gives a mechanical feel without ammo management.
- It creates a casual rhythm: attack, burst, cool, reposition.

### Enemy Wave

First battle:

- 3-5 drones.
- 1 shield guard or charger after drones.
- Optional mini-boss only if the first battle stays under 120 seconds.

Second battle:

- Same structure, but new part should visibly improve one moment:
  - rockets clear drones faster;
  - shield blocks charger;
  - heavier legs reduce knockback;
  - cannon upgrade staggers shield guard.

### Fail And Recovery

First slice should allow failure only after the first loop is proven.

Recovery rules:

- Tutorial battle can be forgiving or auto-retry.
- First real fail should show why the build failed.
- Retry should return to hangar or replay battle without punishment.
- No paid revive in first slice.

## Content List

### Player Parts

Minimum:

1. Starter core/body.
2. Starter cannon.
3. Balanced legs.
4. Short dash module.
5. First purchasable module: shoulder rockets.

Accepted first purchasable module:

- Shoulder rockets.
- Accepted proof enemy: drone swarm.

### Enemies

Minimum:

1. Drone swarm.
2. Shield guard or charger.
3. Foundry Warden industrial mini-boss.

Preferred:

1. Drone swarm.
2. Shield guard.
3. Charger.
4. Foundry Warden.

### Arena

Minimum arena features:

- small readable combat space;
- industrial floor;
- 2-3 cover/obstacle silhouettes if using line danger;
- clear spawn side for enemies;
- enough camera distance to read player parts.

## Data Concepts

These are design concepts, not implementation schema.

### Mech

- equipped core/body;
- equipped weapon;
- equipped legs/drive;
- equipped special module;
- paint/decal;
- health;
- heat capacity;
- movement style.

### Part

- slot;
- archetype;
- visual model/effect id;
- base stat effect;
- milestone behavior;
- upgrade level;
- blueprint progress.

### Mission

- enemy set;
- arena;
- reward table;
- first-clear reward;
- replay reward;
- unlock requirement.

### Reward

- salvage amount;
- optional blueprint progress;
- suggested purchase/craft target;
- slot destination;
- suggested action.

## Tuning Targets

First slice target ranges:

- First battle length: 45-90 seconds.
- First full loop length: 2-4 minutes.
- Second loop invitation: visible within 5 minutes.
- First upgrade action: 1 tap after reward if using guided equip.
- Combat actions in first fight: no more than 3 active controls.
- Currencies visible in first loop: 1-2 max.

Reject tuning if:

- battle lasts longer than the player can understand the build change;
- upgrade requires reading multiple stat rows;
- the first loss requires grinding;
- the first purchase/craft cannot be visually tied to a slot.

## Fake Shot Requirements

Before final art or runtime polish, create or accept fake shots for:

1. Hangar first screen.
2. Battle moment.
3. Reward/upgrade return.

The fake shots must match the [visual reference packet](../references/visual_reference_packet_2026-06-19.md)
and answer:

- where am I?
- what do I do?
- what changed?
- why does my mech matter?
- what is the next action?

## Acceptance Criteria

The first slice is ready to implement only when:

- native PC implementation slice is scoped with mobile-style controls/readability;
- landscape-first orientation is used;
- rough runtime visuals follow the accepted visual direction; final polish uses
  fake shot or visual target evidence;
- this spec is accepted or edited by the lead;
- one scoped implementation task is created from this spec;
- screenshot proof plan names the exact screens to capture.

The first playable is acceptable only when:

- hangar -> battle -> reward -> upgrade -> second battle prompt works;
- the mech is visibly modular;
- one part changes combat or visible effect;
- first combat is readable without precise aim;
- no monetization/interruption appears before the first loop is complete;
- screenshot evidence exists for every proof screen.

## Review Questions

All major first-slice decisions are accepted. Remaining review should focus on
implementation proof quality, readability, and whether fake shots need another
iteration before final art.
