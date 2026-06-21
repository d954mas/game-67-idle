---
type: Project Source Notes
title: Ember Road Fantasy Browser RPG UX Reference Set
description: Adjacent visual and UX references for Ember Road after the first visual rejection.
tags: [project, ember-road, references, ux, visual-direction]
timestamp: 2026-06-20T00:00:00Z
---

# Ember Road Fantasy Browser RPG UX Reference Set

Scope: reset the first-screen UX and visual target after the lead rejected the
current native screen as not matching the desired game, even at the UX level.
This note is project-specific. It supports Ember Road's next fake shot /
direction board, not reusable knowledge.

## Reference Question

What should Ember Road borrow from old and modern fantasy browser RPGs so the
first native screen reads as a fantasy hero RPG with town travel, quests, items,
progression, and automated battles, instead of a debug panel layout?

## Study Mode

Central deconstruction, still incomplete for final art. The sources below are
enough to reject the current screen grammar and choose a better fake-shot
direction. They are not enough for final economy, battle pacing, monetization,
or exact UI copying.

Updated central deconstruction and current fake-shot packet live in:

- `gamedesign/projects/ember-road/references/fantasy_browser_rpg_central_deconstruction.md`
- `gamedesign/projects/ember-road/art_requests/ember-road-old-gate-fakeshot-v001.json`
- `gamedesign/projects/ember-road/art/ember-road-old-gate-fakeshot-v001.png`

## Source Matrix

| Source | Link/Path | Quality | Checked | Proves | Does Not Prove |
|---|---|---|---|---|---|
| Lead rejection | current chat, 2026-06-20 | user-provided | 2026-06-20 | Current visual and UX are not the desired target | Exact corrected layout or art style |
| Current Ember Road capture | `build/captures/iterate.png` | current build capture | 2026-06-20 | Native screen is block-art/debug-style, with separated town panel and map panel | What the accepted target should be |
| Legend: Legacy of the Dragons / Xsolla Mall | https://x.la/g/legend-legacy-of-the-dragons | store/listing screenshots | 2026-06-20 | The named reference is a browser fantasy MMORPG with quests, locations, items, resources, clans/party, and animated asynchronous combat claims; page exposes multiple gameplay images | First-minute input timing or current live flow |
| Legend: Legacy of the Dragons / RAWG | https://rawg.io/games/legend-legacy-of-the-dragons | database page with screenshots | 2026-06-20 | Inventory/status density: equipment, item grid, currencies, tabs, chat/social surfaces | Exact current UI or onboarding |
| Dragon Eternity interface handbook | https://dragoneternity.com/about/interface/ | official handbook | 2026-06-20 | Quest NPC indicators, quest log, progress popups, objective tracking, helper arrows, world-map highlight | Visual beauty target; first-session pacing |
| Dragon Eternity / NewRPG | https://newrpg.com/browser-games/dragon-eternity/ | listing with screenshot and loop summary | 2026-06-20 | New quests, battle availability icon, reward collection, backpack with character plus items | Official current state or exact first-input timing |
| Shakes & Fidget / Steam | https://store.steampowered.com/app/438040/Shakes_and_Fidget/ | official store page | 2026-06-20 | Hero creation, accepted quests, adventures, monsters, loot, equipment, dungeons; hand-drawn/cartoon fantasy positioning | Exact in-game first screen from text alone |
| Shakes & Fidget / official site | https://sfgame.net/ | official visual/feature site | 2026-06-20 | Character classes and fantasy battle identity; official image-heavy art language | Quest timing and screen sequence |
| AdventureQuest Worlds screenshots | https://www.aq.com/embed/spil/agame/screenshots.asp | official screenshot page | 2026-06-20 | Town hub as an illustrated play scene with avatars and action/menu buttons layered over the world | Single-player Ember Road layout; automation |
| DragonFable design note screenshot | https://www.dragonfable.com/gamedesignnotes/Winter-Cleaning-5490 | official page/image result, fetch unstable | 2026-06-20 | NPC dialogue plus quest-button grid can make quest choice feel diegetic and immediate | Current page could not be fetched by browser cache; use as visual-only lead until rechecked |
| Wartune Reborn / Xsolla Mall | https://x.la/g/wartune-reborn | store/listing screenshots | 2026-06-20 | Dense RPG HUD with quest tracking, skill/progression windows, city backdrop, map/campaign access, bottom icon belt | Ember Road should not copy MMO clutter or strategy/castle systems |

## Similar Reference Roles

| Reference | Use For Ember Road | Avoid |
|---|---|---|
| Legend: Legacy of the Dragons | Primary taste anchor: ornate fantasy chrome, painted locations, dense character/inventory surfaces, side-location lists, chat/log/status density | Copying exact layout, factions, names, icon shapes, monetization, or clutter for its own sake |
| Dragon Eternity | Strongest UX correction: quest tracking, NPC indicators, objective arrows, map highlight, visible quest progress and reward popups | Overbuilding social/trade/auction systems |
| Shakes & Fidget | Town hub as one illustrated place with clickable actions, hero identity, tavern/quest energy, bottom status/reward bar | Satirical/comedy tone and idle/waiting pressure |
| AdventureQuest Worlds | World-first scene composition: avatar/NPC/action buttons over an actual location, not separated admin panels | MMO crowding, chat dependence, active skill combat |
| DragonFable | NPC portrait/dialogue plus large quest buttons; direct quest choice and story path clarity | Flash-era oversized menu stacks as final UX |
| Wartune Reborn | Dense fantasy HUD, quest tracker, bottom icon belt, map/campaign entry, progression windows | Too many daily/event/VIP buttons and strategy-city scope |

## Observed / Supported UX Facts

- `observed` - The current Ember Road screen is split into a large left debug
  box and a detached right map box. It does not present one believable place.
- `observed` - Legend screenshots use painted location art or parchment/status
  panels inside ornate chrome, with navigation and status surfaces around a
  persistent RPG frame.
- `observed` - Legend inventory/status screenshots support dense character
  management: gear slots, tabs, item grid, currencies, chat/log, and status
  lists can coexist on one RPG screen.
- `observed` - Dragon Eternity documentation explicitly ties quests to NPC
  markers, quest log entries, progress/completion popups, objective tracking,
  helper arrows, and world-map highlights.
- `observed` - Dragon Eternity listing describes a loop where new quests appear,
  a sword icon signals available battle, winning yields items, and backpack
  shows character plus inventory.
- `observed` - AQW screenshots show fantasy town/hub UI layered over an
  illustrated location with avatars and action buttons, not over separate flat
  panels.
- `secondary` - Shakes & Fidget store copy supports hero creation, quests,
  monsters, loot, equipment, and dungeons, and is useful as a town-hub/action
  menu reference, but its comedy tone is wrong for Ember Road.
- `secondary` - Wartune Reborn screenshots support dense quest/progression HUD
  vocabulary, but its event/VIP clutter should be treated as a warning.

## Current-Build Mismatch

Current screenshot: `build/captures/iterate.png`

- The screen reads as debug UI: flat rectangles, no painted location, no
  character art, no NPC portrait, no item art, no fantasy frame language.
- The UX is wrong, not just the art. The main screen is an admin split between
  a "town box" and a "map box"; references treat the hub as a place with
  clickable destinations, NPCs, quest indicators, and persistent RPG status.
- Quest flow is too textual. The first action is a button under the scene,
  instead of a quest/NPC target inside the place with visible reward/progress.
- Map travel is too abstract. The map is a separate beige panel with labels,
  not a painted route/destination surface tied to the town and current quest.
- Hero progression is too thin. References show hero identity, equipment,
  resources, quest log, item grid, and feedback surfaces; Ember Road shows only
  minimal top stats and a small block avatar.
- Automated battle is not visually promised on the first screen. The target
  needs a route/enemy/reward preview so "travel to fight, then loot/equip" is
  visible before the click.

## Revised First-Screen UX Target

The next fake shot should not be a blank UI kit sheet and should not start from
the current two-panel layout. It should be a full composed target screen:

1. One illustrated town hub: Old Gate as a painted location, with hero and Gate
   Warden visible in the scene.
2. Persistent RPG frame: top hero resources/status, bottom log/action belt,
   and a right-side quest/location rail integrated into the frame.
3. Quest/NPC focus: Gate Warden portrait or scene marker, quest title,
   objective, reward preview, and primary action in one connected surface.
4. Map as travel fantasy: a parchment/painted route strip or mini-map showing
   Old Gate -> North Road -> locked Old Mine, with quest marker and lock reason.
5. Progression proof: small equipment/loot preview, XP/gold gain promise, and
   the ring upgrade visible as the first reward idea.
6. Battle promise: Road Wolf marker or encounter card on the route, with
   "auto battle" communicated by combat preview/result slot, not a raw log.

## Borrow / Avoid / Copy-Risk

- Borrow: ornate RPG frame, illustrated town as the main surface, NPC quest
  target, right-side destination/quest rail, map highlights, objective arrows or
  markers, reward preview, inventory/equipment density, bottom log/action belt.
- Avoid: debug rectangles, pure admin dashboard layout, detached map panel,
  empty UI-kit generation before a composed target, excessive MMO daily/event
  clutter, waiting/idle pressure, social/auction/trade systems in the first
  slice.
- Copy-risk: exact Legend/AQW/Dragon Eternity layouts, icons, names, faction
  symbols, screenshots, character art, UI ornament shapes, and monetization
  surfaces.

## Next Proof

Create a composed direction board or fake shot for:

`Old Gate first screen -> accept wolf quest -> North Road auto-battle preview`

Required proof elements:

- full-screen town hub composition;
- Gate Warden/NPC quest focus;
- hero identity/status;
- route/map strip with North Road and locked Old Mine;
- reward preview with XP/gold/ring;
- one battle/result preview surface;
- current screenshot mismatch list updated against `build/captures/iterate.png`.

No final runtime art or UI-kit slicing should proceed until this target is
accepted or the lead approves a narrower exception.
