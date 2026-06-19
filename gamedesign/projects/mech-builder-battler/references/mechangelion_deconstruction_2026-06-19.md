---
type: Project Reference Deconstruction
title: Mechangelion Deconstruction
description: Central deconstruction draft for Mechangelion as an ultra-casual robot boss fight reference.
tags: [project, references, deconstruction, mobile, casual, robots, boss-fight, source-packet-incomplete]
timestamp: 2026-06-19T00:00:00Z
---

# Mechangelion Deconstruction

Scope: central reference draft for the casual mobile/web 3D mech builder
battler. This doc studies Mechangelion as an ultra-casual robot fighting and
boss spectacle reference, with a strong caveat that current evidence is weaker
than the Mech Arena and CATS packets.

## Reference Lock

- Mode: central deconstruction draft / source-packet incomplete.
- Reference question: how does a broad mobile robot game simplify combat,
  upgrades, and boss spectacle for a very casual audience?
- Durable doc path:
  `gamedesign/projects/mech-builder-battler/references/mechangelion_deconstruction_2026-06-19.md`
- Primary sources checked:
  - Google Play: https://play.google.com/store/apps/details?id=com.tsyatsya.mechangelion
  - Secondary web page: https://playmechangelion.pages.dev/
- Missing source quality: no current owned gameplay capture, no reliable
  full-session video timestamps, no complete screenshot board, and no verified
  first-launch sequence.
- Implementation readiness: weak. Use only as a simplicity and boss-framing
  reference until stronger visual/gameplay evidence is captured.

## Definition Of Ready Check

- [x] First battle hypothesis recorded.
- [x] Simple controls recorded from store/secondary text.
- [x] Upgrade cadence recorded at system level.
- [x] Boss framing recorded from store/secondary visual/text.
- [x] Borrow/avoid/copy-risk recorded.
- [x] Mobile/web translation recorded.
- [x] Current build mismatch audit captured.
- [ ] Current gameplay video timestamps captured.
- [ ] First-launch sequence captured.
- [ ] Local screenshot board saved.

## Evidence Board

| Evidence | Source | Status | What It Supports | Limitation |
|---|---|---|---|---|
| Store stats and feature pitch | Google Play | observed/secondary | Large audience, robot arena, simple one-on-one battle, upgrade hero, unlock weapons, upgrade defense, boss/dinosaur encounters, offline/single-player tags | Store copy does not prove feel or pacing |
| How-to-play page | Secondary web page | secondary | Control panel with jabs, punches, special moves; improve weapons/defense between fights; defeat bosses and robots | Unofficial/secondary page |
| Boss screenshot | Secondary image result/page | secondary visual | Huge enemy, large robot, simple combat icons, boss health/resource framing | Single static screenshot cannot define loop |
| Store reviews | Google Play | secondary | Risk around ads, difficulty spikes, and desire for more multiplayer/social play | Anecdotal |

## Screen And Loop Grammar

### First Screen / First Battle

Exact first screen is unknown. Based on store/source framing, likely grammar is:

- Large player robot and opponent are the main visual objects.
- The first meaningful action is entering or continuing a one-on-one fight.
- The game emphasizes immediate battle rather than complex hangar management.
- A boss or oversized enemy creates spectacle quickly.

Translation for our game:

- First minute can open with a hangar, but should reach an impressive enemy
  quickly.
- The first opponent should be visually large enough to sell the mech fantasy.
- Tutorial text should be minimal; the model/action should explain the goal.

### Simple Controls

Recorded source claims:

- Battle uses a control panel with moves such as jabs, punches, and special
  actions.
- The player uses simple, direct combat buttons instead of complex movement.
- Boss fights ask the player to pick attack/defense timing at a broad level.

Translation for our game:

- Replace fighting-game punch language with mech actions: cannon shot, shield,
  dash, missile burst, blade strike.
- Keep only two or three buttons in the first fight.
- Avoid precision aim and tiny cooldown icons.

### Boss Framing

Observed/secondary grammar:

- Oversized enemies such as bosses or dinosaurs create clear stakes.
- Boss health is readable and central.
- Player robot is framed as a giant hero rather than a fragile vehicle.
- Upgrade pressure is tied to defeating tougher bosses.

Translation for our game:

- Use a mini-boss early, but keep its attacks telegraphed and readable.
- Make defense upgrades visible: shield flare, armor plates, leg braces,
  reactor glow.
- Let the player see why an upgrade helped against a boss phase.

### Upgrade Cadence

Recorded source claims:

- Between battles, the player upgrades weapons and defenses.
- Weapon unlocks and defense upgrades are part of level progression.
- The game advises defense upgrades first against difficult bosses, then weapon
  improvement.

Translation for our game:

- First purchasable module should be either an attack part or a
  defense/mobility part.
- A boss can teach "upgrade defense" without forcing a fail wall.
- The player should not need to understand many stats before seeing the model
  change.

## Systems Extraction

### What Mechangelion Is Doing Well

- It reduces robot combat to immediately understandable button actions.
- It uses large enemies and bosses to create spectacle without complex modes.
- It appears to support offline/single-player play, which fits casual access.
- It keeps progression language simple: upgrade hero, weapons, and defense.

### Why It Is Weak As A Core Model

- It may be too shallow for a mech-builder promise.
- It risks turning the mech into a generic punching character.
- It does not prove deep part assembly from available evidence.
- Difficulty spikes and ads are flagged by player complaints.
- The current evidence packet is not strong enough for implementation details.

## Borrow / Avoid / Copy-Risk

### Borrow

- Immediate one-on-one robot spectacle.
- Large boss readability.
- Few large combat buttons.
- Upgrade weapon/defense clarity.
- Offline/single-player accessibility.
- Low-poly/stylized broad-audience tone.

### Avoid

- Generic punching if parts do not matter.
- Boss HP sponge fights.
- Difficulty spikes that imply upgrade gating.
- Ad interruptions as part of the loop.
- Copying a simple fighter without the buildcraft layer.

### Copy-Risk

- Exact boss types, dinosaur framing, robot silhouette, UI button arrangement,
  arena framing, upgrade wording, and title/name language.

## Mobile/Web Translation

Recommended adaptation:

1. Use Mechangelion as a simplicity ceiling: if our first battle needs more
   buttons than this type of game, it is probably too complex.
2. Convert punches into part-driven mech verbs.
3. Use a first mini-boss to make upgrades emotionally obvious.
4. Keep the first boss short and telegraphed.
5. Let buildcraft depth live in the hangar after the first loop, not before.
6. Do not rely on this reference for economy or exact UI until stronger source
   evidence exists.

## Design Decision Draft

Mechangelion should influence first-minute accessibility and boss scale, not
the whole game structure. The project should borrow "huge robot vs huge enemy
with simple buttons" and merge it with Mech Arena's hangar/action clarity and
CATS' build-proof loop.

## Current Build Mismatch

Current native build captures exist and are indexed in
[Current Build Mismatch Audit](current_build_mismatch_audit_2026-06-19.md).
Primary battle capture:
`build/captures/mech_t0021_rockets_smoke.png`.

Current matches:

- Combat uses a small number of readable actions: movement, dash, rocket
  special, and Cooling.
- The rocket action comes from an equipped part and visibly changes the mech.
- The first upgrade improves attack output in a simple, understandable way.
- The first loop completes without forced ad/offer interruptions.

Current mismatches:

- Enemy scale is not yet spectacular; drones do not satisfy the boss-scale
  fantasy.
- Defense/armor progression is not proven; dash exists, but there is no large
  telegraphed attack to dodge or block.
- Runtime touch hit targets and floating joystick are not proven.
- VFX persistence and authored robot model quality are still early.

Next proof: native Foundry Warden or shield-guard encounter screenshot with a
large telegraphed attack, clear dash/defense response, and readable mech
silhouette through effects.

## Source Gaps

- Need current install or full gameplay capture.
- Need exact first-launch sequence.
- Need battle HUD screenshot, upgrade screen screenshot, and boss result
  screenshot.
- Need ad/interruption audit.
- Need stronger evidence before this ref can drive concrete implementation.
