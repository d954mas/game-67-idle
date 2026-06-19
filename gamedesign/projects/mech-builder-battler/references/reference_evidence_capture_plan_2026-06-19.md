---
type: Project Reference Evidence Plan
title: Mech Builder Battler Reference Evidence Capture Plan
description: Capture plan for turning current quick-check and source-incomplete reference docs into implementation-ready evidence.
tags: [project, references, evidence, capture-plan, mobile, web, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# Reference Evidence Capture Plan

Status: capture plan, not completed evidence.

Purpose: define exactly what evidence is still needed before exact reference
claims can drive UI, combat, economy, fake shots, or the first native PC slice.
Current design docs are sufficient for lead review and high-level direction,
but the central references remain source-packet incomplete.

Development/iteration target: native PC harness. Mobile/web informs controls,
UI scale, readability, session length, and later export constraints.

## Inputs

- [Mobile mech analogs](mobile_mech_analogs_2026-06-19.md)
- [Mech Arena deconstruction](mech_arena_deconstruction_2026-06-19.md)
- [CATS deconstruction](cats_deconstruction_2026-06-19.md)
- [Mechangelion deconstruction](mechangelion_deconstruction_2026-06-19.md)
- [Visual reference packet](visual_reference_packet_2026-06-19.md)
- [Mechanics and meta matrix](../design/mechanics_meta_matrix_2026-06-19.md)

## Reference Lock

Mode: central gameplay and visual reference evidence capture.

Reference question:

> Which player-facing screens, first actions, rewards, upgrades, controls,
> friction points, and visual compositions should inform a casual native-PC
> slice for a mobile/web-targeted 3D mech builder battler?

No-coding/no-final-art boundary:

- Do not implement reference-driven UI/combat/economy/balance from the current
  reference docs.
- Do not generate final art from reference claims.
- Fake shots can proceed only as reviewed concept targets, not as proof of exact
  reference behavior.

Unlock condition:

- Each reference used for exact implementation has an evidence board with
  enough screenshots/frames/timestamps to answer first screen, first input,
  visible response, reward feedback, upgrade/progression UI, and friction.

Expected native proof after capture:

- Phone-scale screenshot sequence from the native PC slice:
  hangar -> battle -> special effect -> reward -> upgrade -> retest prompt.

## Source Candidates

Use these as starting points, not as proof completion.

| Reference | Primary source candidates | Supporting source candidates | Current status |
|---|---|---|---|
| Mech Arena | Google Play page; official support pages; official trailers/screenshots; current gameplay videos | player reviews only for friction; wiki/community only for terminology | Strong market/source check, weak first-session evidence |
| CATS | Google Play page; official screenshots/trailer; current gameplay videos or long screenshot sequence | Gamezebo/guide/wiki only for pacing and friction | Strong build-battle concept, weak current first-session capture |
| Mechangelion | Google Play page if available; MEmu/how-to-play screenshot page; gameplay video | secondary store mirrors only as weak evidence | Weakest source, should remain simplicity/boss reference only |
| War Robots | Google Play page; official website/patch notes; gameplay video, especially PvE/solo modes | reviews for pay-to-win/clutter risk, wiki for mode terminology | Useful anti-pattern/scale reference, not first-slice model |

Fresh store pages checked on 2026-06-19:

- Mech Arena Google Play:
  `https://play.google.com/store/apps/details?id=com.plarium.mechlegion&hl=en_US`
- CATS Google Play:
  `https://play.google.com/store/apps/details?id=com.zeptolab.cats.google&hl=en_US`
- War Robots Google Play:
  `https://play.google.com/store/apps/details?id=com.pixonic.wwr&hl=en_US`

## Evidence Board Requirements

For each reference promoted beyond quick-check:

| Board item | Required capture | Why it matters |
|---|---|---|
| First screen | Screenshot/frame before the player acts | Proves main object, visual hierarchy, primary action |
| First input | Screenshot/frame showing what to tap/drag/click | Proves control model and onboarding clarity |
| Visible response | Frame after the first input | Proves feedback and cause/effect clarity |
| Combat proof | 2-3 frames across one fight | Proves pacing, camera, enemy readability, effects |
| Reward feedback | Screen/frame where reward appears | Proves reward location and explanation |
| Upgrade/progression UI | Screen/frame showing equip/upgrade/collection | Proves how build changes are surfaced |
| Friction/blocked state | Popup, timer, price gate, offer, loss, or full inventory | Proves what to avoid or simplify |
| Repeated loop | 1-5 minute sequence or several frames | Proves what repeats, changes, or blocks |

Minimum central ref ready state:

- 6+ cited frames/screenshots or a long video segment with timestamps;
- one observation ledger with 5 visible beats;
- one current native mismatch entry;
- borrow/avoid/copy-risk;
- next native proof screenshot/scenario.

## Capture Passes

### Pass 1: Mech Arena

Priority: highest for hangar/action/mobile combat grammar.

Capture:

- home/hangar before battle;
- `Battle` or matchmaking entry action;
- first combat HUD with movement/aim/ability controls;
- one special ability moment;
- end-of-battle reward/result screen;
- upgrade/weapon/mech screen;
- one popup/offer/event surface or review-supported friction point.

Questions to answer:

- Does the hangar sell one machine or a roster first?
- How much HUD density is acceptable at phone scale?
- What is the minimum visible feedback for special abilities?
- Which layers are clearly too live-service-heavy for our first slice?

Translation target:

- Borrow hangar ownership, short combat proof, control readability, ability
  spectacle.
- Avoid PvP-first structure, pilot/implant depth, popups, tournament/event
  pressure.

### Pass 2: CATS

Priority: highest for build -> proof -> rebuild loop.

Capture:

- build/garage screen with chassis and part slots;
- first fight entry;
- automated battle moment;
- reward/crate/part outcome;
- upgrade/fusion screen;
- blocked/timer/ad/box friction;
- repeated loop after a build change.

Questions to answer:

- How fast does the game show "my build caused that result"?
- Where does it explain part slots and physical attachment?
- Which parts of no-control battle are satisfying, and which hurt agency?
- Which crate/timer systems must be excluded from our first loop?

Translation target:

- Borrow physical slot readability and instant battle proof.
- Avoid no-agency combat as the primary fantasy and timer boxes as first reward.

### Pass 3: Mechangelion

Priority: narrow simplicity and boss-readability reference.

Capture:

- first fight or fight-start frame;
- control panel/buttons;
- boss/enemy scale frame;
- weapon/defense upgrade screen;
- reward or progression screen;
- ad/friction if visible.

Questions to answer:

- Which simple button layout reads fastest?
- How large should the enemy/boss be relative to the player robot?
- Does the reference show actual buildcraft, or only stat upgrades?

Translation target:

- Borrow simplicity ceiling and big-enemy readability.
- Avoid shallow punch-only progression and weak buildcraft.

### Pass 4: War Robots

Priority: anti-pattern and scale reference, not first-slice blueprint.

Capture:

- robot/weapon/module customization screen;
- combat HUD with movement/fire/ability surfaces;
- PvE/solo or Extermination mode if available;
- reward/progression screen;
- event/offer/random-item friction;
- one player-review-supported pain point around targeting, ads, or power creep.

Questions to answer:

- Which robot/weapon fantasy is market-proven?
- Which complexity layers should be delayed or banned from first slice?
- What does heavy live-service robot combat look like when overloaded?

Translation target:

- Borrow robot scale, weapon fantasy, and role variety.
- Avoid roster depth, clan/event pressure, power creep, and precision shooter
  friction.

## Observation Ledger Template

Use this table in the updated deconstruction docs after capture.

| Beat | Source timestamp/frame | Visible screen state | Player action | Visible response | Reward/UI feedback | Evidence label | Inferred meaning |
|---|---|---|---|---|---|---|---|
| 1 |  |  |  |  |  | observed |  |
| 2 |  |  |  |  |  | observed |  |
| 3 |  |  |  |  |  | observed |  |
| 4 |  |  |  |  |  | observed |  |
| 5 |  |  |  |  |  | observed |  |

## Native PC Mismatch Template

Use this after the first native PC slice exists.

| Ref claim | Evidence item | Native PC capture | Mismatch | Required change |
|---|---|---|---|---|
| Hangar primary action is readable |  |  |  |  |
| Battle controls read at phone scale |  |  |  |  |
| Reward points to the part slot |  |  |  |  |
| Upgrade changes model/effect |  |  |  |  |
| Friction is removed from first loop |  |  |  |  |

## Acceptance Criteria For Evidence Capture

Evidence capture is done only when:

- Mech Arena and CATS each have a filled evidence board and 5-beat observation
  ledger.
- Mechangelion is either upgraded with enough evidence or explicitly demoted to
  weak simplicity reference only.
- War Robots is captured as scale/anti-pattern reference, not as first-slice
  structure.
- Each updated deconstruction states borrow, avoid, copy-risk, and exact next
  native proof.
- The lead can point to the captured frames/screenshots and challenge any GDD
  claim without replaying the research conversation.

## Current Verdict

Reference study is not ready for exact implementation.

Ready now:

- lead review of concept direction;
- fake-shot planning at concept level;
- high-level mechanics/meta discussion;
- native PC slice planning with explicit evidence gaps.

Not ready now:

- exact UI layout;
- exact economy pacing;
- exact battle duration and HUD density;
- exact upgrade cadence;
- final art style copying from refs;
- claiming "grounded in refs" for implementation details without captured
  frames/timestamps.
