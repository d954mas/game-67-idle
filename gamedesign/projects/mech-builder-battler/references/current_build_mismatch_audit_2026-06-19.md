---
type: Project Reference Mismatch Audit
title: Current Build Mismatch Audit
description: Screenshot-backed comparison of the first native Mech Builder Battler build against the central references.
tags: [project, references, mismatch-audit, screenshots, t0022, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# Current Build Mismatch Audit

Scope: T0022 reference refresh after the first native PC screenshot sequence.

This is not a new external-source deconstruction. The Mech Arena, CATS, and
Mechangelion packets are still source-packet-incomplete for exact live pacing.
This audit only compares the current native build against the reference
translation rules already recorded in those docs.

## Current Build Evidence Board

Checked date: 2026-06-19.

| Build state | Local capture | What it proves | What it cannot prove |
|---|---|---|---|
| Hangar first screen | `build/captures/mech_t0021_hangar_smoke.png` | one large owned mech, clear `Battle`, shoulder module lock, salvage counter, hangar/arena props | final art quality, touch layout, actual mobile viewport |
| Battle entry | `build/captures/mech_t0021_battle_smoke.png` | fixed three-quarter camera, player mech, drones, WASD hint, Cooling meter, dash/rocket action panels | touch control implementation, longer pacing, enemy variety |
| Reward | `build/captures/mech_t0021_reward_smoke.png` | battle grants salvage and routes toward upgrade | reward polish, longer economy, repeat rewards |
| Upgrade | `build/captures/mech_t0021_upgrade_smoke.png` | salvage is spent on shoulder rockets and the slot destination is explicit | inventory depth, multiple part choices, comparison UI |
| Retest prompt | `build/captures/mech_t0021_retest_smoke.png` | equip action returns to a test/rebattle prompt | broader hangar loop, multiple locked slots |
| Rocket proof | `build/captures/mech_t0021_rockets_smoke.png` | mesh-backed rocket modules, large combat effect, drones, HUD actions, PASS product gate screenshot | final high-fidelity mech asset, boss spectacle, portrait/mobile export |

Runtime product gate:
`gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T16-05-21-117Z_desktop-battle.md`.

Runtime visual review:
`gamedesign/projects/mech-builder-battler/evidence/t0021_runtime_visual_review_2026-06-19.md`.

## Mech Arena Translation Check

Borrow target: short mobile mech battle, hangar as home, large mech silhouette,
special ability identity, phone-friendly controls, combat -> upgrade
progression.

Current matches:

- Hangar makes the owned mech the largest object and gives `Battle` as the
  dominant next action.
- Battle uses a fixed readable arena camera, automatic target emphasis, dash,
  special rockets, enemy drones, and compact combat HUD.
- Progression routes directly from battle reward into a shoulder module upgrade.
- No PvP, pilots, implants, stores, event stack, offers, or monetization appear
  before the first loop finishes.

Mismatches:

- First combat input is PC `WASD`; mobile floating joystick is accepted as UX
  target but not implemented in runtime.
- Battle lacks Mech Arena-style incoming-damage clarity: no directional damage
  warning, lock-on warning, or enemy attack telegraph yet.
- The mech has a readable modular silhouette but is still cube-kitbashed, not a
  model-quality hero mech with authored armor bevels/materials.
- No hangar squad/power abstraction is intentional, but the first screen still
  needs a clearer one-mech power/readiness affordance for the casual audience.

Next proof target:

- Native screenshot: hangar plus battle after an authored/high-fidelity starter
  mech pass, with one readable enemy telegraph and unchanged no-offer first
  loop.

## CATS Translation Check

Borrow target: build -> instant proof -> reward -> rebuild, central owned
object, physical part slots, short battle readability, deterministic progress,
and test-as-core verb.

Current matches:

- The hangar centers the owned mech and shows a shoulder module destination.
- The first reward directly buys/equips a shoulder rocket module instead of a
  timer box or random chest.
- The second battle proves the upgrade with visible rocket modules and rocket
  effects against drones.
- The battle remains semi-active: auto basics plus player dash/rocket actions,
  avoiding the fully passive CATS downside.

Mismatches:

- Part slots are represented by a shoulder lock/prompt, not a full visible
  physical slot grammar for core/body, weapon arm, legs, and module.
- The upgrade screen lacks a before/after part comparison and does not show why
  this build is a style choice beyond "rockets equipped".
- Battle proof is readable, but not yet a strong "my build solved a specific
  enemy role" moment; drones are mostly target fodder.
- There is no failure/counter explanation yet, so buildcraft learning remains
  shallow.

Next proof target:

- Native screenshot: upgrade/hangar screen with at least three physical part
  hotspots and a before/after shoulder rocket comparison, followed by battle
  capture where rockets solve a drone swarm faster than baseline.

## Mechangelion Translation Check

Borrow target: simple giant-robot combat, few large buttons, early spectacle,
weapon/defense clarity, and broad-audience readability.

Current matches:

- Battle uses a small number of visible actions: movement, dash, rocket special,
  and Cooling.
- Rocket explosions and drone swarm give an immediate spectacle moment without
  asking for precision aim.
- Upgrade language is simple and single-purpose: salvage -> shoulder rockets ->
  retest.
- The loop remains offline/single-player and avoids ad interruption.

Mismatches:

- The first enemy set is a drone swarm, not a large boss-scale opponent; the
  accepted Foundry Warden mini-boss is still absent.
- Defense/armor progression is not proven; dash exists, but no boss telegraph
  teaches defensive timing.
- Combat buttons are readable on desktop, but touch hit targets and floating
  joystick affordances are not runtime-proven.
- Robot spectacle is constrained by cube-kitbashed geometry and limited VFX
  persistence.

Next proof target:

- Native screenshot: first Foundry Warden or shield-guard encounter with a large
  telegraphed attack, a clear dash/defense response, and the mech silhouette
  staying readable through effects.

## Reference Digest

- Mode: central deconstruction refresh against current native screenshots.
- Sources checked: existing Mech Arena, CATS, and Mechangelion deconstruction
  docs plus the six local `build/captures/mech_t0021_*_smoke.png` captures,
  checked 2026-06-19.
- Observed facts: current hangar has one large mech and a `Battle` action;
  current first loop grants salvage and equips shoulder rockets; current battle
  shows mesh-backed rocket modules and drone combat; no offer/ad/store
  interruption appears before retest; product gate passes with minor art debt.
- Current-build mismatch: the runtime proves the loop and mesh/material path,
  but not final mobile controls, authored mech asset quality, physical slot
  grammar, enemy-role counterplay, or boss-scale spectacle.
- Borrow: Mech Arena's hangar/action clarity, CATS' build-proof loop, and
  Mechangelion's simple spectacle.
- Avoid: PvP service complexity, passive-only combat, timer boxes, ads/offers,
  and exact protected UI/asset shapes.
- Copy-risk: exact mech silhouettes, CATS vehicle/box language, Mechangelion
  boss/dinosaur framing, reference HUD layouts, and monetized flows.
- Next native proof: an authored/high-fidelity starter mech pass using the
  proven mesh/material pack path, plus one enemy-role proof screenshot that
  shows the upgraded part solving a specific combat problem.
