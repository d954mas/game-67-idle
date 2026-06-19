---
type: Project Reference Deconstruction
title: Mech Arena Deconstruction
description: Central deconstruction draft for Mech Arena as a casual mobile mech action and hangar reference.
tags: [project, references, deconstruction, mobile, mechs, action, source-packet-incomplete]
timestamp: 2026-06-19T00:00:00Z
---

# Mech Arena Deconstruction

Scope: central reference draft for the casual mobile/web 3D mech builder
battler. This doc studies Mech Arena as the strongest "short-session mobile
mech action" analog, with special attention to hangar, battle controls,
progression, and copy-risk.

## Reference Lock

- Mode: central deconstruction draft / source-packet incomplete.
- Reference question: how does a successful mobile mech action game package
  short battles, readable controls, mech identity, and upgrade progression?
- Durable doc path:
  `gamedesign/projects/mech-builder-battler/references/mech_arena_deconstruction_2026-06-19.md`
- Primary sources checked:
  - Google Play: https://play.google.com/store/apps/details?id=com.plarium.mechlegion
  - Official support, Squad Power: https://mecharena-support.plarium.com/hc/en-us/articles/12739688618780-How-can-I-increase-Squad-Power
  - Official support, Controls and Battle Settings: https://mecharena-support.plarium.com/hc/en-us/articles/12575581245340-Controls-and-Battle-Settings
  - APKPure secondary screenshot/source page: https://apkpure.net/mech-arena-shooting-game/com.plarium.mechlegion
- Missing source quality: no current owned gameplay capture, no first-session
  video timestamps, no local screenshot board.
- Implementation readiness: not ready for UI or combat copying. Ready only as a
  directional product and systems reference.

## Definition Of Ready Check

- [x] First screen hypothesis recorded.
- [x] First input path recorded from support/store sources.
- [x] Visible battle response recorded from HUD/control evidence.
- [x] Reward/progression UI recorded at system level.
- [x] Borrow/avoid/copy-risk recorded.
- [x] Mobile/web translation recorded.
- [ ] Current gameplay video timestamps captured.
- [ ] Current build mismatch audit captured.
- [ ] Local screenshot board saved.

## Evidence Board

| Evidence | Source | Status | What It Supports | Limitation |
|---|---|---|---|---|
| Store stats and feature pitch | Google Play | observed/secondary | 50M+ downloads, short battles, TPS robot combat, 25+ mechs, 90+ weapons, skins, modes, maps, pilots, cross-platform sync | Store copy does not prove first-session pacing |
| Controls and battle settings | Official support | observed | Battle screen uses virtual joystick, sprint cooldown, damage/missile warnings, customizable button size/position/opacity, targeting assist and camera sensitivity options | Support docs are not a full session trace |
| Squad Power system | Official support | observed | Hangar power combines mechs, weapons, pilots, and implants; upgrades raise squad power | Does not prove economy pacing or fairness |
| Hangar/lobby screenshot | APKPure secondary page | secondary visual | Central mech/squad display, Battle button, shop/gear surfaces, locked slots | Third-party page may lag current live build |
| Store reviews | Google Play | secondary | Player risk around popups, spend pressure, power creep, and progression friction | Reviews are anecdotal |

## Screen And Loop Grammar

### First Screen / Home

Likely first-screen grammar, based on store/support/secondary screenshots:

- Current mech or squad is the hero object.
- Bottom or major primary action leads to `Battle`.
- Secondary surfaces expose gear, shop, squad/hangar power, currencies, rating,
  and unlock slots.
- Locked slots communicate future expansion before the player owns the full
  roster.

Design implication: our first screen should use the hangar as the emotional
center, but reduce it to one mech, one obvious `Battle` action, one upgrade
prompt, and no store/offer stack before the first loop.

### First Input

Observed path from support/source claims:

1. Tap `Battle`.
2. Choose a battle mode or enter a match flow.
3. In battle, move with a virtual joystick.
4. Use sprint, weapon buttons, and mech ability actions.

Translation for our game:

- First input should be `Battle`, not inventory management.
- First combat input should be movement or one oversized attack/special button.
- Any custom control options should wait until after the first playable proof.

### Battle HUD

Observed battle grammar:

- Top HUD: timer, score/control-point state, map/minimap or round state.
- Center: crosshair, target information, enemy mechs, readable arena lanes.
- Left: virtual joystick and movement/sprint.
- Right: weapon, ability, reload/targeting actions.
- Bottom: health and mech state.
- Warning layer: damage direction and missile warnings.

Translation for our game:

- Keep battle HUD to one movement surface, one primary attack affordance, one
  defensive/mobility action, and one special weapon.
- Use warnings sparingly: damage flash, lock-on icon, heat danger.
- Make every combat effect readable around the mech silhouette at phone scale.

### Visible Response

From support and screenshot evidence, the expected response stack is:

- Movement changes position immediately.
- Sprint has an explicit cooldown.
- Incoming damage points back to the threat.
- Missile warning creates a short tactical reaction window.
- Weapon/ability buttons create visible attack output and cooldown state.

Translation for our game:

- First slice needs one response that feels heavy and mechanical: recoil,
  stagger, sparks, shield flare, knockback, or part pop.
- The first special should be visually large but mechanically simple.

### Reward And Progression UI

Observed system grammar:

- Rewards are reported after battle summary; support says leaving a battle
  still shows a summary and rewards depend on time and contribution.
- Squad Power rises through upgrading mechs and weapons, installing/upgrading
  pilots, and using implants.
- Store copy and reviews indicate extensive roster/weapon/skin/pilot layers.

Translation for our game:

- Do not start with squad power, pilots, implants, and broad roster management.
- Collapse the first version into one mech and 3-5 part slots.
- Reward summary must answer: what part/currency was earned, where it goes,
  what changed, and what battle opens next.

## Systems Extraction

### What Mech Arena Is Doing Well

- It makes "mech combat" immediately legible: robot, weapons, arena, battle.
- It keeps sessions short enough for mobile by using few-minute matches.
- It supports different play styles through mech abilities and weapon loadouts.
- It treats the hangar as a long-term identity and progression space.
- It acknowledges mobile/web friction through custom controls and cross-device
  sync.

### Why It Is Too Heavy For This Project

- Multiplayer service balance is a large first-version burden.
- Squad breadth can drown the fantasy of owning one expressive mech.
- Pilots, implants, tournaments, events, skins, currencies, and offers are too
  many concepts for a first casual loop.
- Player review complaints flag the genre's biggest retention risks: popups,
  spend pressure, power creep, and obsolete gear.

## Borrow / Avoid / Copy-Risk

### Borrow

- Short mech battles.
- Hangar as home.
- Large readable mech silhouettes.
- Special ability as build identity.
- Phone-friendly virtual controls.
- Direct progression from combat into upgrade.

### Avoid

- PvP as the first core pillar.
- Wide squad collection before the starter mech feels personal.
- Popups before the first satisfying loop.
- Pilot/implant/stat layers in the first minute.
- Gear power creep that invalidates earlier parts.

### Copy-Risk

- Exact mech silhouettes, names, factions, abilities, maps, modes, HUD layout,
  store/offer flow, pilot/implant structure, and event naming.

## Mobile/Web Translation

Recommended adaptation:

1. Use a landscape or responsive combat view with a large mech and small arena.
2. Use semi-auto targeting to avoid precise shooter aim.
3. Let the player press one special and one defense/mobility action.
4. Keep the hangar focused on one mech and visible part swaps.
5. Make rewards physical: a cannon, shoulder pod, leg kit, shield, blade,
   reactor glow, or paint.
6. Push all service-style complexity out of the first playable.

## Design Decision Draft

Mech Arena should influence the battle/hangar feel, not the business model or
service architecture. The first version should be a PvE single-mech compression
of the fantasy: short battle, obvious controls, one build-defining special,
visible reward, immediate hangar upgrade.

## Current Build Mismatch

No current build exists for `mech-builder-battler`. First mismatch audit should
compare a future capture against this checklist:

- Does the first screen show the mech as the largest object?
- Is `Battle` the obvious next action?
- Can the player understand the first combat input without precise aim?
- Does the special action visibly express the equipped part?
- Does the reward attach back to the mech?
- Is there zero shop/offer interruption before the first loop completes?

## Source Gaps

- Need current gameplay capture from install, emulator, or reliable video.
- Need first-session timing: first launch -> first battle -> first reward.
- Need screenshot board for hangar, mode select, battle HUD, reward, upgrade.
- Need exact interruption audit: offers, tutorials, popups, permissions.
