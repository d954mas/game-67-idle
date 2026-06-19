---
type: Project Art Request Packet
title: Fake Shot Art Request Packet
description: Draft art request packet for three Mech Builder Battler fake shots before final art or implementation.
tags: [project, art-request, fake-shot, visual-direction, mobile, web, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# Fake Shot Art Request Packet

Status: accepted fake-shot direction for concept generation; not final runtime
art.

Purpose: prepare controlled fake-shot generation for the first visual proof of
`Mech Builder Battler`. This packet should be accepted or edited by the lead
before final runtime visual polish. Rough native PC implementation may start
from this accepted direction.

## Inputs

- [Visual reference packet](../references/visual_reference_packet_2026-06-19.md)
- [First slice spec](first_slice_spec_2026-06-19.md)
- [Lead review packet](lead_review_packet_2026-06-19.md)
- [Reference traceability audit](reference_traceability_audit_2026-06-19.md)

## Accepted Target

Accepted on 2026-06-19:

> Landscape-first, PvE-first, one-mech, semi-auto 3D arena battler where the
> first battle gives salvage/resources, the player buys or crafts a first
> module in the hangar, and the next fight proves the change.

Generation can proceed from this target.

## Asset Family

Family: first-slice fake shots.

Reusable kind:

- fake-shot/reference images only;
- not runtime UI;
- not final assets;
- not reusable UI kit;
- not source sheets for slicing.

Expected outputs:

1. Hangar first screen fake shot.
2. Battle moment fake shot.
3. Reward/upgrade return fake shot.

Candidate policy:

- Generate 3-6 candidates per fake shot after lead acceptance.
- Reject candidates before any integration work.
- Select one accepted target per fake shot.
- Record accepted image path, rejected notes, prompt, model/workflow, and
  no-seed or seed reason if generation happens later.

Selected source sheet path:

- None yet; no image has been generated or accepted.

## Global Art Direction

Style:

- stylized 3D mobile action;
- chunky modular hero mech;
- readable silhouettes at phone scale;
- painted metal panels with bevels;
- dark mechanical joints;
- cyan reactor/energy accents;
- orange/amber heat and rocket effects;
- neutral industrial hangar with warm work lights;
- clean game UI safe spaces.

Tone:

- industrial salvage sport;
- premium casual action;
- energetic but not gritty simulation;
- powerful but approachable.

Camera:

- landscape-first fake shots;
- fixed three-quarter/isometric for battle;
- three-quarter hangar view for ownership.

Palette:

- player base: graphite, off-white, steel blue, or muted red;
- energy: cyan/electric blue;
- heat/danger: amber/orange/red;
- rewards: gold/green salvage glow;
- hangar: neutral gray metal with warm light accents.

Avoid:

- dark muddy kitbash;
- tiny realistic greeble noise;
- one-note purple/blue sci-fi palette;
- exact Mech Arena, War Robots, CATS, or Mechangelion silhouettes/UI;
- baked offer popups or monetization surfaces.

## Must-Not-Bake List

Do not bake into fake-shot images:

- final title/logo;
- exact button text beyond temporary simple labels;
- live counters that must change at runtime;
- store/offer/event surfaces;
- debug text;
- random letters or fake text blocks;
- detailed stat rows;
- copyrighted robot silhouettes;
- Mech Arena/War Robots/CATS/Mechangelion UI layout copies;
- final icons intended for runtime reuse;
- exact currency values;
- player names, chats, inbox, clan, tournament, battle pass, or ad labels.

Allowed temporary UI text:

- `Battle`;
- `Equip`;
- `Upgrade`;
- `Test`;
- short placeholder labels such as `Rockets` or `Salvage`.

If text quality is weak, prefer icon-only UI and leave runtime labels for later
composition.

## Fake Shot 1 - Hangar First Screen

Goal:

- Prove ownership, next action, and modular mech fantasy.

Composition:

- Landscape mobile/web frame.
- One large mech occupies 45-60% of safe viewport height.
- Mech stands in calm industrial hangar.
- Shoulder sockets visible, empty, locked, or highlighted.
- Starter arm cannon and balanced legs visible.
- `Battle` is the dominant primary action.
- 3-5 part slot hotspots are visible around the mech.
- One progress/locked hint suggests future upgrade.
- No shop, offers, event banners, inbox, squad roster, or dense inventory.

Prompt draft:

```text
landscape mobile game fake screenshot, stylized 3D mech hangar, one chunky
modular hero mech centered in a clean industrial garage, broad torso, readable
arm cannon, balanced mechanical legs, visible shoulder module sockets, painted
metal panels, dark joints and pistons, cyan reactor glow, warm work lights,
large clean touch-friendly Battle button area, simple part slot hotspots,
premium casual action game, readable at phone scale, no clutter
```

Acceptance:

- Viewer can identify the mech, `Battle` action, and one upgrade hook in five
  seconds.
- The screen feels like a game, not a model viewer.
- The mech silhouette is clear at small size.

Reject if:

- it looks like a realistic simulator bay;
- the UI is a dense PC menu;
- there are multiple squad mechs;
- the mech is too small or hidden by UI.

## Fake Shot 2 - Battle Moment

Goal:

- Prove readable short combat and part-driven action.

Composition:

- Landscape mobile game battle view.
- Fixed three-quarter/isometric small arena.
- Player mech lower center or lower left.
- Drone swarm in upper/mid play space.
- Starter cannon or shoulder rocket burst visible.
- Heat feedback visible through meter, weapon glow, or vent glow.
- Large touch controls/buttons implied in lower corners.
- Top progress/enemy health/wave area readable.
- Effects do not cover mech silhouette.

Prompt draft:

```text
landscape mobile game battle fake screenshot, stylized 3D modular mech in a
small industrial arena, fixed three-quarter camera, player mech firing shoulder
rockets at a swarm of small hovering drones, orange rocket trails and compact
explosions, cyan reactor accents, amber heat glow on vents, readable enemy
silhouettes, large clean touch control areas, top wave progress HUD, premium
casual mech action, clear at phone scale
```

Acceptance:

- Viewer understands who is the player, where enemies are, and what action is
  happening.
- Rockets or starter weapon effect is visibly tied to the mech.
- HUD/control spaces are readable but not cluttered.

Reject if:

- effects hide the mech;
- enemy role is unclear;
- combat reads as a hardcore shooter;
- there are too many buttons or tiny icons.

## Fake Shot 3 - Reward / Upgrade Return

Goal:

- Prove the loop: earned resources -> guided module purchase/craft -> slot
  destination -> visible change -> next battle prompt.

Composition:

- Hangar return or reward overlay.
- Salvage/resources shown as the battle reward.
- Shoulder rockets shown as the accepted guided purchase/craft module.
- Slot destination highlighted on mech shoulders.
- Before/after or installed module visible.
- `Buy`, `Craft`, `Equip`, `Upgrade`, `Test`, or `Battle` next action visible.
- No chest timer, ad multiplier, offer, or random loot wall.

Prompt draft:

```text
landscape mobile game reward upgrade fake screenshot, stylized 3D mech back in
industrial hangar, salvage reward glow leading to a guided shoulder rocket
module purchase option near the mech, highlighted shoulder slot with glowing
socket, before and after upgrade feel, clean Buy Equip and Battle action areas,
cyan and gold upgrade effects, premium casual mobile UI, readable at phone
scale, no shop or ad popups
```

Acceptance:

- Viewer understands resources were earned and spent on a module that has a
  clear slot.
- The upgraded mech looks different from the starter mech.
- The next action is obvious.

Reject if:

- reward is only a number;
- upgrade destination is unclear;
- UI looks like gacha/chest monetization;
- text is unreadable or fake.

## Global Negative Prompt

```text
copyrighted robot design, Mech Arena UI copy, War Robots robot silhouette copy,
CATS cat vehicle, Mechangelion boss copy, Transformers, Gundam, Battletech,
tiny unreadable text, random letters, excessive UI clutter, shop popup, ad
popup, battle pass, event banner, chat, clan, five mech squad, realistic muddy
kitbash, dark low contrast, excessive particles, full-screen bloom, fused UI,
watermark, logo, broken anatomy, extra limbs, illegible controls
```

## Expected Runtime Composition Later

The fake shots are visual targets only. If accepted, runtime implementation
should compose:

- 3D mech model or placeholder model with production-style proportions;
- runtime UI controls as separate elements;
- runtime text rendered by the game;
- reward/upgrade overlays as UI, not baked screenshot text;
- effects rendered or composited separately from core model readability.

## Crops / Slice9 / Transparency

Not applicable for this fake-shot packet.

If later converted into reusable runtime UI assets:

- create a separate UI-kit art request;
- generate blank panels/buttons without labels;
- define slice9 insets and content safe areas;
- generate icons separately;
- reject fused UI.

## QA Rejection Rules

Reject any candidate that has:

- weak mech silhouette;
- unreadable phone-scale composition;
- UI text that looks fake or broken;
- copied reference UI layout or robot silhouette;
- too much service-game clutter;
- monetization surface before first loop;
- effects hiding the mech or enemy;
- no clear next action;
- no visible part/change;
- wrong subject, such as tanks, cars, cats, anime pilots, or generic robots
  without modular mech parts;
- watermarks or logos.

## Review Checklist Before Generation

- [x] Lead accepts landscape-first fake shots.
- [x] Lead accepts one-mech PvE first slice.
- [x] Lead accepts semi-auto arena as fake-shot target.
- [x] Lead accepts resources/salvage as first battle reward, followed by
      hangar purchase/craft.
- [x] Lead accepts shoulder rockets as first purchasable module.
- [x] Lead accepts industrial salvage sport tone.
- [x] Lead accepts this packet as the generation brief.

## Handoff After Generation

If fake shots are generated later, record:

- accepted image path;
- rejected candidates and reason;
- prompt and negative prompt used;
- provider/model/workflow;
- seed or no-seed reason;
- visual review verdict;
- mismatch list against this packet;
- next implementation or rework task.
