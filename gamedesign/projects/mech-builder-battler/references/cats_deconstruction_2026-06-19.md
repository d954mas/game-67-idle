---
type: Project Reference Deconstruction
title: CATS Deconstruction
description: Central deconstruction draft for CATS as a casual buildcraft and automated battle proof reference.
tags: [project, references, deconstruction, mobile, casual, buildcraft, source-packet-incomplete]
timestamp: 2026-06-19T00:00:00Z
---

# CATS Deconstruction

Scope: central reference draft for the casual mobile/web 3D mech builder
battler. This doc studies CATS: Crash Arena Turbo Stars as the main reference
for casual buildcraft, short automated battle proof, part slots, reward boxes,
and the danger of low-agency combat.

## Reference Lock

- Mode: central deconstruction draft / source-packet incomplete.
- Reference question: how can a casual game make assembly/build decisions the
  main fun while keeping battles short and readable?
- Durable doc path:
  `gamedesign/projects/mech-builder-battler/references/cats_deconstruction_2026-06-19.md`
- Primary sources checked:
  - Google Play: https://play.google.com/store/apps/details?id=com.zeptolab.cats.google
  - Gamezebo guide: https://www.gamezebo.com/walkthroughs/cats-crash-arena-turbo-stars-tips-cheats-and-strategies/
  - KickMyGeek screenshot gallery: https://kickmygeek.com/image-jeu/android/cats-crash-arena-turbo-stars
- Missing source quality: no current owned gameplay capture, no first-session
  video timestamps, no local screenshot board, and some visual evidence is from
  secondary/older pages.
- Implementation readiness: useful for loop and buildcraft, not ready for exact
  UI or economy replication.

## Definition Of Ready Check

- [x] Build screen grammar recorded.
- [x] Part slot and build implication recorded.
- [x] Battle proof grammar recorded.
- [x] Reward timing recorded.
- [x] Difficulty/ad pressure risk recorded.
- [x] Borrow/avoid/copy-risk recorded.
- [x] Mobile/web translation recorded.
- [ ] Current gameplay video timestamps captured.
- [ ] Current build mismatch audit captured.
- [ ] Local screenshot board saved.

## Evidence Board

| Evidence | Source | Status | What It Supports | Limitation |
|---|---|---|---|---|
| Store stats and pitch | Google Play | observed/secondary | 100M+ downloads, casual tag, craft combat machine, attachments, weapons, 1v1 arenas, rankings, random-item IAP | Store copy does not explain first-session flow |
| Garage/loop description | Gamezebo | secondary | Garage home, Quick Fight, automated 1v1, three wins for supply box, four-box limit, timers/gems, championship progression | Guide may not reflect latest live tuning |
| Strategy notes | Gamezebo | secondary | Build testing, counters, wheels/chassis/weapons interaction, current-vehicle state used in fights | Not a formal system spec |
| Garage/battle screenshots | Gamezebo/KickMyGeek | secondary visual | Central vehicle, edit prompt, Quick Fight surface, health/damage battle framing | Secondary pages can lag current version |
| Store reviews | Google Play | secondary | Player risk around whales, pay pressure, difficulty scaling, forced ads | Anecdotal and version-dependent |

## Screen And Loop Grammar

### Build Screen / Garage

Observed grammar:

- The custom machine is the center of the screen.
- A direct edit affordance sits close to the vehicle.
- `Quick Fight` is a prominent action from the garage.
- Boxes, timers, currencies, ranking, and events surround the main build.
- The player can enter fights with the current vehicle state.

Design implication: the garage works because it makes the object of ownership
obvious. Our hangar should do the same with one mech, but with less timer,
box, and currency pressure in the first minute.

### Part Slots And Build Logic

Observed/system-level CATS grammar:

- Chassis shape and attachment points constrain the build.
- Weapons and gadgets change how the vehicle wins or loses.
- Wheels/tires and body shape affect contact, reach, and stability.
- The same part can be strong or weak depending on placement and opponent.
- Testing and counter-building are part of the fun.

Translation for our game:

- Mech parts should change both model silhouette and combat function.
- Slots should be readable and few: core/body, weapon arm, legs/drive, special
  module, paint/decal.
- Parts should create style, not spreadsheet-only optimization.
- A quick test battle is needed after every meaningful equip.

### First Input / Battle Entry

Observed path from secondary guide:

1. Player presses `Quick Fight` from the garage.
2. Game finds a random opponent at similar rank.
3. Battle resolves automatically.
4. A short result determines rank/reward progress.

Translation for our game:

- `Battle` from hangar remains the cleanest first input.
- We should not use fully automated combat as the main fantasy unless we pivot
  to build-simulation.
- Better compromise: battle proves the build quickly, while the player still
  triggers one special or defense action.

### Battle Proof

Observed battle grammar:

- Two machines collide in a compact arena.
- Combat is short and readable.
- Health/damage state is visible at the top.
- The player mostly watches whether the build succeeds.
- The build result is the proof, not a long manual fight.

Translation for our game:

- A mech battle can be 60-120 seconds, but the first proof should happen in the
  first 10 seconds: did the new cannon hit harder, did rockets clear drones,
  did shield legs survive the charger?
- Camera should show the mech's body and equipped part doing the work.

### Reward Timing

Observed from guide:

- Quick Fight wins can contribute to supply boxes.
- Boxes have timers or premium acceleration.
- Championship progression unlocks higher-tier item access.
- Rating/league systems create longer-term pressure.

Translation for our game:

- Use immediate salvage/part reward first, not a wait timer.
- If chests/boxes exist later, they should not block first build expression.
- Reward should attach directly to a slot: weapon, legs, module, paint.

## Systems Extraction

### What CATS Is Doing Well

- It makes buildcraft accessible through a physical object.
- It turns short battle results into immediate learning.
- It makes part placement, shape, and counterplay visible.
- It lets casual players feel clever without complex controls.
- It creates strong "try one more build" pressure.

### Why It Is Risky For A Mech Game

- Fully automated battle can weaken the fantasy of piloting a mech.
- Losses can feel unfair if the player cannot intervene.
- Timer boxes and monetized acceleration can dominate session rhythm.
- Difficulty spikes and ad pressure are common player complaints.
- The machine logic is vehicle/contact-based; mechs need more action identity.

## Borrow / Avoid / Copy-Risk

### Borrow

- Build -> instant proof -> reward -> rebuild loop.
- Central object as the home screen.
- Part slots that physically constrain and express build identity.
- Short battle readability.
- Counter-building through enemy roles.
- Testing as a core verb.

### Avoid

- Removing all agency from battle.
- Reward timers as the main progression choke.
- Overusing random boxes for desired builds.
- Letting ads become the expected way to continue.
- Punishing the player for editing before they understand state rules.

### Copy-Risk

- Cat theme, vehicle silhouettes, chassis/attachment visual language, exact
  Quick Fight surface, box/timer economy, league naming, and battle framing.

## Mobile/Web Translation

Recommended adaptation:

1. Use hangar as a 3D build bench with a large mech, not a vehicle garage clone.
2. Keep part slots physical and obvious.
3. After equipping a part, offer a small `Test` or `Battle` that proves it.
4. Keep combat semi-active: auto-target and auto-basics are acceptable, but the
   player should trigger one signature move.
5. Reward deterministic build progress before random boxes.
6. Make failure explainable through enemy role or missing counter, not hidden
   power math.

## Design Decision Draft

CATS should influence build loop structure more than combat control. The game
should borrow its fast "my build worked or failed" clarity, but preserve the
mech-pilot fantasy through at least one active combat decision.

## Current Build Mismatch

No current build exists for `mech-builder-battler`. First mismatch audit should
compare a future capture against this checklist:

- Is the mech the central owned object on the first screen?
- Are part slots visually obvious without opening a dense inventory?
- Does the first battle prove one equipped part quickly?
- Does the player have at least one active intervention in battle?
- Does the reward return to a specific slot or part family?
- Are there no forced timers/ads before the first satisfying loop?

## Source Gaps

- Need current first-session capture from install, emulator, or reliable video.
- Need exact garage -> edit -> quick fight -> reward timing.
- Need current ad/interruption audit.
- Need screenshot board for garage, edit, battle, reward, championship, boxes.
