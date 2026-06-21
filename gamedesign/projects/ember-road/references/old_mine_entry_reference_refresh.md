---
title: Ember Road Old Mine Entry Reference Refresh
description: Focused reference digest for the next Old Mine visual/UX pass after T0016.
tags: [project, ember-road, references, ux, visual-direction, old-mine]
updated: 2026-06-20
---

# Ember Road Old Mine Entry Reference Refresh

## Purpose

The current T0016 Old Mine choice proves the route and modal behavior, but it is
not the final visual target. It reuses the road backdrop and a generic parchment
choice panel. Before adding dungeon content, the next slice should improve the
entry fantasy and UX against browser-fantasy RPG references.

2026-06-21 update: the T0019 Depth 1 result screenshot is better than the
earlier modal, but the lead still rejects the overall visual/UX match. Treat
this as a reference-readiness problem before adding more mine systems.

## Sources Checked

| Source | Type | Useful for Ember Road | Do not copy |
| --- | --- | --- | --- |
| Legend: Legacy of the Dragons, official site (`https://warofdragons.com/`) | named taste anchor | Dense browser RPG chrome, persistent character/social/game frame, ornate fantasy mood, event/reward density. | Exact UI ornaments, factions, logos, character art, live-service clutter. |
| Dragon Eternity interface handbook (`https://dragoneternity.com/about/interface/`) | official interface documentation | Quest NPC indicators, quest log, completion popups, objective tracking, helper arrows, world-map objective highlights. | Its trading/auction/social complexity and mobile/web-specific chrome. |
| AdventureQuest Worlds screenshots (`https://www.aq.com/embed/spil/agame/screenshots.asp`) | official screenshot gallery | Scene-first fantasy screens where the avatar/NPC/action space is embedded in the world rather than only in detached admin panels. | Cartoon proportions or exact AQW characters/UI. |
| DragonFable screenshots (`https://www.dragonfable.com/Screenshots`) | official screenshot gallery | Quest locations, NPCs, monsters, pets, and equip-focused reward fantasy; good candidate for town/entry/dungeon readability. | Its exact side-view composition or character style. |
| DragonFable mapping page (`https://www.dragonfable.com/Mapping`) | official dungeon/map page | Dungeon entry should imply mapping, navigation, and unknown depth, not just "button opens next screen." | Joke/manual flavor; keep Ember Road serious-fantasy. |
| AdventureQuest screenshots (`https://www.battleon.com/Screenshots`) | official screenshot gallery | Inn/town NPC entry points, class/enemy/reward readability, and clear battle/adventure vignettes. | Single-player battle layout as-is; Ember Road remains map-travel plus automated battle. |
| Shakes and Fidget official site (`https://sfgame.net/`) | adjacent browser RPG visual reference | Strong class identity, compact RPG chrome, tavern/dungeon/equipment loop expectations. | Its parody tone, idle timers, and premium/event clutter. |
| Drakensang Online | spatial dungeon caution reference | Dungeon entry, threat, and loot should feel spatial and location-bound, not only a panel state. | Action-RPG camera/control model; do not turn Ember Road into isometric click combat. |
| Hero Wars / Dragon Awaken / League of Angels | autobattle/result caution references | Reward/result panels, hero power, upgrade/equipment promise, campaign/dungeon progression density. | Gacha/VIP pressure, copied character silhouettes, formation UI, premium clutter. |

## Similar Reference Set

Primary close refs for Ember Road:

- Legend: Legacy of the Dragons: strongest taste anchor for ornate browser RPG density.
- Dragon Eternity: best functional reference for quest/objective guidance and world-map highlights.
- DragonFable: best adjacent reference for readable quest locations, NPCs, monsters, and dungeon framing.
- AdventureQuest / AdventureQuest Worlds: useful for scene-first fantasy screens and clear avatar/NPC affordances.

Secondary / caution refs:

- Shakes and Fidget: useful for class/equipment/dungeon loop clarity, but tonal mismatch.
- Drakensang Online: useful as "fantasy dungeon entry should feel spatial", but too action-RPG/isometric for this UI layer.
- Wartune / Hero Wars / Dragon Awaken / League of Angels: useful for
  reward/result/progression density, but only as caution refs. Ember Road must
  not inherit live-service clutter, gacha pressure, or a copied party-formation
  screen.

## Current T0016 Mismatch

- Old Mine uses the North Road landscape, so the location title changes faster
  than the actual place fantasy.
- The choice surface is functional, but it reads as a modal pasted over a road,
  not as an entrance threshold with danger, lock, depth, and reward promise.
- The right quest rail stays useful but is still dense; future screens should
  reduce duplicated actions when the main choice panel is active.
- Route OPEN is readable, but the mine plaque should become a stronger fantasy
  affordance: gate, cave mouth, torch, skull marker, or depth icon.

## T0017 Update

- `build/captures/ember-road/state_modal_or_choice_open.png` now shows a
  dedicated Old Mine entrance backdrop: cave mouth, timber supports, torchlight,
  forest road foreground, and mountain distance.
- The choice surface moved into the right quest rail, so the cave entrance and
  hero remain visible in the main scene.
- Remaining mismatch: the mine is still an entry/choice screen only. It does
  not yet prove a dungeon interior, encounter, resource node, or depth floor.

## T0018 Lead Rejection

The 2026-06-20 lead feedback rejects the current visual and UX direction even
after the dedicated backdrop. Treat the T0017 product PASS as technical
evidence only, not as accepted art direction.

Use the newer rejection digest before any Old Mine implementation:

`gamedesign/projects/ember-road/references/visual_ux_rejection_reference_digest.md`

Current blockers:

- `SCOUT` is player-facing production scaffolding because it is labeled
  `NEXT SLICE`.
- The screen still behaves like a modal/rail choice, not a real RPG route or
  scout decision.
- The player cannot see threat, resource, depth, reward, or result feedback.
- The route strip is decorative rather than a functional map/destination UX.
- Y-up remains mandatory for game/world/UI layout; do not use visual rework as
  an excuse to flip layout semantics.

## T0019 Depth Result Mismatch

Current screenshot:
`build/captures/ember-road/state_old_mine_depth_encounter.png`

- Better: the scene is now a dedicated mine entrance, the route strip changes
  state, and the bottom report log shows reward feedback.
- Still wrong: the main playable meaning is concentrated in the right rail and
  a bottom text line. The mine entrance itself does not yet carry enough
  encounter, depth, danger, resource, or route-decision information.
- Still wrong: `CLEARED` reads like a closed state rather than a new RPG
  choice. A player can understand what happened, but not what the next mine
  loop is.
- Still wrong: the Cave Bat result is not visual enough. Browser RPG references
  put enemy, reward, quest marker, inventory/status, or destination meaning
  into visible game surfaces, not only prose.
- Still wrong: the route strip is improving, but it remains a set of labels
  instead of a destination grammar with current node, next depth, lock/reason,
  and reward/resource promise.
- Still wrong: right-side chrome dominates the interaction. The scene should
  feel like the threshold where the hero chooses to enter, scout, clear, delve,
  or return.
- Hard rule: all future layout notes, `UiBox` placement, route/depth ordering,
  and DevAPI logical rects remain Y-up. Any Y-down value may exist only in a
  named render/input/screenshot/DevAPI boundary conversion.

## Stronger Reference Translation

Use these roles before the next Old Mine runtime or fake-shot pass:

| Role | Reference | Translation For Old Mine |
| --- | --- | --- |
| Primary taste anchor | Legend: Legacy of the Dragons / War of Dragons | Dense fantasy browser-RPG frame, painted location, side location/object/NPC lists, map/hunt readability. Translate as "mine is a playable location with objects and threats", not as copied chrome. |
| Quest guidance | Dragon Eternity interface handbook | Quest markers, objective tracking, helper arrows, world-map highlights, and completion feedback. Translate as "the mine result says what changed and what target is next." |
| Quest-location readability | DragonFable / AdventureQuest | NPC, monster, reward, and quest entry are visually immediate. Translate as "Cave Bat/resource/depth should be visible in the mine surface." |
| Scene-first world UI | AdventureQuest Worlds | Avatar/NPC/action affordances live over an illustrated place. Translate as "the mine threshold must remain the first read while controls support it." |
| Compact RPG promise | Shakes & Fidget | Hero identity, tavern/adventure/dungeon/equipment promise are visible early. Translate as "hero status, reward, and next dungeon promise stay compact and readable." |
| Spatial dungeon caution | Drakensang Online | Dungeon entrance feels like a place and a threat. Translate only the spatial promise; avoid action-RPG controls/camera. |
| Autobattle/result caution | Hero Wars / Dragon Awaken / League of Angels | Result/reward/progression panels are legible. Borrow only reward clarity; avoid gacha/formation/VIP clutter. |

## Next Visual Target

The next Old Mine target should show:

- A dedicated cave/mine entrance backdrop with a clear threshold in the world.
- Hero near the entrance, facing the danger/source of choice, not standing in a
  generic road scene.
- A route plaque or map marker that reads as "unlocked destination", not only a
  small status label.
- One main choice panel with two or three actions: enter/scout/back.
- Scout locked/future state shown as a deliberate lock, not a disabled mistake.
- Quest/progression rail simplified when the modal is open, so the player reads
  the decision first.
- A visible first repeatable promise if the next slice is a grind loop:
  `Depth 1 cleared -> delve cache / next depth / return`, with reward and
  route meaning visible before the click.
- If a result state is shown, enemy/resource/reward should have a visual
  anchor in the scene or route UI, not only in prose.

## Implementation Rules For Next Slice

- Keep all game/world/UI layout Y-up; convert Y-down only at renderer/input/
  screenshot/DevAPI boundaries.
- Treat Y-up as an invariant, not a preference. Do not store screen-space
  Y-down coordinates as layout truth.
- Do not add full dungeon systems until the Old Mine entry target passes a
  fresh screenshot-vs-reference gate.
- Use legal project-local assets for any dedicated mine backdrop or UI art.
- Keep the first Old Mine slice narrow: entry backdrop, one locked scout/enter
  preview, or one encounter. No inventory/economy expansion in the same slice.

## Next Proof After This Refresh

Do not proceed directly to a broad repeatable dungeon. Pick one of these:

1. Revised fake shot: `Old Mine depth result -> next delve choice`, with visible
   threat/resource/reward/depth grammar and Y-up layout notes.
2. Narrow runtime rewrite: keep the existing mine asset, but make the scene and
   route strip carry the next action (`DELVE`, `RETURN`, `NEXT DEPTH LOCKED`)
   instead of relying on the right rail alone.

The proof screenshot should be named before coding. Candidate:
`build/captures/ember-road/state_old_mine_next_delve_choice.png`.
